#pragma once

#include <atomic>
#include "./Commons.hpp"

// Simulation Control
inline std::atomic<bool> g_sim_running{false};
inline std::atomic<bool> g_fixed_steps_mode{false};
inline std::atomic<i32> g_steps_remaining{0};
inline std::atomic<u32> g_sim_step_count{0};

// Simulation Stats
inline std::atomic<f32> g_sim_sps{0.0f};
inline std::atomic<i32> g_rigidbody_count{0};
inline std::atomic<i32> g_static_mesh_count{0};
inline std::atomic<i32> g_stat_mesh_ms{0};
inline std::atomic<i32> g_stat_update_ms{0};
inline std::atomic<i32> g_stat_debris_count{0};
inline std::atomic<i32> g_stat_chains{0};
