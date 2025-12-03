#pragma once

#include <vector>

#include "Commons.hpp"
#include "ComponentStore.hpp"
#include "Entity.hpp"
#include "Transform2D.hpp"

struct Rigidbody2D {
    bool is_static = false;
    float invMass = 1.0f;
    Vec2D velocity = Vec2D::zero;
    Vec2D force = Vec2D::zero;
    float angularVelocity = 0.0f;
    float restitution = 0.2f;
    float linearDamping = 0.0f;
    float drag = 0.1f;
    bool is_sleeping = false;
    float sleep_timer = 0.0f; // Time spent with low velocity

    void setMass(float m) { invMass = (m > 0.0f && !is_static) ? 1.0f / m : 0.0f; }
};

struct CircleCollider {
    float radius = 0.5f;
    Vec2D offset = Vec2D::zero;
};

struct BoxCollider {
    Vec2D size = Vec2D::one; // half-extent
    Vec2D offset = Vec2D::zero;
};

struct Manifold {
    Entity a = INVALID_ENTITY;
    Entity b = INVALID_ENTITY;
    Vec2D normal = Vec2D::zero; // from a to b
    float penetration = 0.0f;
    bool colliding = false;
};

struct Broadphase {
    virtual ~Broadphase() = default;
    virtual std::vector<std::pair<Entity, Entity>> collectPairs(const std::vector<Entity>& candidates) = 0;
};

struct BruteForceBroadphase : public Broadphase {
    std::vector<std::pair<Entity, Entity>> collectPairs(const std::vector<Entity>& candidates) override {
        std::vector<std::pair<Entity, Entity>> pairs;
        pairs.reserve((candidates.size() * (candidates.size() - 1)) / 2);
        for (size_t i = 0; i < candidates.size(); ++i) {
            for (size_t j = i + 1; j < candidates.size(); ++j) {
                pairs.emplace_back(candidates[i], candidates[j]);
            }
        }
        return pairs;
    }
};

struct QuadtreeBroadphase : public Broadphase {
    struct AABB {
        Vec2D min, max;
        bool overlaps(const AABB& other) const { return !(max.x < other.min.x || min.x > other.max.x || max.y < other.min.y || min.y > other.max.y); }
    };

    struct Node {
        AABB bounds;
        std::vector<Entity> entities;
        std::unique_ptr<Node> children[4];
        int depth;
        Node(const AABB& b, int d) : bounds(b), depth(d) {}
        bool isLeaf() const { return !children[0]; }
    };

    int maxDepth = 5;
    int maxEntities = 8;
    ComponentStore<Transform2D>* transforms;
    ComponentStore<CircleCollider>* circles;
    ComponentStore<BoxCollider>* boxes;

    QuadtreeBroadphase(ComponentStore<Transform2D>* t, ComponentStore<CircleCollider>* c, ComponentStore<BoxCollider>* b) : transforms(t), circles(c), boxes(b) {}

    std::vector<std::pair<Entity, Entity>> collectPairs(const std::vector<Entity>& candidates) override {
        if (candidates.empty())
            return {};
        // Compute world bounds
        AABB world = computeWorldAABB(candidates);
        Node root(world, 0);
        // Insert entities
        for (Entity e : candidates) {
            AABB aabb = getEntityAABB(e);
            insert(&root, e, aabb);
        }
        // Collect pairs
        std::vector<std::pair<Entity, Entity>> pairs;
        collectPairsRecursive(&root, pairs);
        return pairs;
    }

    AABB computeWorldAABB(const std::vector<Entity>& entities) {
        Vec2D min = {1e6f, 1e6f}, max = {-1e6f, -1e6f};
        for (Entity e : entities) {
            AABB aabb = getEntityAABB(e);
            min.x = std::min(min.x, aabb.min.x);
            min.y = std::min(min.y, aabb.min.y);
            max.x = std::max(max.x, aabb.max.x);
            max.y = std::max(max.y, aabb.max.y);
        }
        return {min, max};
    }

    AABB getEntityAABB(Entity e) {
        Transform2D* t = transforms->get(e);
        if (circles->has(e)) {
            CircleCollider* c = circles->get(e);
            Vec2D pos = t->position + c->offset;
            float r = c->radius;
            return {pos - Vec2D{r, r}, pos + Vec2D{r, r}};
        } else if (boxes->has(e)) {
            BoxCollider* b = boxes->get(e);
            Vec2D pos = t->position + b->offset;
            Vec2D half = b->size;
            return {pos - half, pos + half};
        }
        return {t->position, t->position};
    }

    void insert(Node* node, Entity e, const AABB& aabb) {
        if (node->depth >= maxDepth || node->isLeaf() && node->entities.size() < maxEntities) {
            node->entities.push_back(e);
            return;
        }
        if (node->isLeaf())
            split(node);
        for (int i = 0; i < 4; ++i) {
            if (node->children[i]->bounds.overlaps(aabb)) {
                insert(node->children[i].get(), e, aabb);
            }
        }
    }

