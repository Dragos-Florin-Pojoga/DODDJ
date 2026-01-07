#pragma once

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <utility>

#include "Array2D.hpp"
#include "Commons.hpp"
#include "Logging.hpp"
#include "ThreadPool.hpp"

i32 SLOWDOWN_FACTOR = 1;

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
};

static const std::array<SDL_FColor, 4> particle_colors = {{
    {0.0f, 0.0f, 0.0f, 1.0f}, // AIR
    {0.5f, 0.5f, 0.5f, 1.0f}, // STONE
    {1.0f, 1.0f, 0.0f, 1.0f}, // SAND
    {0.0f, 0.0f, 1.0f, 1.0f}, // WATER
}};

static std::array<u32, 4> particle_colors_u32;

struct Particle {
    ParticleID id; // id defines material, behavior and color
    u16 lifetime;  // in ms
};

template <u32 WIDTH, u32 HEIGHT, u32 CHUNK_WIDTH = 64, u32 CHUNK_HEIGHT = 64>
class SandWorld {
public:
    SandWorld() {
        clear();

        const SDL_PixelFormatDetails* details = SDL_GetPixelFormatDetails(SDL_PIXELFORMAT_RGBA8888);
        for (size_t i = 0; i < particle_colors.size(); ++i) {
            const auto& c = particle_colors[i];
            particle_colors_u32[i] = SDL_MapRGBA(details, nullptr, static_cast<u8>(c.r * 255), static_cast<u8>(c.g * 255), static_cast<u8>(c.b * 255), static_cast<u8>(c.a * 255));
        }
    }

    void update_sand(const u32 x, const u32 y) {
        static constexpr std::pair<int, int> dirs[] = {
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
                // m_updated_particles(nx, ny) = true;
                return;
            } else if (m_particles(nx, ny).id == ParticleID::WATER) {
                m_particles(nx, ny).id = ParticleID::SAND;
                m_particles(x, y).id = ParticleID::WATER;

                m_updated_particles.set(nx, ny);
                update_water(x, y); // expensive but, prevents water climbing up
                // m_updated_particles(nx, ny) = true;
                return;
            }
        }
    }

    void update_water(const u32 x, const u32 y) {
        // down
        if (m_particles(x, y + 1).id == ParticleID::AIR) {
            m_particles(x, y + 1).id = ParticleID::WATER;
            m_particles(x, y).id = ParticleID::AIR;
            m_updated_particles.set(x, y + 1);
            return;
        }

        const u32 r = fast_rand();
        const bool flip = r & 1;

        // down-left / down-right
        {
            const int dx1 = flip ? -1 : 1;
            const int dx2 = flip ? 1 : -1;

            if (m_particles(x + dx1, y + 1).id == ParticleID::AIR) {
                m_particles(x + dx1, y + 1).id = ParticleID::WATER;
                m_particles(x, y).id = ParticleID::AIR;
                m_updated_particles.set(x + dx1, y + 1);
                return;
            }

            if (m_particles(x + dx2, y + 1).id == ParticleID::AIR) {
                m_particles(x + dx2, y + 1).id = ParticleID::WATER;
                m_particles(x, y).id = ParticleID::AIR;
                m_updated_particles.set(x + dx2, y + 1);
                return;
            }
        }

        // left / right
        {
            const int dx1 = (r & 2) ? -1 : 1;
            const int dx2 = -dx1;

            if (m_particles(x + dx1, y).id == ParticleID::AIR) {
                m_particles(x + dx1, y).id = ParticleID::WATER;
                m_particles(x, y).id = ParticleID::AIR;
                m_updated_particles.set(x + dx1, y);
                return;
            }

            if (m_particles(x + dx2, y).id == ParticleID::AIR) {
                m_particles(x + dx2, y).id = ParticleID::WATER;
                m_particles(x, y).id = ParticleID::AIR;
                m_updated_particles.set(x + dx2, y);
                return;
            }
        }
    }

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
        static u32 frame_count = 0;
        frame_count++;

        if (frame_count % SLOWDOWN_FACTOR != 0) {
            return;
        }

        m_updated_particles.clear();

        const bool flip_chunks_x = frame_count & 1; // every 1
        const bool flip_chunks_y = (frame_count >> 1) & 1;  // every 2

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
        }
    }

    void clear() {
        m_particles.fill({ParticleID::AIR, 0});

        // stone border
        for (u32 i = 0; i < WIDTH * CHUNK_WIDTH; ++i) {
            m_particles(i, HEIGHT * CHUNK_HEIGHT - 1).id = ParticleID::STONE;
            m_particles(i, 0).id = ParticleID::STONE;
        }
        for (u32 i = 0; i < HEIGHT * CHUNK_HEIGHT; ++i) {
            m_particles(WIDTH * CHUNK_WIDTH - 1, i).id = ParticleID::STONE;
            m_particles(0, i).id = ParticleID::STONE;
        }
    }

private:
    Array2D<Particle, WIDTH * CHUNK_WIDTH, HEIGHT * CHUNK_HEIGHT> m_particles;
    Bitset2D<WIDTH * CHUNK_WIDTH, HEIGHT * CHUNK_HEIGHT> m_updated_particles;
    // Array2D<bool, WIDTH * CHUNK_WIDTH, HEIGHT * CHUNK_HEIGHT> m_updated_particles;
    ThreadPool m_thread_pool;
};
