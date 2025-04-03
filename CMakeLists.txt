cmake_minimum_required(VERSION 3.5.0)
project(VirtualSoc VERSION 0.1.0 LANGUAGES C CXX)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
include_directories(VirtualSoc ${SDL2_INCLUDE_DIRS})
add_executable(VirtualSoc  imgui/imgui.cpp imgui/imgui_draw.cpp imgui/imgui_demo.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp imgui/imgui_impl_sdl2.cpp imgui/imgui_impl_opengl3.cpp client.cpp)
target_link_libraries(VirtualSoc ${SDL2_LIBRARIES} OpenGL::OpenGL)