    void split(Node* node) {
        Vec2D c = (node->bounds.min + node->bounds.max) * 0.5f;
        Vec2D min = node->bounds.min, max = node->bounds.max;
        node->children[0] = std::make_unique<Node>(AABB{min, c}, node->depth + 1);                     // TL
        node->children[1] = std::make_unique<Node>(AABB{{c.x, min.y}, {max.x, c.y}}, node->depth + 1); // TR
        node->children[2] = std::make_unique<Node>(AABB{{min.x, c.y}, {c.x, max.y}}, node->depth + 1); // BL
        node->children[3] = std::make_unique<Node>(AABB{c, max}, node->depth + 1);                     // BR
        // Re-insert entities
        auto old = node->entities;
        node->entities.clear();
        for (Entity e : old) {
            AABB aabb = getEntityAABB(e);
            for (int i = 0; i < 4; ++i) {
                if (node->children[i]->bounds.overlaps(aabb)) {
                    node->children[i]->entities.push_back(e);
                }
            }
        }
    }

    void collectPairsRecursive(Node* node, std::vector<std::pair<Entity, Entity>>& pairs) {
        // Brute-force pairs in this node
        for (size_t i = 0; i < node->entities.size(); ++i) {
            for (size_t j = i + 1; j < node->entities.size(); ++j) {
                pairs.emplace_back(node->entities[i], node->entities[j]);
            }
        }
        // Recurse
        if (!node->isLeaf()) {
            for (int i = 0; i < 4; ++i) {
                collectPairsRecursive(node->children[i].get(), pairs);
            }
        }
    }
};

class PhysicsSystem {
public:
    PhysicsSystem(ComponentStore<Transform2D>& transforms, ComponentStore<Rigidbody2D>& rigidbodies, ComponentStore<CircleCollider>& circles, ComponentStore<BoxCollider>& boxes,
                  float fixed_dt = 1.0f / 60.0f)
        : m_transforms(transforms), m_rigidbodies(rigidbodies), m_circles(circles), m_boxes(boxes), m_fixed_dt(fixed_dt),
          m_broadphase(std::make_unique<QuadtreeBroadphase>(&transforms, &circles, &boxes)) {}

    void setBroadphase(std::unique_ptr<Broadphase> bp) { m_broadphase = std::move(bp); }

    void update(float dt) {
        m_accumulator += dt;
        const float maxAccum = m_fixed_dt * 5.0f;
        if (m_accumulator > maxAccum)
            m_accumulator = maxAccum;

        collectActiveEntities();

        while (m_accumulator >= m_fixed_dt) {
            stepFixed(m_fixed_dt);
            m_accumulator -= m_fixed_dt;
        }
    }

private:
    ComponentStore<Transform2D>& m_transforms;
    ComponentStore<Rigidbody2D>& m_rigidbodies;
    ComponentStore<CircleCollider>& m_circles;
    ComponentStore<BoxCollider>& m_boxes;

    float m_fixed_dt;
    float m_accumulator = 0.0f;

    std::unique_ptr<Broadphase> m_broadphase;
    std::vector<Entity> m_active_entities;

    const float m_sleep_velocity_sq_threshold = 0.01f;
    const float m_sleep_time_threshold = 2.0f;

    Vec2D m_gravity = {0.0f, 9.8f};

    void collectActiveEntities() {
        m_active_entities.clear();
        const auto& rb_entities = m_rigidbodies.all_entities();
        for (Entity e : rb_entities) {
            if (m_rigidbodies.has(e) && !m_rigidbodies.get(e)->is_sleeping && m_transforms.has(e) && (m_circles.has(e) || m_boxes.has(e))) {
                m_active_entities.push_back(e);
            }
        }
    }

    void wakeEntity(Entity e) {
        Rigidbody2D* rb = m_rigidbodies.get(e);
        if (rb && rb->is_sleeping) {
            rb->is_sleeping = false;
            rb->sleep_timer = 0.0f;
            // FIXME: should this be added back this frame?
        }
    }

