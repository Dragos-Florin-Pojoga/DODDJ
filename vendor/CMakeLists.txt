add_subdirectory(SDL EXCLUDE_FROM_ALL)

add_library(vendor INTERFACE)
target_link_libraries(vendor INTERFACE
    SDL3::SDL3
)