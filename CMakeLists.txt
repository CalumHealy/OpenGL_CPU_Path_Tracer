cmake_minimum_required(VERSION 3.10)
project(OpenGLProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Disable GLFW extras so "boing" and friends don't build ----
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

# ---- Add your executable ----
add_executable(OpenGLProject
    src/main.cpp
    src/glad.c
)

# ---- GLAD include path ----
target_include_directories(OpenGLProject PRIVATE
    ${CMAKE_SOURCE_DIR}/external/glad/include
    ${CMAKE_SOURCE_DIR}/external/glm
)

# ---- Add GLFW as a subproject (source folder must contain CMakeLists.txt) ----
add_subdirectory(external/glfw)

# ---- Link GLFW + OpenGL ----
target_link_libraries(OpenGLProject PRIVATE glfw opengl32)
