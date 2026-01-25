#pragma once

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <utility>
#include <mutex>
#include <box2d/box2d.h>
#include <vector>
#include <bitset>
#include <map>
#include <tuple>
#include <cmath>

#include "Array2D.hpp"
#include "Commons.hpp"
#include "Logging.hpp"
#include "ThreadPool.hpp"
#include "GlobalAtomics.hpp"
#include "Camera.hpp"

// Douglas-Peucker simplification threshold 
static constexpr f32 SIMPLIFICATION_EPSILON = 0.0001f;

// Water spreading configuration
u32 WATER_MAX_DIST = 10;
u32 WATER_SPREAD_FALLOFF = 1;

static inline u32 fast_rand() {
    static u32 seed = 0x12345678u;
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}

enum class ParticleID : u8 {
    AIR = 0,
    STONE,
    SAND,
    WATER,
    WOOD,
};

static const std::array<SDL_FColor, 5> particle_colors = {{
    {0.0f, 0.0f, 0.0f, 1.0f}, // AIR
    {0.5f, 0.5f, 0.5f, 1.0f}, // STONE
    {255.0f/255.0f, 215.0f/255.0f, 0.0f/255.0f, 1.0f}, // SAND
    {0.0f, 0.0f, 1.0f, 1.0f}, // WATER
    {139.0f/255.0f, 69.0f/255.0f, 19.0f/255.0f, 1.0f}, // WOOD
}};

static std::array<u32, 5> particle_colors_u32;

struct Particle {
    ParticleID id;    // Material type (u8)
    u8 body_id;       // 0 = terrain/free, 1-255 = rigidbody ID
    u16 lifetime;     // Top bit = settled flag, lower 15 bits = lifetime in ms
    
    static constexpr u16 SETTLED_FLAG = 0b1000000000000000;
    static constexpr u16 LIFETIME_MASK = 0b0111111111111111;

    // Settled flag accessors (uses top bit of lifetime)
    bool is_settled() const { return (lifetime & SETTLED_FLAG) != 0; }
    void set_settled(bool v) { lifetime = v ? (lifetime | SETTLED_FLAG) : (lifetime & LIFETIME_MASK); }
    u16 get_lifetime() const { return lifetime & LIFETIME_MASK; }
    void set_lifetime(u16 v) { lifetime = (lifetime & SETTLED_FLAG) | (v & LIFETIME_MASK); }
};

template <u32 WIDTH, u32 HEIGHT, u32 CHUNK_WIDTH = 64, u32 CHUNK_HEIGHT = 64>
class SandWorld {
public:
    SandWorld() {
        clear();

        const SDL_PixelFormatDetails* details = SDL_GetPixelFormatDetails(SDL_PIXELFORMAT_RGBA8888);
        for (u64 i = 0; i < particle_colors.size(); ++i) {
            const auto& c = particle_colors[i];
            particle_colors_u32[i] = SDL_MapRGBA(details, nullptr, static_cast<u8>(c.r * 255), static_cast<u8>(c.g * 255), static_cast<u8>(c.b * 255), static_cast<u8>(c.a * 255));
        }
    }
    
    // terrain mesh pixels
    bool is_static_solid(i32 x, i32 y) const {
        if (x < 0 || x >= (i32)width() || y < 0 || y >= (i32)height()) {
            return false;
        } 

        const auto& p = m_particles((u32)x, (u32)y);
        switch (p.id) {
            case ParticleID::STONE:
            case ParticleID::SAND:
                return true;
            default:
                return false;
        }
    }
    
    bool is_chunk_dirty(u32 chunk_x, u32 chunk_y) const {
        if (chunk_x >= WIDTH || chunk_y >= HEIGHT) {
            return false;
        }

        return m_dirty_chunks(chunk_x, chunk_y);
    }
    
