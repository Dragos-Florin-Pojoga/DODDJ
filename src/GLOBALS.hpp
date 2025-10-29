#pragma once

class Scene;
class EntityManager;

static struct {
    EntityManager* current_entity_manager;
    Scene* current_scene;
} GLOBALS;