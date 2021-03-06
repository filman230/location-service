cmake_minimum_required(VERSION 2.8)

project(connectivity)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

find_package(GLog)
find_package(PkgConfig)
find_package(Threads)

pkg_check_modules(
  DBUS_CPP 
  dbus-cpp REQUIRED)

pkg_check_modules(
  PROCESS_CPP 
  process-cpp REQUIRED)

pkg_check_modules(
  LOCATION_CONNECTIVITY 
  ubuntu-location-service-connectivity REQUIRED)

include_directories(
  ${DBUS_CPP_INCLUDE_DIRS}
  ${GLog_INCLUDE_DIR}
  ${PROCESS_CPP_INCLUDE_DIRS}
  ${LOCATION_CONNECTIVITY_INCLUDE_DIRS})

add_executable(
  connectivity

  connectivity.cpp)

target_link_libraries(
  connectivity

  ${CMAKE_THREAD_LIBS_INIT}

  ${DBUS_CPP_LDFLAGS}
  ${GLog_LIBRARY}
  ${PROCESS_CPP_LDFLAGS}
  ${LOCATION_CONNECTIVITY_LDFLAGS})


