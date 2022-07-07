cmake_minimum_required(VERSION 3.21)
project(myWebServer)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -lmysqlclient")

add_executable(myWebServer main.cpp timer/timer.cpp timer/timer.h threadpool/thread_pool.cpp threadpool/thread_pool.h http/http_coon.cpp http/http_coon.h server/server.cpp server/server.h sql/sqlConnectionPool.cpp sql/sqlConnectionPool.h)
