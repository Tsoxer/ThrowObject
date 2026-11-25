cmake_minimum_required(VERSION 3.25)
project(ThrowObject VERSION 0.0.15 LANGUAGES CXX)

include(CTest)
if(BUILD_TESTING)
  add_executable(throw_object_tests tests/core.cpp)
  target_compile_features(throw_object_tests PRIVATE cxx_std_20)
  target_include_directories(throw_object_tests PRIVATE src)
  add_test(NAME core COMMAND throw_object_tests)
  add_executable(throw_object_carry_visual_tests tests/carry_visual.cpp)
  target_compile_features(throw_object_carry_visual_tests PRIVATE cxx_std_20)
  target_include_directories(throw_object_carry_visual_tests PRIVATE src)
  add_test(NAME carry_visual COMMAND throw_object_carry_visual_tests)
  find_package(Threads REQUIRED)
  add_executable(throw_object_input_tests tests/gameplay_input.cpp)
  target_compile_features(throw_object_input_tests PRIVATE cxx_std_20)
  target_include_directories(throw_object_input_tests PRIVATE src)
  target_link_libraries(throw_object_input_tests PRIVATE Threads::Threads)
  add_test(NAME gameplay_input COMMAND throw_object_input_tests)
endif()
if(NOT WIN32)
  return()
endif()

include(FetchContent)
set(SPDLOG_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_Declare(spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG 47584c1d8106f06560cf9f7558b64f3b5e5b88e7)
FetchContent_MakeAvailable(spdlog)
# Expose the dependency target to CommonLib's find_package in the same build.
set(CMAKE_FIND_PACKAGE_REDIRECTS_DIR "${CMAKE_BINARY_DIR}/CMakeFiles/pkgRedirects")
file(WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/spdlog-config.cmake"
  "set(spdlog_FOUND TRUE)\n")

FetchContent_Declare(rapidcsv
  GIT_REPOSITORY https://github.com/d99kris/rapidcsv.git
  GIT_TAG 2114a1aa0fc29c152717d4660d885a9dc42c10a3 SOURCE_SUBDIR no-cmake)
FetchContent_Declare(commonlib
  GIT_REPOSITORY https://github.com/CharmedBaryon/CommonLibSSE-NG.git
  GIT_TAG b93280e832f263dbef44e44cbe2936622a02f91a)
FetchContent_Declare(menu
  GIT_REPOSITORY https://github.com/QTR-Modding/SKSE-Menu-Framework-3.git
  GIT_TAG 59a14ce192d63022e9174adf3da8c317b5f3091e SOURCE_SUBDIR no-cmake)
FetchContent_MakeAvailable(rapidcsv menu)
set(RAPIDCSV_INCLUDE_DIRS "${rapidcsv_SOURCE_DIR}/src" CACHE PATH "" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_SE OFF CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(commonlib)
include(cmake/CommonLibClang.cmake)

add_library(ThrowObject SHARED src/plugin.cpp src/carry_visual.cpp src/config.cpp src/menu.cpp src/physics.cpp src/tolerance.cpp)
target_compile_features(ThrowObject PRIVATE cxx_std_23)
target_link_libraries(ThrowObject PRIVATE CommonLibSSE::CommonLibSSE)
target_include_directories(ThrowObject PRIVATE "${menu_SOURCE_DIR}/resources")
target_precompile_headers(ThrowObject PRIVATE <RE/Skyrim.h> <SKSE/SKSE.h>)
if(MSVC)
  target_compile_options(ThrowObject PRIVATE /Z7)
  target_link_options(ThrowObject PRIVATE /DEBUG:FULL /OPT:REF /OPT:ICF /INCREMENTAL:NO)
endif()
install(TARGETS ThrowObject RUNTIME DESTINATION SKSE/Plugins COMPONENT ThrowObjectRuntime)
