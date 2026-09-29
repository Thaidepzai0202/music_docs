cmake_minimum_required(VERSION 3.25)
project(LoopCore VERSION 0.1.0 LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LE_BUILD_HARNESS    "Build harness + tools (macOS)" ON)   # tắt khi build iOS
option(LE_BUILD_TESTS      "Build tests" ON)
option(LE_RTSAN_SELFCHECK  "Build le-rtsan-selfcheck (cố tình malloc trong hàm nonblocking)" OFF)
option(LE_ENABLE_SIM       "Op sim.offline / sim.advance cho test (05 §3). iOS: OFF" ON)

find_program(LE_CCACHE ccache)
if(LE_CCACHE AND NOT CMAKE_GENERATOR STREQUAL "Xcode")
  foreach(lang C CXX OBJC OBJCXX)
    set(CMAKE_${lang}_COMPILER_LAUNCHER ${LE_CCACHE})
  endforeach()
endif()

# ─────────────────────────── Thư viện bên thứ ba ───────────────────────────
# Phiên bản đã pin: third_party/VERSIONS.md
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(JUCE_MODULES_ONLY ON)   # không build juceaide lúc configure (ta không dùng juce_add_* helper)
add_subdirectory(third_party/JUCE EXCLUDE_FROM_ALL)          # chỉ dùng module, không dùng juce_add_gui_app
add_subdirectory(third_party/signalsmith-linear EXCLUDE_FROM_ALL)   # phải đứng trước stretch (tránh FetchContent)
add_subdirectory(third_party/signalsmith-stretch EXCLUDE_FROM_ALL)

add_library(le_spsc INTERFACE)
target_include_directories(le_spsc SYSTEM INTERFACE third_party/SPSCQueue/include)

# ─────────────────────────── le_juce: các module JUCE, compile đúng 1 lần ───────────────────────────
# Module JUCE là INTERFACE library: ai link nó cũng compile lại nguồn của nó. Vì vậy gom vào 1 static lib
# (link PRIVATE), rồi chỉ lộ include dir + define ra ngoài (mẫu "shared code" trong JUCE/docs/CMake API.md).
add_library(le_juce STATIC)
target_link_libraries(le_juce
  PRIVATE juce::juce_core juce::juce_events juce::juce_audio_basics
          juce::juce_audio_devices juce::juce_audio_formats juce::juce_dsp
  PUBLIC  juce::juce_recommended_config_flags)
target_compile_definitions(le_juce
  PUBLIC
    JUCE_STANDALONE_APPLICATION=0
    JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0
    JUCE_SILENCE_XCODE_15_LINKER_WARNING=1
  INTERFACE
    $<TARGET_PROPERTY:le_juce,COMPILE_DEFINITIONS>)
target_include_directories(le_juce SYSTEM
  INTERFACE $<TARGET_PROPERTY:le_juce,INCLUDE_DIRECTORIES>)
set_target_properties(le_juce PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden
  OBJC_VISIBILITY_PRESET hidden OBJCXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON)

# ─────────────────────────── loopcore ───────────────────────────
# Glob để agent khác thêm file vào src/ mà không phải sửa CMake (11 §6).
file(GLOB_RECURSE LE_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)
if(APPLE)
  file(GLOB_RECURSE LE_OBJC_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.mm)
  list(APPEND LE_SOURCES ${LE_OBJC_SOURCES})
  set_source_files_properties(${LE_OBJC_SOURCES} PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
endif()

add_library(loopcore STATIC ${LE_SOURCES})
target_include_directories(loopcore
  PUBLIC  include
  PRIVATE src)
target_link_libraries(loopcore
  PUBLIC le_juce le_spsc signalsmith-stretch signalsmith-linear
         juce::juce_recommended_warning_flags)
if(APPLE)
  target_link_libraries(loopcore PUBLIC "-framework AVFoundation" "-framework Foundation")
endif()
target_compile_options(loopcore PRIVATE
  $<$<COMPILE_LANG_AND_ID:CXX,AppleClang,Clang>:-Wfunction-effects -Werror=function-effects>)   # R: vi phạm RT = lỗi build
target_compile_definitions(loopcore PRIVATE LE_ENABLE_SIM=$<BOOL:${LE_ENABLE_SIM}>)
set_target_properties(loopcore PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  CXX_VISIBILITY_PRESET hidden OBJCXX_VISIBILITY_PRESET hidden   # chỉ symbol LE_EXPORT lộ ra
  VISIBILITY_INLINES_HIDDEN ON)

# Signalsmith ở Debug chậm ~10×: các file render/đo chạy -O2 ở mọi config.
file(GLOB LE_HOT_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/spike/measure/StretchBench.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/render/*Renderer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/render/Yin.cpp)
if(LE_HOT_SOURCES)
  set_source_files_properties(${LE_HOT_SOURCES} PROPERTIES COMPILE_OPTIONS "-O2")
endif()

# ─────────────────────────── harness / tools / tests ───────────────────────────
if(LE_BUILD_HARNESS)
  # libLoopCore.dylib (chỉ macOS): cho test hợp đồng Dart chạy với engine thật (DynamicLibrary.open).
  # -force_load kéo toàn bộ object; chỉ symbol LE_EXPORT (le_*) được export vì phần còn lại visibility hidden.
  if(APPLE)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/loopcore_dylib_stub.cpp "// trống: toàn bộ code đến từ -force_load\n")
    add_library(loopcore_dylib SHARED ${CMAKE_CURRENT_BINARY_DIR}/loopcore_dylib_stub.cpp)
    add_dependencies(loopcore_dylib loopcore le_juce)
    target_link_libraries(loopcore_dylib PRIVATE
      "-Wl,-force_load,$<TARGET_FILE:loopcore>" "-Wl,-force_load,$<TARGET_FILE:le_juce>"
      loopcore)   # lấy framework + usage requirement; object đã có từ force_load nên không trùng
    target_link_options(loopcore_dylib PRIVATE "LINKER:-exported_symbol,_le_*")   # chỉ export C API
    set_target_properties(loopcore_dylib PROPERTIES OUTPUT_NAME LoopCore)
  endif()
  add_subdirectory(harness)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tools/CMakeLists.txt)
    add_subdirectory(tools)
  endif()
endif()

if(LE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(third_party/Catch2 EXCLUDE_FROM_ALL)
  add_subdirectory(tests)
endif()
