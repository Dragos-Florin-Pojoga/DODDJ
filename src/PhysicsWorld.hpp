#pragma once

// HACKY: This needs to be included after SandWorld.hpp

#include <vector>
#include <cmath>
#include <box2d/box2d.h>
#include <SDL3/SDL.h>
#include "Logging.hpp"
#include "Camera.hpp"
#include "Commons.hpp"

class PhysicsWorld {
public:
    PhysicsWorld() {
        init();
    }
    
    ~PhysicsWorld() {
        if (b2Body_IsValid(m_terrain_body_id)) {
            b2DestroyBody(m_terrain_body_id);
        }
        if (b2World_IsValid(m_world_id)) {
            b2DestroyWorld(m_world_id);
        }
    }

    void init() {
        b2WorldDef def = b2DefaultWorldDef();
        def.gravity = {0.0f, 10.0f};
        m_world_id = b2CreateWorld(&def);
        
        b2BodyDef groundBodyDef = b2DefaultBodyDef();
        groundBodyDef.position = {0.0f, 0.0f}; 
        m_terrain_body_id = b2CreateBody(m_world_id, &groundBodyDef);
    }
    
    void reset() {
        if (b2World_IsValid(m_world_id)) {
            b2DestroyWorld(m_world_id);
        }
        m_dynamic_bodies.clear();
        init();
    }

    void step(f32 dt) {
        b2World_Step(m_world_id, dt, 4);
    }

    void update_terrain_mesh(const std::vector<std::vector<b2Vec2>>& chains) {
        if (!b2Body_IsValid(m_terrain_body_id)) {
            Logging::log_error("Terrain body is not valid!");
            return;
        }

        // clear existing shapes from terrain body
        const i32 shapeCount = b2Body_GetShapeCount(m_terrain_body_id);
        if (shapeCount > 0) {
            std::vector<b2ShapeId> shapes(shapeCount);
            const i32 count = b2Body_GetShapes(m_terrain_body_id, shapes.data(), shapeCount);
            for(i32 i = 0; i < count; ++i) {
                b2DestroyShape(shapes[i], false); // defer mass update since it's static
            }
        }
        
        const b2ShapeDef shapeDef = b2DefaultShapeDef();
        
        // add new chains/segments
        for (const auto& chain_points : chains) {
            u64 n = chain_points.size();
            if (n < 2) {
                continue;  // need at least 2 points
            }
            
            // check if chain is closed (first point == last point)
            bool is_closed = false;
            if (n >= 3) {
                const f32 threshold = 0.001f;
                b2Vec2 diff = {chain_points.back().x - chain_points.front().x,
                               chain_points.back().y - chain_points.front().y};
                is_closed = (diff.x * diff.x + diff.y * diff.y) < threshold * threshold;
            }
            
            // for very short chains (2-3 points), use segment shapes
            if (n == 2 || (n == 3 && !is_closed)) {
                // create individual segment shapes for each edge
                for (u64 i = 0; i + 1 < n; ++i) {
                    const b2Segment seg = {chain_points[i], chain_points[i + 1]};
                    b2CreateSegmentShape(m_terrain_body_id, &shapeDef, &seg);
                }
            } else if (n >= 4 || (n == 3 && is_closed)) {
                // Box2D requires at least 4 points for a chain
                b2ChainDef chainDef = b2DefaultChainDef();
                chainDef.isLoop = is_closed;
                
                if (n == 3 && is_closed) {
                    // duplicate first point to meet 4-point minimum (it's already closed)
                    std::vector<b2Vec2> extended = chain_points;
                    extended.push_back(chain_points[0]);  // add extra point
                    chainDef.points = extended.data();
                    chainDef.count = static_cast<i32>(extended.size());
                    b2CreateChain(m_terrain_body_id, &chainDef);
                } else {
                    chainDef.points = chain_points.data();
                    chainDef.count = static_cast<i32>(n);
                    b2CreateChain(m_terrain_body_id, &chainDef);
                }
            }
        }
    }
    
    i32 get_terrain_shape_count() const {
        if (!b2Body_IsValid(m_terrain_body_id)) {
            return 0;
        }
        return b2Body_GetShapeCount(m_terrain_body_id);
    }
    
