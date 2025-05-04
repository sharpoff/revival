cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 17)
project(Playground LANGUAGES CXX)

file(GLOB_RECURSE SOURCE_FILES "src/*.cpp")
file(GLOB_RECURSE HEADER_FILES "src/*.h")
file(GLOB_RECURSE GLSL_SOURCE_FILES "shaders/*.vert" "shaders/*.frag" "shaders/*.tesc" "shaders/*.tese")

add_compile_options("$<$<CONFIG:DEBUG>:-Wall>")

add_subdirectory(external)

add_library(playground
    STATIC
    ${SOURCE_FILES}
    ${HEADER_FILES}
)

target_include_directories(playground PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(playground
    PUBLIC

    vulkan
    glfw
    volk
    stb
    glm
    cgltf
    imgui
)

# https://gist.github.com/evilactually/a0d191701cb48f157b05be7f74d79396
set(GLSL_VALIDATOR "glslangValidator")
foreach(GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/"
        COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(shaders DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(playground shaders)

add_executable(main
    src/main.cpp
)

target_link_libraries(main
    PUBLIC
    playground
)
