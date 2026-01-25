#pragma once

#include <box2d/box2d.h>
#include <vector>
#include <unordered_map>
#include <cmath>

#include "SandSimulation.hpp"
#include "Commons.hpp"
#include "Logging.hpp"
#include "Camera.hpp"

// manages the relationship between Box2D rigidbodies and sand world pixels
class RigidbodyManager {
public:
    // info about a rigidbody's pixel representation
    struct BodyInfo {
        b2BodyId body_id;
        u8 manager_id;     // 1-255, used in Particle::body_id
        f32 width, height; // Size in meters
        ParticleID material; // FIXME: see below
        
        /////////
        // WIP //
        /////////
        // TODO: This assumes a rigidbody is made of the same material which is not always true
        //       For complex objects, the particles should know their UV inside the body

        // stored pixels when extracted (relative coords + types)
        struct StoredPixel {
            i32 rel_x, rel_y; // Relative to body center
            ParticleID type;
        };
        std::vector<StoredPixel> stored_pixels;
    };
    
    u8 register_body(b2BodyId body_id, f32 width, f32 height, ParticleID material) {
        if (m_next_id == 0) {
            m_next_id = 1; // 0 is reserved for terrain
        }
        const u8 id = m_next_id++;
        
        BodyInfo info;
        info.body_id = body_id;
        info.manager_id = id;
        info.width = width;
        info.height = height;
        info.material = material;
        
        m_bodies[id] = info;
        return id;
    }
    
    // helper to iterate pixels inside rotated body
    template<typename Func>
    void for_each_pixel_in_body(b2BodyId body_id, Func&& func) {
        if (!b2Body_IsValid(body_id)) {
            Logging::log_error("Invalid body");
            return;
        }
        
        const b2Transform xf = b2Body_GetTransform(body_id);
        
        // get shapes
        const i32 shapeCount = b2Body_GetShapeCount(body_id);
        if (shapeCount == 0) {
            Logging::log_error("No shapes found");
            return;
        }
        std::vector<b2ShapeId> shapes(shapeCount);
        b2Body_GetShapes(body_id, shapes.data(), shapeCount);
        
        // compute rotated AABB union
        b2AABB aabb = b2Shape_GetAABB(shapes[0]); // world space AABB
        for(i32 i = 1; i < shapeCount; ++i) {
            const b2AABB s_aabb = b2Shape_GetAABB(shapes[i]);
            aabb.lowerBound = b2Min(aabb.lowerBound, s_aabb.lowerBound);
            aabb.upperBound = b2Max(aabb.upperBound, s_aabb.upperBound);
        }
        
        // expand slightly for safety (rasterization coverage)
        const i32 min_x = static_cast<i32>(std::floor(aabb.lowerBound.x * PIXELS_PER_METER));
        const i32 max_x = static_cast<i32>(std::ceil(aabb.upperBound.x * PIXELS_PER_METER));
        const i32 min_y = static_cast<i32>(std::floor(aabb.lowerBound.y * PIXELS_PER_METER));
        const i32 max_y = static_cast<i32>(std::ceil(aabb.upperBound.y * PIXELS_PER_METER));
        
        // iterate AABB
        for (i32 py = min_y; py <= max_y; ++py) {
            for (i32 px = min_x; px <= max_x; ++px) {
                // check center of pixel
                const b2Vec2 world_pos = { (px + 0.5f) / PIXELS_PER_METER, (py + 0.5f) / PIXELS_PER_METER };
                const b2Vec2 local_pos = b2InvTransformPoint(xf, world_pos);

                bool inside = false;
                // check all shapes
                for (b2ShapeId shapeId : shapes) {
                    if (b2Shape_TestPoint(shapeId, world_pos)) {
                        inside = true;
                        break;
                    }
                }
                
                if (inside) {
                    func(px, py, local_pos); // pass local_pos if needed for texture mapping
                }
            }
        }
    }

    // extract body pixels from world
    // stores the pixels and clears them from world
    template<u32 W, u32 H>
    void extract_body_pixels(u8 id, SandWorld<W, H>& world) {
        const auto it = m_bodies.find(id);
        if (it == m_bodies.end()) {
            Logging::log_error("Body not found");
            return;
        }
        
        const BodyInfo& info = it->second;
        if (!b2Body_IsValid(info.body_id)) {
            Logging::log_error("Invalid body");
            return;
        }
        
        for_each_pixel_in_body(info.body_id, [&](i32 px, i32 py, b2Vec2 local) {
            if (px > 0 && px < static_cast<i32>(world.width()) - 1 &&
                py > 0 && py < static_cast<i32>(world.height()) - 1) {
                
                Particle& p = world.getParticleMut(px, py);
                if (p.body_id == id) {
                    p.id = ParticleID::AIR;
                    p.body_id = 0;
                }
            }
        });
    }
    
    // restore body pixels after physics step
    // returns list of displaced sand pixels that need to become debris
    template<u32 W, u32 H>
    std::vector<std::tuple<i32, i32, ParticleID>> restore_body_pixels(u8 id, SandWorld<W, H>& world) {
        std::vector<std::tuple<i32, i32, ParticleID>> displaced;
        
        const auto it = m_bodies.find(id);
        if (it == m_bodies.end()) {
            Logging::log_error("Body not found");
            return displaced;
        }
        
        const BodyInfo& info = it->second;
        if (!b2Body_IsValid(info.body_id)) {
            Logging::log_error("Invalid body");
            return displaced;
        }
        
        for_each_pixel_in_body(info.body_id, [&](i32 px, i32 py, b2Vec2 local) {
            if (px > 0 && px < static_cast<i32>(world.width()) - 1 &&
                py > 0 && py < static_cast<i32>(world.height()) - 1) {
                
                Particle& p = world.getParticleMut(px, py);
                if (p.body_id == 0 && p.id != ParticleID::AIR) {
                    displaced.push_back({px, py, p.id});
                }
                
                // stamp pixel
                // here we use the body's material (uniform)
                // TODO: see top of file
                p.id = info.material;
                p.body_id = id;
            }
        });
        
        return displaced;
    }
    
    // extract ALL registered bodies
    template<u32 W, u32 H>
    void extract_all(SandWorld<W, H>& world) {
        for (auto& [id, info] : m_bodies) {
            extract_body_pixels(id, world);
        }
    }
    
    // restore ALL registered bodies, collect all displaced
    template<u32 W, u32 H>
    std::vector<std::tuple<i32, i32, ParticleID>> restore_all(SandWorld<W, H>& world) {
        std::vector<std::tuple<i32, i32, ParticleID>> all_displaced;
        
        for (auto& [id, info] : m_bodies) {
            const auto displaced = restore_body_pixels(id, world);
            all_displaced.insert(all_displaced.end(), displaced.begin(), displaced.end());
        }
        
        return all_displaced;
    }
    
    void clear() {
        m_bodies.clear();
        m_next_id = 1;
    }
    
    const std::unordered_map<u8, BodyInfo>& get_bodies() const { return m_bodies; }
    
private:
    std::unordered_map<u8, BodyInfo> m_bodies;
    u8 m_next_id = 1;
};
