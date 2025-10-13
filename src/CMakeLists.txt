add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE
    main.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE vendor)

if(CMAKE_BUILD_TYPE STREQUAL "Dist" AND CMAKE_STRIP)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_STRIP} $<TARGET_FILE:${PROJECT_NAME}>
    )
    message(STATUS "Stripping symbols from distribution binary")
endif()