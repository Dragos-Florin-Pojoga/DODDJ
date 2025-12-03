#pragma once

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <utility>

#include "Array2D.hpp"
#include "Commons.hpp"
#include "Logging.hpp"
#include "ThreadPool.hpp"

i32 SLOWDOWN_FACTOR = 100;

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
class World {
public:
    World() {
        // stone border
        for (u32 i = 0; i < WIDTH * CHUNK_WIDTH; ++i) {
            m_particles(i, HEIGHT * CHUNK_HEIGHT - 1).id = ParticleID::STONE;
            m_particles(i, 0).id = ParticleID::STONE;
        }
        for (u32 i = 0; i < HEIGHT * CHUNK_HEIGHT; ++i) {
            m_particles(WIDTH * CHUNK_WIDTH - 1, i).id = ParticleID::STONE;
            m_particles(0, i).id = ParticleID::STONE;
        }

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
                // m_updated_particles(nx, ny) = true;
                return;
            }
        }
    }

    void update_water(const u32 x, const u32 y) {
        static constexpr std::pair<int, int> dirs[] = {
            {0, 1},  // down
            {-1, 1}, // down-left
            {1, 1},  // down-right
            {-1, 0}, // left
            {1, 0}   // right
        };

        for (auto [dx, dy] : dirs) {
            const u32 nx = x + dx;
            const u32 ny = y + dy;

            if (m_particles(nx, ny).id == ParticleID::AIR) {
                m_particles(nx, ny).id = ParticleID::WATER;
                m_particles(x, y).id = ParticleID::AIR;

                m_updated_particles.set(nx, ny);
                // m_updated_particles(nx, ny) = true;
                return;
            }
        }
    }

    void update_chunk(const u32 chunk_x, const u32 chunk_y) {
        // bottom left corner of the chunk
        const u32 x_start = chunk_x * CHUNK_WIDTH;
        const u32 y_start = (chunk_y + 1) * CHUNK_HEIGHT - 1;

        for (u32 i = 0; i < CHUNK_WIDTH; ++i) {
            for (u32 j = 0; j < CHUNK_HEIGHT; ++j) {
                const u32 x = x_start + i;
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

        m_particles.at(100, 1).id = ParticleID::SAND;
        m_particles.at(200, 1).id = ParticleID::WATER;

        for (u32 phase_y = 0; phase_y < 2; ++phase_y) {
            for (u32 phase_x = 0; phase_x < 2; ++phase_x) {
                for (i32 chunk_y = HEIGHT - 1 - phase_y; chunk_y >= 0; chunk_y -= 2) {
                    for (u32 chunk_x = phase_x; chunk_x < WIDTH; chunk_x += 2) {
                        m_thread_pool.enqueue([this, chunk_x, chunk_y] { update_chunk(chunk_x, chunk_y); });
                    }
                }
                m_thread_pool.wait_all();
            }
        }
    }

    void renderToTexture(SDL_Renderer* renderer, SDL_Texture* texture) {
        if (m_texture_buffer.size() != WIDTH * CHUNK_WIDTH * HEIGHT * CHUNK_HEIGHT) {
            Logging::log_warning("Resizing texture buffer");
            m_texture_buffer.resize(WIDTH * CHUNK_WIDTH * HEIGHT * CHUNK_HEIGHT);
        }

        const u32 total_rows = HEIGHT * CHUNK_HEIGHT;
        const u32 rows_per_task = 32;

        for (u32 y_start = 0; y_start < total_rows; y_start += rows_per_task) {
            m_thread_pool.enqueue([this, y_start, rows_per_task, total_rows] {
                const u32 y_end = std::min(y_start + rows_per_task, total_rows);
                const u32 width = WIDTH * CHUNK_WIDTH;

                for (u32 y = y_start; y < y_end; ++y) {
                    for (u32 x = 0; x < width; ++x) {
                        const auto& particle = m_particles(x, y);
                        m_texture_buffer[y * width + x] = particle_colors_u32[static_cast<u32>(particle.id)];
                    }
                }
            });
        }
        m_thread_pool.wait_all();

        SDL_UpdateTexture(texture, nullptr, m_texture_buffer.data(), WIDTH * CHUNK_WIDTH * sizeof(u32));
    }

    u32 width() const { return WIDTH * CHUNK_WIDTH; }
    u32 height() const { return HEIGHT * CHUNK_HEIGHT; }

private:
    Array2D<Particle, WIDTH * CHUNK_WIDTH, HEIGHT * CHUNK_HEIGHT> m_particles;
    Bitset2D<WIDTH * CHUNK_WIDTH, HEIGHT * CHUNK_HEIGHT> m_updated_particles;
    // Array2D<bool, WIDTH * CHUNK_WIDTH, HEIGHT * CHUNK_HEIGHT> m_updated_particles;
    ThreadPool m_thread_pool;
    std::vector<u32> m_texture_buffer;
};