    void mark_chunk_dirty(u32 x_pixel, u32 y_pixel) {
        // absoulte chunk coords
        const u32 cx = x_pixel / CHUNK_WIDTH;
        const u32 cy = y_pixel / CHUNK_HEIGHT;

        if (cx < WIDTH && cy < HEIGHT) {
            m_dirty_chunks.set(cx, cy);
            
            // pixel coords relative to chunk
            const u32 lx = x_pixel % CHUNK_WIDTH;
            const u32 ly = y_pixel % CHUNK_HEIGHT;
            
            // mark neighbors if on edge
            if (lx == 0 && cx > 0) { // left edge
                m_dirty_chunks.set(cx - 1, cy);
            }
            if (lx == CHUNK_WIDTH - 1 && cx < WIDTH - 1) { // right edge
                m_dirty_chunks.set(cx + 1, cy);
            }
            if (ly == 0 && cy > 0) { // top edge
                m_dirty_chunks.set(cx, cy - 1);
            }
            if (ly == CHUNK_HEIGHT - 1 && cy < HEIGHT - 1) { // bottom edge
                m_dirty_chunks.set(cx, cy + 1);
            }
        }
    }

    void update_sand(const u32 x, const u32 y) {
        static constexpr std::pair<i32, i32> dirs[] = {
            {0, 1},  // down
            {-1, 1}, // down-left
            {1, 1}   // down-right
        };

        for (auto [dx, dy] : dirs) {
            const u32 nx = x + dx;
            const u32 ny = y + dy;

            if (m_particles(nx, ny).id == ParticleID::AIR) {
                m_particles(nx, ny).id = ParticleID::SAND;
                m_particles(x, y).id = ParticleID::AIR;

                m_updated_particles.set(nx, ny);

                mark_chunk_dirty(nx, ny);
                mark_chunk_dirty(x, y);
                return;
            } else if (m_particles(nx, ny).id == ParticleID::WATER) {
                m_particles(nx, ny).id = ParticleID::SAND;
                m_particles(x, y).id = ParticleID::WATER;

                m_updated_particles.set(nx, ny);

                mark_chunk_dirty(nx, ny);
                mark_chunk_dirty(x, y);
                
                update_water(x, y); // expensive but, prevents water climbing up
                return;
            }
        }
    }

