# Công cụ đo P0 (agent 80) — chỉ build trên macOS, bật bằng LE_BUILD_HARNESS (top-level add_subdirectory).
#   le-stretch-bench     P0-09: render 13 zone bằng Signalsmith, in thời gian, ghi WAV để nghe
#   le-latency-selftest  P0-07: kiểm LatencyProbe bằng loopback giả lập; --device: đo thật loa → mic của Mac
#   le-load-bench        P0-08: CPU của LoadGenerator theo số voice (offline, để so với số trên iPad)
# Các class nằm trong src/spike/measure/ (được compile vào loopcore), ở đây chỉ có main().

set(LE_TOOLS
  le-stretch-bench    stretch_bench_main.cpp
  le-latency-selftest latency_selftest_main.cpp
  le-load-bench       load_bench_main.cpp)

while(LE_TOOLS)
  list(POP_FRONT LE_TOOLS tool src)
  add_executable(${tool} ${src})
  target_include_directories(${tool} PRIVATE ${PROJECT_SOURCE_DIR}/src)
  target_link_libraries(${tool} PRIVATE loopcore)
  target_compile_options(${tool} PRIVATE
    $<$<COMPILE_LANG_AND_ID:CXX,AppleClang,Clang>:-Wfunction-effects>)
endwhile()