    b2BodyId create_box(f32 x, f32 y, f32 width, f32 height) {
        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.type = b2_dynamicBody;
        bodyDef.position = {x, y};
        
        const b2BodyId bodyId = b2CreateBody(m_world_id, &bodyDef);
        
        const b2Polygon box = b2MakeBox(width * 0.5f, height * 0.5f);
        
        b2ShapeDef shapeDef = b2DefaultShapeDef();
        shapeDef.density = 1.0f;
        shapeDef.material.friction = 0.3f;
        shapeDef.material.restitution = 0.2f; // Bounciness
        
        // BOX Filter: Cat 2 (Dynamic), Mask Terrain(1) | Dynamic(2) | Debris(4)
        shapeDef.filter.categoryBits = 0x0002;
        shapeDef.filter.maskBits = 0x0001 | 0x0002 | 0x0004;
        
        b2CreatePolygonShape(bodyId, &shapeDef, &box);
        
        m_dynamic_bodies.push_back(bodyId);
        return bodyId;
    }
    
    // debris particle info
    struct DebrisParticle {
        b2BodyId body_id;
        ParticleID particle_type;
        u8 settled_frames; // frames with low velocity
        u16 age;           // age in frames (to kill old debris)
        u16 stuck_frames;  // frames stuck overlapping a solid block
    };
    
    // create debris particle (ejected sand)
    void create_debris(f32 x, f32 y, f32 vx, f32 vy, ParticleID particle_type) {
        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.type = b2_dynamicBody;
        bodyDef.position = {x, y};
        bodyDef.linearVelocity = {vx, vy};
        bodyDef.gravityScale = 1.0f;
        
        const b2BodyId bodyId = b2CreateBody(m_world_id, &bodyDef);
        
        b2Circle circle;
        circle.center = {0, 0};
        circle.radius = 0.5f / PIXELS_PER_METER;
        
        b2ShapeDef shapeDef = b2DefaultShapeDef();
        shapeDef.density = 0.001f;
        shapeDef.material.friction = 0.5f;
        shapeDef.material.restitution = 0.3f;

        // DEBRIS Filter: Cat 4 (Debris), Mask Terrain(1) | Dynamic(2)
        shapeDef.filter.categoryBits = 0x0004;
        shapeDef.filter.maskBits = 0x0001 | 0x0002;
        
        b2CreateCircleShape(bodyId, &shapeDef, &circle);
        
        DebrisParticle dp;
        dp.body_id = bodyId;
        dp.particle_type = particle_type;
        dp.settled_frames = 0;
        dp.age = 0;
        dp.stuck_frames = 0;
        
        m_debris.push_back(dp);
    }
    
