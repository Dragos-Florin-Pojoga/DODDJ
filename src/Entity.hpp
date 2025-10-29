#pragma once

#include "./Commons.hpp"
#include "./Logging.hpp"
#include "./GLOBALS.hpp"

#include <unordered_map>
#include <stack>

struct Entity {
    u16 id;
    constexpr Entity() : id(0) {}
    constexpr explicit Entity(u16 val) : id(val) {}
    constexpr bool operator==(const Entity& other) const { return id == other.id; }
    constexpr bool operator==(u16 other) const { return id == other; }
    constexpr bool operator!=(const Entity& other) const { return id != other.id; }
    constexpr operator u16() const { return id; }
};

namespace std {
    template <>
    struct hash<Entity> {
        size_t operator()(const Entity& e) const {
            return std::hash<u16>{}(e.id);
        }
    };
}

const Entity INVALID_ENTITY = Entity(0);

class EntityManager {
public:
    Entity create() {
        if (!m_free_ids.empty()) {
            const Entity id = m_free_ids.top();
            m_free_ids.pop();
            return id;
        }
        const Entity new_id = m_next_id;
        m_next_id = Entity(m_next_id.id + 1);
        return new_id;
    }

    Entity create(const std::string& name) {
        if (m_name_to_entity.count(name)) {
            Logging::log_critical("Attempted to create entity with duplicate name: ", name);
            return Entity(0);
        }
        
        const Entity new_id = create();
        m_entity_names[new_id] = name;
        m_name_to_entity[name] = new_id;
        return new_id;
    }

    void destroy(Entity id) {
        if (id.id >= m_next_id.id) {
            Logging::log_warning("Attempted to destroy invalid Entity ID: ", id);
            return;
        }

        const auto name_it = m_entity_names.find(id);
        if (name_it != m_entity_names.end()) {
            m_name_to_entity.erase(name_it->second);
            m_entity_names.erase(name_it);
        }

        m_free_ids.push(id);
    }

    std::string getName(Entity id) const {
        const auto it = m_entity_names.find(id);
        if (it != m_entity_names.end()) {
            return it->second;
        }
        return std::to_string(id) + " (unnamed)";
    }

    void setName(Entity id, const std::string& name) {
        if (id >= m_next_id || id == INVALID_ENTITY) {
            Logging::log_critical("Attempted to set name for unmanaged Entity ID.");
            return;
        }
        
        const auto old_name_it = m_entity_names.find(id);
        if (old_name_it != m_entity_names.end()) {
            m_name_to_entity.erase(old_name_it->second);
        }
        
        if (m_name_to_entity.count(name)) {
            Logging::log_critical("Attempted to set duplicate name '", name, "' for Entity: ", id);
            return; 
        }

        m_entity_names[id] = name;
        m_name_to_entity[name] = id;
    }

    const Entity getByName(const std::string& name) const {
        const auto it = m_name_to_entity.find(name);
        if (it != m_name_to_entity.end()) {
            return it->second;
        }
        return INVALID_ENTITY;
    }

private:
    Entity m_next_id = Entity(1);
    std::unordered_map<Entity, std::string> m_entity_names;
    std::unordered_map<std::string, Entity> m_name_to_entity;
    std::stack<Entity> m_free_ids;
    friend std::ostream& operator<<(std::ostream& os, const EntityManager& em);
};