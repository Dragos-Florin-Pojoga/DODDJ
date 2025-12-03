#pragma once

#include <SDL3/SDL.h>
#include <vector>

#include "./Commons.hpp"
#include "./ComponentStore.hpp"
#include "./Constatnts.hpp"
#include "./Logging.hpp"
#include "./Transform2D.hpp"

// clang-format off

struct Renderable {
    enum class Shape { QUAD, CIRCLE };
    Shape shape = Shape::QUAD;
    SDL_FColor color = { 1.0f, 1.0f, 1.0f, 1.0f };
    SDL_Texture* texture = nullptr; // nullptr for solid color
    
    // higher on top
    enum class ZIndex {
        BACKGROUND = 0,
        DEFAULT = 100,
        FOREGROUND = 200,
        UI = 300,
    };
    ZIndex z_index = ZIndex::DEFAULT;
};

// Manages a single batch of vertices for one texture
class BatchRenderer {
public:
    BatchRenderer() {
        m_vertices.resize(MAX_VERTICES_PER_BATCH);
        m_indices.resize(MAX_INDICES_PER_BATCH);
    }
    ~BatchRenderer() = default;

    void begin() {
        if (!m_renderer) {
            Logging::log_critical("BatchRenderer has no SDL_Renderer set!"); 
            return;
        }
        m_vertex_count = 0;
        m_index_count = 0;
        m_current_texture = nullptr;
    }
    
    void submit(const Transform2D& transform, const Renderable& renderable);

    void end() {
        if (m_vertex_count > 0) {
            flush();
        }
    }
    
    void setRenderer(SDL_Renderer* renderer) {
        m_renderer = renderer;
    }

private:
    void flush() {
        if (m_vertex_count == 0 || !m_renderer) return;

        // draw all vertices with m_current_texture
        // If m_current_texture is nullptr, use solid vertex colors
        SDL_RenderGeometry(m_renderer,
            m_current_texture,
            m_vertices.data(), m_vertex_count,
            m_indices.data(), m_index_count);

        m_vertex_count = 0;
        m_index_count = 0;
        // m_current_texture is NOT reset, it's set by submit()
    }

    SDL_Renderer* m_renderer = nullptr;
    
    SDL_Texture* m_current_texture = nullptr;

    std::vector<SDL_Vertex> m_vertices;
    std::vector<int> m_indices;
    
    size_t m_vertex_count = 0;
    size_t m_index_count = 0;

    // Settings
    static const size_t MAX_QUADS_PER_BATCH = 1000;
    static const size_t MAX_VERTICES_PER_BATCH = MAX_QUADS_PER_BATCH * 4;
    static const size_t MAX_INDICES_PER_BATCH = MAX_QUADS_PER_BATCH * 6;
};

void BatchRenderer::submit(const Transform2D& transform, const Renderable& renderable) {
    if (m_vertex_count + 4 > MAX_VERTICES_PER_BATCH) {
        flush(); // Not enough space, flush current batch
    }
    
    if (renderable.texture != m_current_texture) {
        flush(); // Texture has changed, flush old batch
        m_current_texture = renderable.texture; // Start new batch with new texture
    }

    const float w_pixels = transform.scale.x * METERS_TO_PIXELS;
    const float h_pixels = transform.scale.y * METERS_TO_PIXELS;

    // Center of the quad
    const float x = transform.position.x * METERS_TO_PIXELS;
    const float y = transform.position.y * METERS_TO_PIXELS;
    
    // TODO: Add rotation logic
    
    const float half_w = w_pixels * 0.5f;
    const float half_h = h_pixels * 0.5f;

    // 0---1
    // | \ |
    // 3---2
    // 1 quad = 2 triangles: 0-1-2 and 0-2-3

    m_vertices[m_vertex_count + 0] = { { x - half_w, y - half_h }, renderable.color, {0.0f, 0.0f} }; // Top-left
    m_vertices[m_vertex_count + 1] = { { x + half_w, y - half_h }, renderable.color, {1.0f, 0.0f} }; // Top-right
    m_vertices[m_vertex_count + 2] = { { x + half_w, y + half_h }, renderable.color, {1.0f, 1.0f} }; // Bottom-right
    m_vertices[m_vertex_count + 3] = { { x - half_w, y + half_h }, renderable.color, {0.0f, 1.0f} }; // Bottom-left
    
    m_indices[m_index_count + 0] = m_vertex_count + 0;
    m_indices[m_index_count + 1] = m_vertex_count + 1;
    m_indices[m_index_count + 2] = m_vertex_count + 2;
    m_indices[m_index_count + 3] = m_vertex_count + 0;
    m_indices[m_index_count + 4] = m_vertex_count + 2;
    m_indices[m_index_count + 5] = m_vertex_count + 3;
    
    m_vertex_count += 4;
    m_index_count += 6;
}


class RenderSystem {
public:
    RenderSystem(ComponentStore<Transform2D>& transforms, ComponentStore<Renderable>& renderables)
        : m_transforms(transforms), m_renderables(renderables) 
    {
    }

    void draw(SDL_Renderer* renderer) {
        m_batcher.setRenderer(renderer);

        m_render_jobs.clear();
        
        for (Entity e : m_renderables.all_entities()) {
            const Renderable* r = m_renderables.get(e);
            const Transform2D* t = m_transforms.get(e);
            m_render_jobs.push_back({ t, r });
        }

        // Sort by z_index, then by texture
        std::sort(m_render_jobs.begin(), m_render_jobs.end(), [](const RenderJob& a, const RenderJob& b) {
            if (a.renderable->z_index != b.renderable->z_index) {
                return a.renderable->z_index < b.renderable->z_index;
            }
            return a.renderable->texture < b.renderable->texture;
        });

        m_batcher.begin();
        for (const auto& job : m_render_jobs) {
            m_batcher.submit(*job.transform, *job.renderable);
        }
        m_batcher.end();
    }

private:
    ComponentStore<Transform2D>& m_transforms;
    ComponentStore<Renderable>& m_renderables;
    
    BatchRenderer m_batcher;

    struct RenderJob {
        const Transform2D* transform;
        const Renderable* renderable;
    };
    std::vector<RenderJob> m_render_jobs;
};