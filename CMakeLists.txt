cmake_minimum_required(VERSION 3.12)

project(remote-server)

set(CMAKE_C_STANDARD 99)
set(C_STANDARD_REQUIRED TRUE)

include_directories(server_dir)
add_subdirectory(server_dir)

add_executable(server server_dir/server.c)
target_link_libraries(server server_files)

include_directories(client_dir)

add_executable(client client_dir/client.c)