    void stepFixed(float dt) {
        // 1) Integrate forces -> velocity (semi-implicit Euler)
        for (Entity e : m_active_entities) {
            Rigidbody2D* rb = m_rigidbodies.get(e);
            Transform2D* tr = m_transforms.get(e);

            if (rb->is_static || rb->invMass == 0.0f) {
                rb->force = Vec2D::zero;
                continue;
            }

            float mass = 1.0f / rb->invMass;
            rb->force += m_gravity * mass;

            Vec2D accel = rb->force * rb->invMass;
            rb->velocity += accel * dt;
            rb->velocity *= (1.0f / (1.0f + rb->linearDamping * dt));
            tr->position += rb->velocity * dt;
            rb->force = Vec2D::zero;

            if (rb->velocity.MagnitudeSquared() < m_sleep_velocity_sq_threshold) {
                rb->sleep_timer += dt;
                if (rb->sleep_timer > m_sleep_time_threshold) {
                    rb->is_sleeping = true;
                }
            } else {
                rb->sleep_timer = 0.0f; // Reset timer if moving
            }
        }

        m_active_entities.erase(std::remove_if(m_active_entities.begin(), m_active_entities.end(),
                                               [this](Entity e) {
                                                   Rigidbody2D* rb = m_rigidbodies.get(e);
                                                   return rb->is_sleeping;
                                               }),
                                m_active_entities.end());

        // 2) Broadphase generate pairs
        auto pairs = m_broadphase->collectPairs(m_active_entities);

        // 3) Narrow-phase: compute contacts
        std::vector<Manifold> manifolds;
        manifolds.reserve(64);
        for (auto& p : pairs) {
            Entity a = p.first;
            Entity b = p.second;

            Manifold m = computeManifold(a, b);
            if (m.colliding)
                manifolds.push_back(m);
        }

        // 4) Resolve collisions (impulses)
        for (auto& m : manifolds) {
            resolveCollision(m);
        }

        // 5) Positional correction (prevent sinking)
        for (auto& m : manifolds) {
            positionalCorrection(m);
        }
    }

    // Narrow-phase dispatcher
    Manifold computeManifold(Entity a, Entity b) {
        if (m_circles.has(a) && m_circles.has(b))
            return circleVsCircle(a, b);
        if (m_boxes.has(a) && m_boxes.has(b))
            return boxVsBox(a, b);
        if (m_circles.has(a) && m_boxes.has(b))
            return circleVsBox(a, b);
        if (m_boxes.has(a) && m_circles.has(b)) {
            Manifold m = circleVsBox(b, a);
            // reverse normal because circleVsBox assumed circle is first
            m.normal = m.normal * -1.0f;
            std::swap(m.a, m.b);
            return m;
        }
        return Manifold{};
    }

    Manifold circleVsCircle(Entity a, Entity b) {
        Manifold m;
        m.a = a;
        m.b = b;
        Transform2D* ta = m_transforms.get(a);
        Transform2D* tb = m_transforms.get(b);
        CircleCollider* ca = m_circles.get(a);
        CircleCollider* cb = m_circles.get(b);
        if (!ta || !tb || !ca || !cb)
            return m;

        Vec2D pa = ta->position + ca->offset;
        Vec2D pb = tb->position + cb->offset;
        Vec2D n = pb - pa;
        float r = ca->radius + cb->radius;
        float dist2 = n.x * n.x + n.y * n.y;
        if (dist2 > r * r) {
            m.colliding = false;
            return m;
        }

        float dist = std::sqrt(dist2);
        if (dist != 0.0f) {
            m.colliding = true;
            m.penetration = r - dist;
            m.normal = n / dist;
        } else {
            // same position — choose arbitrary normal
            m.colliding = true;
            m.penetration = ca->radius;
            m.normal = Vec2D::right;
        }
        return m;
    }

    // box-box (AABB) using full extents
    Manifold boxVsBox(Entity a, Entity b) {
        Manifold m;
        m.a = a;
        m.b = b;
        Transform2D* ta = m_transforms.get(a);
        Transform2D* tb = m_transforms.get(b);
        BoxCollider* ba = m_boxes.get(a);
        BoxCollider* bb = m_boxes.get(b);
        if (!ta || !tb || !ba || !bb)
            return m;

        // treating size as half-extents
        Vec2D aMin = ta->position + ba->offset - ba->size;
        Vec2D aMax = ta->position + ba->offset + ba->size;
        Vec2D bMin = tb->position + bb->offset - bb->size;
        Vec2D bMax = tb->position + bb->offset + bb->size;

        float overlapX = std::min(aMax.x, bMax.x) - std::max(aMin.x, bMin.x);
        if (overlapX <= 0.0f)
            return m;
        float overlapY = std::min(aMax.y, bMax.y) - std::max(aMin.y, bMin.y);
        if (overlapY <= 0.0f)
            return m;

        m.colliding = true;
        // choose smallest axis for normal
        if (overlapX < overlapY) {
            m.penetration = overlapX;
            // A->B: if A is left (ta.x < tb.x), normal is +X
            m.normal = (ta->position.x < tb->position.x) ? Vec2D::right : Vec2D::left;
        } else {
            m.penetration = overlapY;
            // A->B: if A is above (ta.y < tb.y), normal is +Y (Y-down)
            m.normal = (ta->position.y < tb->position.y) ? Vec2D::down : Vec2D::up;
        }
        return m;
    }