    // update debris particles - check for settling & cleanup
    template<u32 W, u32 H>
    void update_debris(SandWorld<W, H>& world) {
        constexpr f32 SETTLE_VELOCITY_THRESHOLD = 0.5f;
        constexpr u8 SETTLE_FRAMES_REQUIRED = 5;
   
        // TODO: right now, these are physics frames based :/
        constexpr u16 MAX_AGE = 60 * 7;
        
        for (auto it = m_debris.begin(); it != m_debris.end(); ) {
            // validate body
            if (!b2Body_IsValid(it->body_id)) {
                it = m_debris.erase(it);
                continue;
            }
            
            const b2Vec2 pos = b2Body_GetPosition(it->body_id);
            
            // bounds check
            // TODO: this is hacky
            const f32 world_w = static_cast<f32>(world.width()) / PIXELS_PER_METER;
            const f32 world_h = static_cast<f32>(world.height()) / PIXELS_PER_METER;
            if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || 
                pos.x < -10.0f || pos.x > world_w + 10.0f ||
                pos.y < -10.0f || pos.y > world_h + 10.0f) {
                b2DestroyBody(it->body_id);
                it = m_debris.erase(it);
                continue;
            }
            
            // age check
            if (++it->age > MAX_AGE) {
                b2DestroyBody(it->body_id);
                it = m_debris.erase(it);
                continue;
            }
            
            b2Vec2 vel = b2Body_GetLinearVelocity(it->body_id);
            f32 speed = std::sqrt(vel.x * vel.x + vel.y * vel.y);
            
            // grid interaction
            bool overlap_solid = false;
            const i32 px = static_cast<i32>(std::round(pos.x * PIXELS_PER_METER));
            const i32 py = static_cast<i32>(std::round(pos.y * PIXELS_PER_METER));
            
            if (px > 0 && px < static_cast<i32>(world.width()) - 1 &&
                py > 0 && py < static_cast<i32>(world.height()) - 1) {
                const Particle& p = world.getParticle(px, py);
                if (p.id != ParticleID::AIR) {
                    overlap_solid = true;
                    // drag
                    vel.x *= 0.8f;
                    vel.y *= 0.8f;
                    b2Body_SetLinearVelocity(it->body_id, vel);
                    speed *= 0.8f;
                }
            }

            if (overlap_solid) {
                // track 'stuck' frames
                ++it->stuck_frames;
                // kill if stuck for too long
                if (it->stuck_frames > 10) { 
                    b2DestroyBody(it->body_id);
                    it = m_debris.erase(it);
                    continue;
                }
            } else {
                it->stuck_frames = 0;
                if (speed < SETTLE_VELOCITY_THRESHOLD) {
                    ++it->settled_frames;
                } else {
                    it->settled_frames = 0;
                }
            }
            
            if (it->settled_frames >= SETTLE_FRAMES_REQUIRED) {
                // settle logic
                if (px > 0 && px < static_cast<i32>(world.width()) - 1 &&
                    py > 0 && py < static_cast<i32>(world.height()) - 1) {
                    Particle& p = world.getParticleMut(px, py);
                    
                    bool supported = false;
                    if (py + 1 < static_cast<i32>(world.height())) {
                        const Particle& below = world.getParticle(px, py + 1);
                        if (below.id != ParticleID::AIR) {
                            supported = true;
                        }
                    }

                    if (p.id == ParticleID::AIR && supported) {
                        p.id = static_cast<ParticleID>(it->particle_type);
                        p.body_id = 0;
                        
                        b2DestroyBody(it->body_id);
                        it = m_debris.erase(it);
                        continue;
                    }
                }

                
                it->settled_frames = 0;
            }
            ++it;
        }
    }
    
    u64 debris_count() const { return m_debris.size(); }
    
    // simple debug draw
    void render_debug(SDL_Renderer* renderer, const Camera& camera) {
        // draw terrain
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); 
        draw_body(renderer, camera, m_terrain_body_id);
        
        // draw dynamic bodies
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255); 

        for (b2BodyId id : m_dynamic_bodies) {
            draw_body(renderer, camera, id);
        }
    }
    
    // render debris as colored points (batched for performance)
    // TODO: this is really hacky and should be done better
    void render_debris(SDL_Renderer* renderer, const Camera& camera) {
        // reuse buffers to avoid allocation
        m_batch_sand.clear();
        m_batch_water.clear();
        m_batch_stone.clear();
        m_batch_wood.clear();
        m_batch_other.clear();
        
        // size of debris visual (slightly larger than 1px for visibility)
        const f32 size = 2.5f;
        const f32 offset = size * 0.5f;

        for (const auto& dp : m_debris) {
            if (!b2Body_IsValid(dp.body_id)) continue;
            
            const b2Vec2 pos = b2Body_GetPosition(dp.body_id);
            const SDL_FPoint p = camera.worldToScreen({pos.x, pos.y});
            
            const SDL_FRect r = {p.x - offset, p.y - offset, size, size};

            switch (dp.particle_type) {
                case ParticleID::SAND:  m_batch_sand.push_back(r); break;
                case ParticleID::WATER: m_batch_water.push_back(r); break;
                case ParticleID::STONE: m_batch_stone.push_back(r); break;
                case ParticleID::WOOD:  m_batch_wood.push_back(r); break;
                default:                m_batch_other.push_back(r); break;
            }
        }
        
        if (!m_batch_sand.empty()) {
            const auto& c = particle_colors[static_cast<i32>(ParticleID::SAND)];
            SDL_SetRenderDrawColorFloat(renderer, c.r, c.g, c.b, c.a);
            SDL_RenderFillRects(renderer, m_batch_sand.data(), static_cast<i32>(m_batch_sand.size()));
        }
        if (!m_batch_water.empty()) {
            const auto& c = particle_colors[static_cast<i32>(ParticleID::WATER)];
            SDL_SetRenderDrawColorFloat(renderer, c.r, c.g, c.b, c.a);
            SDL_RenderFillRects(renderer, m_batch_water.data(), static_cast<i32>(m_batch_water.size()));
        }
        if (!m_batch_stone.empty()) {
            const auto& c = particle_colors[static_cast<i32>(ParticleID::STONE)];
            SDL_SetRenderDrawColorFloat(renderer, c.r, c.g, c.b, c.a);
            SDL_RenderFillRects(renderer, m_batch_stone.data(), static_cast<i32>(m_batch_stone.size()));
        }
        if (!m_batch_wood.empty()) {
            const auto& c = particle_colors[static_cast<i32>(ParticleID::WOOD)];
            SDL_SetRenderDrawColorFloat(renderer, c.r, c.g, c.b, c.a);
            SDL_RenderFillRects(renderer, m_batch_wood.data(), static_cast<i32>(m_batch_wood.size()));
        }
        if (!m_batch_other.empty()) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // White
            SDL_RenderFillRects(renderer, m_batch_other.data(), static_cast<i32>(m_batch_other.size()));
        }
    }

    b2WorldId get_world_id() { return m_world_id; }
    const std::vector<b2BodyId>& get_dynamic_bodies() const { return m_dynamic_bodies; }
    u64 get_dynamic_body_count() const { return m_dynamic_bodies.size(); }

