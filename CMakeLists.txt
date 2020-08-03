cmake_minimum_required(VERSION 3.17)
project(ffmpeg1 C)

set(CMAKE_C_STANDARD 99)
include_directories($ENV{HOME}/ffmpeg_build/include/)
link_directories($ENV{HOME}/ffmpeg_build/lib/)
add_executable(ffmpeg1 main.c)
target_link_libraries(ffmpeg1 avcodec avformat swscale avutil)