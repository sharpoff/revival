# glfw
add_subdirectory(glfw)

# volk
add_subdirectory(volk)

# stb
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/stb")

# glm
add_subdirectory(glm)

# cgltf
add_library(cgltf INTERFACE)
target_include_directories(cgltf INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/cgltf")

# Dear ImGUI
file(GLOB_RECURSE IMGUI_SRC "${CMAKE_CURRENT_SOURCE_DIR}/imgui/*.cpp")
file(GLOB_RECURSE IMGUI_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/imgui/*.h")
add_library(imgui STATIC "${IMGUI_SRC}" "${IMGUI_HEADERS}")
target_link_libraries(imgui PUBLIC volk)
target_include_directories(imgui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/imgui")