private:
    // for debug rendering
    void draw_body(SDL_Renderer* renderer, const Camera& camera, b2BodyId bodyId) {
        if (!b2Body_IsValid(bodyId)) {
            return;
        }
        
        const b2Transform xf = b2Body_GetTransform(bodyId);
        const i32 shapeCount = b2Body_GetShapeCount(bodyId);
        if (shapeCount == 0) {
            return;
        }
        
        std::vector<b2ShapeId> shapes(shapeCount);
        b2Body_GetShapes(bodyId, shapes.data(), shapeCount);
        
        for (b2ShapeId shapeId : shapes) {
            const b2ShapeType type = b2Shape_GetType(shapeId);
            if (type == b2_polygonShape) {
                b2Polygon poly = b2Shape_GetPolygon(shapeId);
                draw_polygon(renderer, camera, &poly, xf);
            } else if (type == b2_chainSegmentShape) {
                b2ChainSegment chainSegment = b2Shape_GetChainSegment(shapeId);
                draw_segment(renderer, camera, &chainSegment.segment, xf);
            } else if (type == b2_segmentShape) {
                b2Segment seg = b2Shape_GetSegment(shapeId);
                draw_segment(renderer, camera, &seg, xf);
            } else if (type == b2_circleShape) {
                const b2Circle circle = b2Shape_GetCircle(shapeId);
                const b2Vec2 center = b2TransformPoint(xf, circle.center);
                const f32 radius = circle.radius;
                
                // simple approximation
                constexpr i32 segments = 8;
                SDL_FPoint points[segments + 1];
                for (i32 i = 0; i <= segments; ++i) {
                    const f32 theta = (f32)i / segments * 2.0f * SDL_PI_F;
                    const f32 x = center.x + radius * std::cos(theta);
                    const f32 y = center.y + radius * std::sin(theta);
                    points[i] = camera.worldToScreen({x, y});
                }
                SDL_RenderLines(renderer, points, segments + 1);
            }
        }
    }
    
    // for debug rendering
    void draw_polygon(SDL_Renderer* renderer, const Camera& camera, b2Polygon* poly, const b2Transform& xf) {
        std::vector<SDL_FPoint> points;
        for (i32 i = 0; i < poly->count; ++i) {
            b2Vec2 world_pos = b2TransformPoint(xf, poly->vertices[i]);
            points.push_back(camera.worldToScreen(world_pos));
        }
        if (!points.empty()) {
            points.push_back(points[0]);
        }

        SDL_RenderLines(renderer, points.data(), static_cast<i32>(points.size()));
    }
    
    // for debug rendering
    void draw_segment(SDL_Renderer* renderer, const Camera& camera, b2Segment* seg, const b2Transform& xf) {
        b2Vec2 p1 = b2TransformPoint(xf, seg->point1);
        b2Vec2 p2 = b2TransformPoint(xf, seg->point2);
        
        SDL_FPoint sp1 = camera.worldToScreen(p1);
        SDL_FPoint sp2 = camera.worldToScreen(p2);
        
        SDL_RenderLine(renderer, sp1.x, sp1.y, sp2.x, sp2.y);
    }

    b2WorldId m_world_id;
    b2BodyId m_terrain_body_id;
    std::vector<b2BodyId> m_dynamic_bodies;
    std::vector<DebrisParticle> m_debris;

    // Render batches
    std::vector<SDL_FRect> m_batch_sand;
    std::vector<SDL_FRect> m_batch_water;
    std::vector<SDL_FRect> m_batch_stone;
    std::vector<SDL_FRect> m_batch_wood;
    std::vector<SDL_FRect> m_batch_other;
};