    // circle vs AABB (box)
    Manifold circleVsBox(Entity circleE, Entity boxE) {
        Manifold m;
        m.a = circleE;
        m.b = boxE;
        Transform2D* tc = m_transforms.get(circleE);
        Transform2D* tb = m_transforms.get(boxE);
        CircleCollider* cc = m_circles.get(circleE);
        BoxCollider* bc = m_boxes.get(boxE);
        if (!tc || !tb || !cc || !bc)
            return m;

        // All these values are now in METERS
        Vec2D circlePos = tc->position + cc->offset;
        Vec2D boxPos = tb->position + bc->offset;
        Vec2D half = bc->size;

        // 1. Find the closest point 'closest' on the box to the circle center
        Vec2D min = boxPos - half;
        Vec2D max = boxPos + half;
        Vec2D closest = circlePos.Clamped(min, max); // Clamps circlePos to be within the box bounds
        Vec2D n_outward = circlePos - closest;       // Vector from closest point on box OUT to circle center
        float dist2 = n_outward.MagnitudeSquared();

        if (dist2 > (cc->radius * cc->radius)) {
            m.colliding = false;
            return m; // No collision
        }

        m.colliding = true;
        float dist = std::sqrt(dist2);

        if (dist != 0.0f) {
            // Case 1: Circle center is outside or on the box edge
            m.normal = (n_outward / dist) * -1.0f;
            m.penetration = cc->radius - dist;
        } else {
            // Case 2: Circle center is inside the box (dist == 0)
            float dx = circlePos.x - boxPos.x;
            float dy = circlePos.y - boxPos.y;

            float penetrationX = half.x - std::abs(dx);
            float penetrationY = half.y - std::abs(dy);

            if (penetrationX < penetrationY) {
                m.penetration = penetrationX + cc->radius; // circle edge to box edge
                if (dx > 0)
                    m.normal = Vec2D::right;
                else
                    m.normal = Vec2D::left;
            } else {
                m.penetration = penetrationY + cc->radius;
                if (dy > 0)
                    m.normal = Vec2D::down;
                else
                    m.normal = Vec2D::up;
            }
            // from box center *towards* circle. i.e. B -> A
            m.normal = m.normal * -1.0f;
        }

        return m;
    }

    // Resolve collision using impulse-based resolution
    void resolveCollision(const Manifold& m) {
        Rigidbody2D* A = m_rigidbodies.get(m.a);
        Rigidbody2D* B = m_rigidbodies.get(m.b);
        Transform2D* TA = m_transforms.get(m.a);
        Transform2D* TB = m_transforms.get(m.b);
        if (!A || !B || !TA || !TB)
            return;
        if (!m.colliding)
            return;
        if (A->is_static && B->is_static)
            return;

        // Ensure moving objects wake up sleeping static/kinematic objects
        if (!A->is_static)
            wakeEntity(m.a);
        else
            wakeEntity(m.b);
        if (!B->is_static)
            wakeEntity(m.b);
        else
            wakeEntity(m.a);

        Vec2D rv = B->velocity - A->velocity;
        float velAlongNormal = rv.Dot(m.normal);

        if (velAlongNormal > 0.0f || std::abs(velAlongNormal) < 0.001f)
            return;

        float e = std::min(A->restitution, B->restitution);
        float invMassA = A->invMass;
        float invMassB = B->invMass;
        float j = -(1.0f + e) * velAlongNormal;
        float invMassSum = invMassA + invMassB;
        if (invMassSum == 0.0f)
            return;
        j /= invMassSum;

        Vec2D impulse = m.normal * j;
        if (!A->is_static)
            A->velocity -= impulse * invMassA;
        if (!B->is_static)
            B->velocity += impulse * invMassB;
    }

    // positional correction to avoid sinking
    void positionalCorrection(const Manifold& m) {
        const float percent = 0.25f; // 20% - 80%
        const float slop = 0.05f;    // penetration allowance
        if (m.penetration <= slop)
            return;

        Rigidbody2D* A = m_rigidbodies.get(m.a);
        Rigidbody2D* B = m_rigidbodies.get(m.b);
        Transform2D* TA = m_transforms.get(m.a);
        Transform2D* TB = m_transforms.get(m.b);
        if (!A || !B || !TA || !TB)
            return;

        float invMassA = A->invMass;
        float invMassB = B->invMass;
        float invMassSum = invMassA + invMassB;
        if (invMassSum == 0.0f)
            return;

        Vec2D correction = m.normal * ((std::max(m.penetration - slop, 0.0f) / invMassSum) * percent);

        if (!A->is_static)
            TA->position -= correction * invMassA;
        if (!B->is_static)
            TB->position += correction * invMassB;
    }
};