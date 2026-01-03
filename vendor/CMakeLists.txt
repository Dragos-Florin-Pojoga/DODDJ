# SDL3
add_subdirectory(SDL EXCLUDE_FROM_ALL)
# -- SDL3

# Dear ImGui
add_library(imgui
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    
    imgui/backends/imgui_impl_sdl3.cpp
    imgui/backends/imgui_impl_sdlrenderer3.cpp # TODO: SWAP WHOLE RENDER CHAIN TO SDL3_GPU :/
)

target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
)

target_link_libraries(imgui PUBLIC SDL3::SDL3)
# -- Dear ImGui

# Box2D
add_subdirectory(box2d EXCLUDE_FROM_ALL)
# -- Box2D

add_library(vendor INTERFACE)
target_link_libraries(vendor INTERFACE
    SDL3::SDL3
    imgui
    box2d
)