    void update_water(const u32 x, const u32 y) {
        // straight down
        if (m_particles(x, y + 1).id == ParticleID::AIR) {
            m_particles(x, y + 1).id = ParticleID::WATER;
            m_particles(x, y).id = ParticleID::AIR;
            m_updated_particles.set(x, y + 1);
            mark_chunk_dirty(x, y + 1);
            mark_chunk_dirty(x, y);
            return;
        }

        auto try_spread = [&](bool left) -> bool {
            u32 cur_x = x;
            u32 cur_y = y;
            constexpr u32 max_x = WIDTH * CHUNK_WIDTH - 1;
            constexpr u32 max_y = HEIGHT * CHUNK_HEIGHT - 1;
            
            for (u32 step = 1; step <= WATER_MAX_DIST; ++step) {
                // probability falloff: the further we spread, the less likely to continue
                if (step > 1 && (fast_rand() % WATER_SPREAD_FALLOFF) >= (WATER_MAX_DIST + 1 - step)) {
                    break;
                }
                
                const u32 next_x = left ? cur_x - 1 : cur_x + 1;
                const u32 next_y = cur_y + 1;
                
                // Bounds check (stone border guarantees x >= 1)
                if (next_x < 1 || next_x >= max_x) {
                    break;
                }
                
                // diagonal-down
                if (next_y < max_y && m_particles(next_x, next_y).id == ParticleID::AIR) {
                    cur_x = next_x;
                    cur_y = next_y;
                    continue;
                }
                
                // horizontal
                if (m_particles(next_x, cur_y).id == ParticleID::AIR) {
                    cur_x = next_x;
                    continue;
                }
                
                break;
            }
            
            if (cur_x != x || cur_y != y) {
                m_particles(cur_x, cur_y).id = ParticleID::WATER;
                m_particles(x, y).id = ParticleID::AIR;

                m_updated_particles.set(cur_x, cur_y);

                mark_chunk_dirty(cur_x, cur_y);
                mark_chunk_dirty(x, y);

                return true;
            }
            return false;
        };

        const bool go_left = fast_rand() & 1;

        if (try_spread(go_left)) {
            return;
        }
        
        // Try opposite direction if primary blocked
        try_spread(!go_left);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // FIXME: THE SET OF FUNCTIONS BELOW IS HIGHLY INEFFICIENT
    // Also, I suspect there are still bugs in it :/
    
    struct ChunkCache {
        // a chain is simply a list of vertices that form a polygon
        std::vector<std::vector<b2Vec2>> chains;
        bool populated = false;
    };
    
    Array2D<ChunkCache, WIDTH, HEIGHT> m_chunk_cache;
    std::mutex m_cache_mutex;

    std::vector<std::vector<b2Vec2>> mesh_world_parallel() {
        std::vector<std::vector<b2Vec2>> all_chains;
        std::vector<std::pair<u32, u32>> dirty_indices;

        for (u32 cy = 0; cy < HEIGHT; ++cy) {
            for (u32 cx = 0; cx < WIDTH; ++cx) {
                if (m_dirty_chunks(cx, cy) || !m_chunk_cache(cx, cy).populated) {
                    dirty_indices.push_back({cx, cy});
                }
            }
        }
        
        if (!dirty_indices.empty()) {
            for (const auto& [cx, cy] : dirty_indices) {
                m_thread_pool.enqueue([this, cx, cy] {
                    auto chains = mesh_chunk(cx, cy);
                    
                    std::lock_guard<std::mutex> lock(m_cache_mutex);
                    m_chunk_cache(cx, cy).chains = std::move(chains);
                    m_chunk_cache(cx, cy).populated = true;
                });
            }
            m_thread_pool.wait_all();
        }
        
        for (const auto& cache : m_chunk_cache) {
            all_chains.insert(all_chains.end(), cache.chains.begin(), cache.chains.end());
        }
        
        return all_chains;
    }
    
    struct Segment {
        b2Vec2 p1, p2;
    };

    // generates a list of line segments that form the boundaries of solid particles in a chunk
    std::vector<std::vector<b2Vec2>> mesh_chunk(u32 cx, u32 cy) {
        std::vector<Segment> segments;
        constexpr f32 scale = 1.0f / PIXELS_PER_METER;
        
        const u32 start_x = cx * CHUNK_WIDTH;
        const u32 start_y = cy * CHUNK_HEIGHT;
        const u32 end_x = start_x + CHUNK_WIDTH;
        const u32 end_y = start_y + CHUNK_HEIGHT;
        
        for (u32 y = start_y; y < end_y; ++y) {
            for (u32 x = start_x; x < end_x; ++x) {
                // cast to i32 for boundary checks
                const i32 world_x = x;
                const i32 world_y = y;
                
                // solid pixels generate boundaries
                if (!is_static_solid(world_x, world_y)) {
                    continue;
                }
                
                // check 4 GLOBAL neighbors. If Air/Bound, add edge.
                
                // coords in meters
                const f32 x0 = world_x * scale;
                const f32 x1 = (world_x + 1) * scale;
                const f32 y0 = world_y * scale;
                const f32 y1 = (world_y + 1) * scale;
                
                // top
                if (!is_static_solid(world_x, world_y - 1)) {
                    segments.push_back({{x1, y0}, {x0, y0}});
                }
                // bottom
                if (!is_static_solid(world_x, world_y + 1)) {
                    segments.push_back({{x0, y1}, {x1, y1}});
                }
                // left
                if (!is_static_solid(world_x - 1, world_y)) {
                    segments.push_back({{x0, y0}, {x0, y1}});
                }
                // right
                if (!is_static_solid(world_x + 1, world_y)) {
                    segments.push_back({{x1, y1}, {x1, y0}});
                }
            }
        }
        
        return stitch_segments(segments);
    }

    // stitches line segments into continuous chains (polygons)
    std::vector<std::vector<b2Vec2>> stitch_segments(const std::vector<Segment>& segments) {
        if (segments.empty()) {
            return {};
        }

        // int keys for stitching (avoid float precision issues)
        struct PointKey {
            i32 x, y;
            bool operator<(const PointKey& o) const { return (x < o.x) || (x == o.x && y < o.y); }
        };
        
        auto to_key = [](b2Vec2 v) -> PointKey {
            return { static_cast<i32>(std::round(v.x * PIXELS_PER_METER)), 
                     static_cast<i32>(std::round(v.y * PIXELS_PER_METER)) };
        };
        
        // adjacent if they share a vertex (same coords)
        std::map<PointKey, std::vector<i32>> adj;
        for(i32 i = 0; i < segments.size(); ++i) {
            adj[to_key(segments[i].p1)].push_back(i);
        }
        
        std::vector<bool> used(segments.size(), false);
        std::vector<std::vector<b2Vec2>> chains;
        
        // stitch segments into chains
        for(i32 i = 0; i < segments.size(); ++i) {
            if (used[i]) {
                continue;
            }
            
            std::vector<b2Vec2> chain;
            
            chain.push_back(segments[i].p1);
            chain.push_back(segments[i].p2);

            used[i] = true;
            
            b2Vec2 tip = segments[i].p2;
            
            // greedily extend chain
            while(true) {
                auto it = adj.find(to_key(tip));
                if (it == adj.end() || it->second.empty()) {
                    break;
                }
                
                i32 next_idx = -1;
                for(i32 s_idx : it->second) {
                    if (!used[s_idx]) {
                        next_idx = s_idx;
                        break;
                    }
                }
                
                if (next_idx != -1) {
                    used[next_idx] = true;
                    
                    // add new point
                    tip = segments[next_idx].p2;
                    chain.push_back(tip);
                } else {
                    break;
                }
            }
            
            if (chain.size() > 1) {
                std::vector<b2Vec2> sim = simplify_collinear(chain, SIMPLIFICATION_EPSILON);
                if (sim.size() > 1) {
                    chains.push_back(sim);
                }
            }
        }

        return chains;
    }
    
    // remove points that are collinear
    static std::vector<b2Vec2> simplify_collinear(const std::vector<b2Vec2>& points, f32 epsilon) {
        if (points.size() < 3) {
            return points;
        }
        
        std::vector<b2Vec2> result;
        result.push_back(points[0]);
        
        for (u64 i = 1; i < points.size() - 1; ++i) {
            const b2Vec2& prev = result.back();
            const b2Vec2& curr = points[i];
            const b2Vec2& next = points[i + 1];
            
            const f32 dx1 = curr.x - prev.x;
            const f32 dy1 = curr.y - prev.y;
            const f32 dx2 = next.x - curr.x;
            const f32 dy2 = next.y - curr.y;
            
            const f32 cross = dx1 * dy2 - dy1 * dx2;
            const f32 dot = dx1 * dx2 + dy1 * dy2;
            
            // if cross product is close to 0, points are collinear
            // if dot product is positive, points are in the same direction
            if (std::abs(cross) < epsilon && dot > 0) {
                continue;
            }
            
            result.push_back(curr);
        }
        
        result.push_back(points.back());
        return result;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////

    void update_chunk(const u32 chunk_x, const u32 chunk_y) {
        // bottom left corner of the chunk
        const u32 x_start = chunk_x * CHUNK_WIDTH;
        const u32 y_start = (chunk_y + 1) * CHUNK_HEIGHT - 1;

        const bool flip_x = fast_rand() & 1;

        for (u32 i = 0; i < CHUNK_WIDTH; ++i) {
            const u32 x = flip_x 
            ? (x_start + CHUNK_WIDTH - 1 - i)  // right-to-left
            : (x_start + i);                    // left-to-right
            
            for (u32 j = 0; j < CHUNK_HEIGHT; ++j) {
                // const u32 x = x_start + i;
                const u32 y = y_start - j;

                if (m_updated_particles(x, y)) {
                    continue;
                }

                switch (m_particles(x, y).id) {
                    case ParticleID::AIR: break;
                    case ParticleID::STONE: break;
                    case ParticleID::SAND: update_sand(x, y); break;
                    case ParticleID::WATER: update_water(x, y); break;

                    default: break;
                }
            }
        }
    }
    void update() {
        g_sim_step_count++;
        m_updated_particles.clear();
        m_dirty_chunks.fill(); 

        const bool flip_chunks_x = g_sim_step_count & 1; // every 1
        const bool flip_chunks_y = (g_sim_step_count >> 1) & 1; // every 2

        for (u32 phase_y = 0; phase_y < 2; ++phase_y) {
            for (u32 phase_x = 0; phase_x < 2; ++phase_x) {
                if (flip_chunks_y) { // T-B
                    for (u32 chunk_y = phase_y; chunk_y < HEIGHT; chunk_y += 2) {
                        enqueue_row(chunk_y, phase_x, flip_chunks_x);
                    }
                } else { // B-T
                    for (i32 chunk_y = HEIGHT - 1 - phase_y; chunk_y >= 0; chunk_y -= 2) {
                        enqueue_row(chunk_y, phase_x, flip_chunks_x);
                    }
                }
                m_thread_pool.wait_all();
            }
        }
    }

    void enqueue_row(i32 chunk_y, u32 phase_x, bool flip_x) {
        if (flip_x) { // R-L
            for (i32 chunk_x = WIDTH - 1 - phase_x; chunk_x >= 0; chunk_x -= 2) {
                m_thread_pool.enqueue([this, chunk_x, chunk_y] { update_chunk(chunk_x, chunk_y); });
            }
        } else { // L-R
            for (u32 chunk_x = phase_x; chunk_x < WIDTH; chunk_x += 2) {
                m_thread_pool.enqueue([this, chunk_x, chunk_y] { update_chunk(chunk_x, chunk_y); });
            }
        }
    }

    // Write directly to GPU texture buffer
    void renderToTexture(SDL_Texture* texture) {
        void* pixels = nullptr;
        i32 pitch_bytes = 0;

        if (!SDL_LockTexture(texture, nullptr, &pixels, &pitch_bytes)) {
            Logging::log_warning("Failed to lock texture: ", SDL_GetError());
            return;
        }

        const u32 width = WIDTH * CHUNK_WIDTH;
        const u32 height = HEIGHT * CHUNK_HEIGHT;
        const u32 pitch = pitch_bytes / sizeof(u32);

        u32* dst = static_cast<u32*>(pixels);

        const u32 workers = m_thread_pool.thread_count();
        const u32 rows_per_task = (height + workers - 1) / workers;

        for (u32 i = 0; i < workers; ++i) {
            const u32 y_start = i * rows_per_task;
            const u32 y_end = std::min(y_start + rows_per_task, height);

            if (y_start >= y_end) {
                break;
            }

            m_thread_pool.enqueue([=, this] {
                for (u32 y = y_start; y < y_end; ++y) {
                    u32* row = dst + y * width;

                    for (u32 x = 0; x < width; ++x) {
                        // TODO: maybe add some variation based on coords?
                        row[x] = particle_colors_u32[
                            static_cast<u32>(m_particles(x, y).id)
                        ];
                    }
                }
            });
        }

        m_thread_pool.wait_all();
        SDL_UnlockTexture(texture);
    }

    u32 width() const { return WIDTH * CHUNK_WIDTH; }
    u32 height() const { return HEIGHT * CHUNK_HEIGHT; }

    void setParticle(u32 x, u32 y, ParticleID id) {
        // avoid overwriting the stone border
        if (x > 0 && x < width() - 1 && y > 0 && y < height() - 1) {
            m_particles(x, y).id = id;
            mark_chunk_dirty(x, y);
        }
    }

    const Particle& getParticle(u32 x, u32 y) const {
        return m_particles(x, y);
    }
    
    Particle& getParticleMut(u32 x, u32 y) {
        return m_particles(x, y);
    }

    void clear() {
        m_particles.fill({ParticleID::AIR, 0, 0});  // id, body_id, lifetime

        // stone border
        for (u32 i = 0; i < WIDTH * CHUNK_WIDTH; ++i) {
            m_particles(i, HEIGHT * CHUNK_HEIGHT - 1).id = ParticleID::STONE;
            m_particles(i, 0).id = ParticleID::STONE;
        }
        for (u32 i = 0; i < HEIGHT * CHUNK_HEIGHT; ++i) {
            m_particles(WIDTH * CHUNK_WIDTH - 1, i).id = ParticleID::STONE;
            m_particles(0, i).id = ParticleID::STONE;
        }
        
        for (auto& cache : m_chunk_cache) {
            cache.populated = false;
            cache.chains.clear();
        }
        m_dirty_chunks.fill();
        m_updated_particles.clear();
    }

private:
    Array2D<Particle, WIDTH * CHUNK_WIDTH, HEIGHT * CHUNK_HEIGHT> m_particles;
    Bitset2D<WIDTH * CHUNK_WIDTH, HEIGHT * CHUNK_HEIGHT> m_updated_particles;
    
    Bitset2D<WIDTH, HEIGHT> m_dirty_chunks;

    ThreadPool m_thread_pool;
};
