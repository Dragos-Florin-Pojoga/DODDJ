#pragma once

#include <SDL3/SDL.h>
#include <vector>

#include "./Commons.hpp"
#include "./ComponentStore.hpp"
#include "./Logging.hpp"
#include "./Transform2D.hpp"
#include "./Camera.hpp"

// clang-format off

struct Renderable {
    enum class Shape { QUAD, CIRCLE };
    // higher on top
    enum class ZIndex {
        BACKGROUND = 0,
        DEFAULT = 100,
        FOREGROUND = 200,
        UI = 300,
    };

    Shape shape = Shape::QUAD;
    ZIndex z_index = ZIndex::DEFAULT;
    SDL_Texture* texture = nullptr; // nullptr for solid color
    SDL_FColor color = { 1.0f, 1.0f, 1.0f, 1.0f };
};

// Manages a single batch of vertices for one texture
class BatchRenderer {
public:
    BatchRenderer() {
        m_vertices.resize(MAX_VERTICES_PER_BATCH);
        m_indices.resize(MAX_INDICES_PER_BATCH);
    }
    ~BatchRenderer() = default;

    void begin(const Camera& camera) {
        if (!m_renderer) {
            Logging::log_critical("BatchRenderer has no SDL_Renderer set!"); 
            return;
        }
        m_camera = &camera;
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
    const Camera* m_camera = nullptr;

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

    const float half_w = transform.scale.x / 2;
    const float half_h = transform.scale.y / 2;
    
    const b2Vec2 center = { transform.position.x, transform.position.y };
    
    // TODO: Add rotation logic ?
    const b2Vec2 topLeft = { center.x - half_w, center.y - half_h };
    const b2Vec2 topRight = { center.x + half_w, center.y - half_h };
    const b2Vec2 bottomRight = { center.x + half_w, center.y + half_h };
    const b2Vec2 bottomLeft = { center.x - half_w, center.y + half_h };
    
    const SDL_FPoint screenTL = m_camera->worldToScreen(topLeft);
    const SDL_FPoint screenTR = m_camera->worldToScreen(topRight);
    const SDL_FPoint screenBR = m_camera->worldToScreen(bottomRight);
    const SDL_FPoint screenBL = m_camera->worldToScreen(bottomLeft);

    // 0---1
    // | \ |
    // 3---2
    // 1 quad = 2 triangles: 0-1-2 and 0-2-3

    m_vertices[m_vertex_count + 0] = { screenTL, renderable.color, {0.0f, 0.0f} }; // Top-left
    m_vertices[m_vertex_count + 1] = { screenTR, renderable.color, {1.0f, 0.0f} }; // Top-right
    m_vertices[m_vertex_count + 2] = { screenBR, renderable.color, {1.0f, 1.0f} }; // Bottom-right
    m_vertices[m_vertex_count + 3] = { screenBL, renderable.color, {0.0f, 1.0f} }; // Bottom-left
    
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

    void draw(SDL_Renderer* renderer, const Camera& camera) {
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

        m_batcher.begin(camera);
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