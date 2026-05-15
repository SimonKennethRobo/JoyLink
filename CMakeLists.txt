cmake_minimum_required(VERSION 3.16)
project(JoyLink VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
include(ProcessorCount)

function(build_cmake_dependency dep_name git_repo git_tag install_dir)
  set(dep_source_dir "${CMAKE_SOURCE_DIR}/third_party/${dep_name}")
  set(dep_build_dir "${CMAKE_BINARY_DIR}/third_party/${dep_name}-build")

  if(dep_name STREQUAL "cyclonedds")
    set(dep_package_config "${install_dir}/lib/cmake/CycloneDDS/CycloneDDSConfig.cmake")
  elseif(dep_name STREQUAL "cyclonedds-cxx")
    set(dep_package_config "${install_dir}/lib/cmake/CycloneDDS-CXX/CycloneDDS-CXXConfig.cmake")
  endif()

  if(dep_package_config AND EXISTS "${dep_package_config}")
    message(STATUS "Using existing ${dep_name} install at ${install_dir}")
    return()
  endif()

  FetchContent_Declare(
    ${dep_name}
    GIT_REPOSITORY ${git_repo}
    GIT_TAG ${git_tag}
    GIT_SHALLOW TRUE
    SOURCE_DIR ${dep_source_dir}
  )
  FetchContent_Populate(${dep_name})

  ProcessorCount(dep_parallel_jobs)
  if(NOT dep_parallel_jobs)
    set(dep_parallel_jobs 2)
  endif()

  set(dep_configure_args
    -S ${dep_source_dir}
    -B ${dep_build_dir}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX=${install_dir}
    -DBUILD_TESTING=OFF
    -DBUILD_EXAMPLES=OFF
    -DBUILD_DOCS=OFF
  )

  if(dep_name STREQUAL "cyclonedds")
    list(APPEND dep_configure_args
      -DBUILD_DDSPERF=OFF
      -DBUILD_IDLC=ON
    )
  elseif(dep_name STREQUAL "cyclonedds-cxx")
    list(APPEND dep_configure_args
      -DBUILD_IDLLIB=ON
      -DCMAKE_PREFIX_PATH=${CMAKE_SOURCE_DIR}/third_party/cyclonedds/install
    )
  endif()

  message(STATUS "Configuring ${dep_name}")
  execute_process(
    COMMAND ${CMAKE_COMMAND} ${dep_configure_args}
    RESULT_VARIABLE dep_configure_result
  )
  if(NOT dep_configure_result EQUAL 0)
    message(FATAL_ERROR "Failed to configure ${dep_name}")
  endif()

  message(STATUS "Building and installing ${dep_name}")
  execute_process(
    COMMAND ${CMAKE_COMMAND} --build ${dep_build_dir} --target install --parallel ${dep_parallel_jobs}
    RESULT_VARIABLE dep_build_result
  )
  if(NOT dep_build_result EQUAL 0)
    message(FATAL_ERROR "Failed to build/install ${dep_name}")
  endif()
endfunction()

# ── Options ──────────────────────────────────────────────────────────
option(BUILD_ZMQ "Build with ZMQ publisher support" ON)
option(BUILD_ROS2 "Build with ROS2 publisher support" OFF)
option(BUILD_DDS "Build with CycloneDDS publisher support" OFF)
option(FETCH_DDS_DEPS "Fetch CycloneDDS dependencies automatically when BUILD_DDS is ON" ON)
option(FORCE_FETCH_DDS_DEPS "Always build CycloneDDS dependencies from third_party when BUILD_DDS is ON" ON)
set(CYCLONEDDS_VERSION "0.10.5" CACHE STRING "CycloneDDS/CycloneDDS-CXX version to fetch")

# ── Find packages ────────────────────────────────────────────────────
FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG yaml-cpp-0.9.0
)
FetchContent_MakeAvailable(yaml-cpp)


set(HAS_ROS2 OFF)
set(HAS_ZMQ OFF)
set(HAS_DDS OFF)

if(BUILD_ROS2)
  find_package(rclcpp QUIET)
  find_package(sensor_msgs QUIET)
  if(rclcpp_FOUND AND sensor_msgs_FOUND)
    message(STATUS "ROS2 found - enabling ROS2 publisher")
    set(HAS_ROS2 ON)
  else()
    message(STATUS "ROS2 not found - building without ROS2 support")
  endif()
endif()

if(BUILD_ZMQ)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(libzmq IMPORTED_TARGET libzmq)
  endif()

  if(TARGET PkgConfig::libzmq)
    FetchContent_Declare(
      cppzmq
      GIT_REPOSITORY https://github.com/zeromq/cppzmq.git
      GIT_TAG v4.10.0
      GIT_SHALLOW TRUE
    )
    set(CPPZMQ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(cppzmq)
    message(STATUS "cppzmq fetched from GitHub - enabling ZMQ publisher")
    set(HAS_ZMQ ON)
  else()
    message(STATUS "libzmq not found - building without ZMQ support")
  endif()
endif()

if(BUILD_DDS)
  # Prefer an existing installation, but fetch the DDS stack automatically when
  # DDS is requested and no package is already available.
  list(APPEND CMAKE_PREFIX_PATH
    "${CMAKE_SOURCE_DIR}/third_party/cyclonedds/install"
    "${CMAKE_SOURCE_DIR}/third_party/cyclonedds-cxx/install"
  )
  if(NOT FORCE_FETCH_DDS_DEPS)
    find_package(CycloneDDS-CXX QUIET)
  endif()

  if(CycloneDDS-CXX_FOUND)
    message(STATUS "CycloneDDS-CXX found - enabling DDS publisher")
    set(HAS_DDS ON)
  elseif(FETCH_DDS_DEPS)
    message(STATUS "CycloneDDS-CXX not found - fetching CycloneDDS ${CYCLONEDDS_VERSION}")

    build_cmake_dependency(
      cyclonedds
      https://github.com/eclipse-cyclonedds/cyclonedds.git
      ${CYCLONEDDS_VERSION}
      ${CMAKE_SOURCE_DIR}/third_party/cyclonedds/install
    )

    build_cmake_dependency(
      cyclonedds-cxx
      https://github.com/eclipse-cyclonedds/cyclonedds-cxx.git
      ${CYCLONEDDS_VERSION}
      ${CMAKE_SOURCE_DIR}/third_party/cyclonedds-cxx/install
    )

    list(PREPEND CMAKE_PREFIX_PATH
      "${CMAKE_SOURCE_DIR}/third_party/cyclonedds/install"
      "${CMAKE_SOURCE_DIR}/third_party/cyclonedds-cxx/install"
    )
    find_package(CycloneDDS-CXX REQUIRED)
    message(STATUS "CycloneDDS-CXX fetched - enabling DDS publisher")
    set(HAS_DDS ON)
  else()
    message(FATAL_ERROR "BUILD_DDS=ON but CycloneDDS-CXX was not found. Enable FETCH_DDS_DEPS or set CMAKE_PREFIX_PATH.")
  endif()
endif()

if(NOT HAS_ROS2 AND NOT HAS_ZMQ AND NOT HAS_DDS)
  message(FATAL_ERROR "No publisher backend found. Enable at least one: BUILD_ROS2, BUILD_ZMQ, BUILD_DDS.")
endif()

# ── Subdirectories ───────────────────────────────────────────────────
add_subdirectory(common)
add_subdirectory(server)

if(HAS_ZMQ OR HAS_DDS)
  add_subdirectory(client)
endif()

install(DIRECTORY config/ DESTINATION share/joylink/config)
