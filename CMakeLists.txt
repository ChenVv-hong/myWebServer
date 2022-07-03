cmake_minimum_required(VERSION 3.21)
project(myWebServer)

set(CMAKE_CXX_STANDARD 14)

add_executable(myWebServer main.cpp timer/timer.cpp timer/timer.h threadpool/thread_pool.cpp threadpool/thread_pool.h http/http_coon.cpp http/http_coon.h)
