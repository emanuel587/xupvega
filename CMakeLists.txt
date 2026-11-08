cmake_minimum_required(VERSION 3.19)
project(xupernative LANGUAGES C CXX)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build for Vega architecture, not host Ubuntu. Do not link /usr/lib
# libraries from Ubuntu: those cannot be loaded inside Fire TV.
include(FetchContent)
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "" FORCE)
set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "" FORCE)
set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(mbedtls
  GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
  GIT_TAG mbedtls-3.6.7
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(mbedtls)
# curl's FindMbedTLS.cmake accepts explicit include+library values. The
# libraries here are in-project cross-compiled CMake targets.
set(MBEDTLS_INCLUDE_DIR "${mbedtls_SOURCE_DIR}/include" CACHE PATH "" FORCE)
set(MBEDTLS_LIBRARY mbedtls CACHE STRING "" FORCE)
set(MBEDX509_LIBRARY mbedx509 CACHE STRING "" FORCE)
set(MBEDCRYPTO_LIBRARY mbedcrypto CACHE STRING "" FORCE)
set(CURL_USE_PKGCONFIG OFF CACHE BOOL "" FORCE)
set(CURL_USE_MBEDTLS ON CACHE BOOL "" FORCE)
set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBSSH2 OFF CACHE BOOL "" FORCE)
set(CURL_USE_NGHTTP2 OFF CACHE BOOL "" FORCE)
set(CURL_USE_NGTCP2 OFF CACHE BOOL "" FORCE)
set(CURL_USE_QUICHE OFF CACHE BOOL "" FORCE)
set(CURL_BROTLI OFF CACHE STRING "" FORCE)
set(CURL_ZLIB OFF CACHE STRING "" FORCE)
set(CURL_ZSTD OFF CACHE STRING "" FORCE)
set(CURL_DISABLE_LDAP ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_LDAPS ON CACHE BOOL "" FORCE)
set(HTTP_ONLY ON CACHE BOOL "" FORCE)
FetchContent_Declare(curl
  GIT_REPOSITORY https://github.com/curl/curl.git
  GIT_TAG curl-8_14_1
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(curl)

find_package(turbomoduleAPI CONFIG REQUIRED)
find_package(Threads REQUIRED)
file(GLOB XUPER_CODEGEN_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/kepler/turbo-modules/generated/*.cpp")
file(GLOB XUPER_IMPL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/kepler/turbo-modules/*.cpp")
if(NOT XUPER_CODEGEN_SOURCES OR NOT XUPER_IMPL_SOURCES)
  message(FATAL_ERROR "No se ejecutó el codegen de Turbo Module. Ejecuta PREPARAR-Y-COMPILAR.sh")
endif()
set(XUPER_SOURCES
    ${XUPER_CODEGEN_SOURCES}
    ${XUPER_IMPL_SOURCES}
    kepler/AutoLinkInit.cpp
    native-service/gateway.cpp
    native-service/on_device.cpp
    native-service/service.cpp
    native-service/codec.cpp
    native-service/ranger_codec.cpp
    native-service/catalog.cpp
    native-service/trace.cpp)
kepler_add_turbo_module_library(xupernative ${XUPER_SOURCES})
target_compile_definitions(xupernative PRIVATE XUPER_BRIDGE XUPER_VEGA)
target_include_directories(xupernative PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/native-service"
  "${mbedtls_SOURCE_DIR}/include" "${curl_SOURCE_DIR}/include")
target_link_libraries(xupernative PRIVATE CURL::libcurl mbedtls mbedx509 mbedcrypto Threads::Threads)
install(TARGETS xupernative)
