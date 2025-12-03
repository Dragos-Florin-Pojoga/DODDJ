#pragma once

#include <typeinfo>
#include <unordered_map>
#include <vector>

#include "Commons.hpp"
#include "Entity.hpp"
#include "Logging.hpp"

template <typename T>
class ComponentStore {
public:
    void add(Entity e, const T& comp) {
        if (has(e)) {
            Logging::log_warning("Entity '", e, "' already has component '", typeid(T).name(), '\'');
            return;
        }
        const size_t new_index = m_data.size();
        m_data.push_back(comp);
        m_entity_to_index[e] = new_index;
        m_index_to_entity.push_back(e);
    }

    void remove(Entity e) {
        if (!has(e)) {
            Logging::log_warning("Trying to remove component '", typeid(T).name(), "' from Entity '", e, "' which does not have it");
            return;
        }

        const size_t removed_index = m_entity_to_index[e];
        const size_t last_index = m_data.size() - 1;

        if (removed_index != last_index) {
            const Entity last_entity = m_index_to_entity[last_index];
            m_data[removed_index] = m_data[last_index];
            m_index_to_entity[removed_index] = last_entity;
            m_entity_to_index[last_entity] = removed_index;
        }

        m_data.pop_back();
        m_index_to_entity.pop_back();
        m_entity_to_index.erase(e);
    }

    const std::vector<T>& all() const { return m_data; }
    const std::vector<Entity>& all_entities() const { return m_index_to_entity; }

    bool has(Entity e) const { return m_entity_to_index.find(e) != m_entity_to_index.end(); }

    // TODO: many getters can be sure the component exists
    T* get(Entity e) {
        auto it = m_entity_to_index.find(e);
        if (it == m_entity_to_index.end()) {
            Logging::log_warning("Entity '", e, "' does not have requested component '", typeid(T).name(), '\'');
            return nullptr;
        }
        return &m_data[it->second];
    }

    const T* get(Entity e) const {
        auto it = m_entity_to_index.find(e);
        if (it == m_entity_to_index.end()) {
            Logging::log_warning("Entity '", e, "' does not have requested component '", typeid(T).name(), '\'');
            return nullptr;
        }
        return &m_data[it->second];
    }

private:
    std::vector<T> m_data;
    std::unordered_map<Entity, size_t> m_entity_to_index;
    std::vector<Entity> m_index_to_entity;
};