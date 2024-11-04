cmake_minimum_required(VERSION 3.15) # FetchContent is available in 3.11+
project(kyros)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_definitions(-DWINCRYPT_USE_SYMBOL_PREFIX)

set(BORINGSSL_VERSION master)
# find_package(boringssl ${BORINGSSL_VERSION} QUIET) # QUIET or REQUIRED
if (NOT boringssl_FOUND) # If there's none, fetch and build boringssl
  include(FetchContent)
  FetchContent_Declare(
    boringssl
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    GIT_REPOSITORY https://github.com/google/boringssl.git
    GIT_TAG ${BORINGSSL_VERSION}
  )
  FetchContent_GetProperties(boringssl)
  if (NOT boringssl_POPULATED) # Have we downloaded boringssl yet?
    set(FETCHCONTENT_QUIET NO)
    FetchContent_Populate(boringssl)
    set(CMAKE_BUILD_TYPE Release)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) # don't build the supplied examples
    add_subdirectory(${boringssl_SOURCE_DIR} ${boringssl_BINARY_DIR})
  endif()
endif()

set(LIBUV_VERSION 1.49.2)
find_package(libuv ${LIBUV_VERSION} QUIET) # QUIET or REQUIRED
if (NOT libuv_FOUND) # If there's none, fetch and build libuv
  include(FetchContent)
  FetchContent_Declare(
    libuv
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL https://github.com/libuv/libuv/archive/refs/tags/v${LIBUV_VERSION}.tar.gz
  )
  FetchContent_GetProperties(libuv)
  if (NOT libuv_POPULATED) # Have we downloaded libuv yet?
    set(FETCHCONTENT_QUIET NO)
    FetchContent_Populate(libuv)
    set(CMAKE_BUILD_TYPE Release)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) # don't build the supplied examples
    add_subdirectory(${libuv_SOURCE_DIR} ${libuv_BINARY_DIR})
  endif()
endif()

set(LSHPACK_VERSION 2.3.4)
find_package(lshpack ${LSHPACK_VERSION} QUIET) # QUIET or REQUIRED
if (NOT libuv_FOUND) # If there's none, fetch and build libuv
  include(FetchContent)
  FetchContent_Declare(
    lshpack
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL https://github.com/litespeedtech/ls-hpack/archive/refs/tags/v${LSHPACK_VERSION}.tar.gz
  )
  FetchContent_GetProperties(lshpack)
  if (NOT lshpack_POPULATED) # Have we downloaded lshpack yet?
    set(FETCHCONTENT_QUIET NO)
    FetchContent_Populate(lshpack)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) # don't build the supplied examples
    set(CMAKE_BUILD_TYPE Release)
    add_subdirectory(${lshpack_SOURCE_DIR} ${lshpack_BINARY_DIR})
    target_include_directories(ls-hpack PUBLIC ${lshpack_SOURCE_DIR})
  endif()
endif()

set(ADA_VERSION 2.9.2)
find_package(ada ${ADA_VERSION} QUIET) # QUIET or REQUIRED
if (NOT ada_FOUND) # If there's none, fetch and build libuv
  include(FetchContent)
  FetchContent_Declare(
    ada
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL https://github.com/ada-url/ada/archive/refs/tags/v${ADA_VERSION}.tar.gz
  )
  FetchContent_GetProperties(ada)
  if (NOT ada_POPULATED) # Have we downloaded lshpack yet?
    set(FETCHCONTENT_QUIET NO)
    FetchContent_Populate(ada)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) # don't build the supplied examples
    set(CMAKE_BUILD_TYPE Release)
    add_subdirectory(${ada_SOURCE_DIR} ${ada_BINARY_DIR})
    set(ADA_TOOLS 0)
    set(ADA_BENCHMARKS 0)
    set(ADA_TESTING 0)
  endif()
endif()


set(MIMALLOC_VERSION 2.1.7)
find_package(mimalloc ${MIMALLOC_VERSION} QUIET) # QUIET or REQUIRED
if (NOT mimalloc_FOUND) # If there's none, fetch and build libuv
  include(FetchContent)
  FetchContent_Declare(
    mimalloc
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL https://github.com/microsoft/mimalloc/archive/refs/tags/v${MIMALLOC_VERSION}.tar.gz
  )
  FetchContent_GetProperties(mimalloc-static)
  if (NOT mimalloc_POPULATED) # Have we downloaded lshpack yet?
    set(FETCHCONTENT_QUIET NO)
    FetchContent_Populate(mimalloc)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) # don't build the supplied examples
    set(CMAKE_BUILD_TYPE Release)
    add_subdirectory(${mimalloc_SOURCE_DIR} ${mimalloc_BINARY_DIR})
  endif()
endif()


set(KYROS_SRC "src")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c23")
FILE(GLOB KYROS_FILES src/*.c src/**/*.c src/*.h, src/**/.h)

add_library(${PROJECT_NAME} ${KYROS_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC ${KYROS_SRC})
target_include_directories(${PROJECT_NAME} PUBLIC ${KYROS_SRC}/include)
target_include_directories(${PROJECT_NAME} PUBLIC "${lshpack_SOURCE_DIR}/include")
target_include_directories(${PROJECT_NAME} PUBLIC "${libuv_SOURCE_DIR}/include")
target_include_directories(${PROJECT_NAME} PUBLIC "${mimalloc_SOURCE_DIR}/include")
target_include_directories(${PROJECT_NAME} PUBLIC "${boringssl_SOURCE_DIR}/include")
target_include_directories(${PROJECT_NAME} PUBLIC "${ada_SOURCE_DIR}/include")
target_link_libraries(${PROJECT_NAME} uv_a)
target_link_libraries(${PROJECT_NAME} crypto)
target_link_libraries(${PROJECT_NAME} ssl)
target_link_libraries(${PROJECT_NAME} ada)
target_link_libraries(${PROJECT_NAME} ls-hpack)
target_link_libraries(${PROJECT_NAME} mimalloc-static)

if (BUILD_TEST EQUAL 1)
  FILE(GLOB TEST_FILES test/*.c)  
  add_executable(test ${TEST_FILES})
  target_include_directories(test PUBLIC ${KYROS_SRC})
  target_link_libraries(test ${PROJECT_NAME})
endif()
