# Công cụ đo P0 (agent 80) — chỉ build trên macOS, bật bằng LE_BUILD_HARNESS (top-level add_subdirectory).
#   le-stretch-bench     P0-09: render 13 zone bằng Signalsmith, in thời gian, ghi WAV để nghe
#   le-latency-selftest  P0-07: kiểm LatencyProbe bằng loopback giả lập; --device: đo thật loa → mic của Mac
#   le-load-bench        P0-08: CPU của LoadGenerator theo số voice (offline, để so với số trên iPad)
#   le-gen-fixtures      P1-28: ghi bộ trống + nhạc cụ tổng hợp ra tests/fixtures/{kit_synth,inst_synth}/
#   le-engine-bench      P1-34/P3-21: thời gian process mỗi block (TB/p50/p99/max %) với kịch bản tải chuẩn
#   le-fuzz-api          P4-16 (chuẩn bị): fuzz C API có seed, kiểm envelope / LeState / job (docs/fuzz-soak.md)
#   le-soak              P1-36 (chuẩn bị): phiên chơi dài ngẫu nhiên offline, kiểm NaN / RSS / file take (docs/fuzz-soak.md)
#   le-gen-kits          P2-30/P2-35: ghi 8 bộ trống 16 pad tổng hợp (FLAC) ra content/kits/ + WAV nghe thử ra tools/out/kit_preview/
#   le-gen-instruments   P2-35: ghi E-Piano (FM) + Organ (drawbar) nhiều zone có loop ra content/instruments/ + nghe thử ra
#                        tools/out/instrument_preview/  (chung phần ghi FLAC / manifest / preview: library_writer.h)
#   le-import-library    P2-35: rút gọn sample thật đã tải (Salamander Grand Piano V3, VSCO-2 CE) → content/instruments/
# Các class nằm trong src/ (được compile vào loopcore), ở đây chỉ có main().

set(LE_TOOLS
  le-stretch-bench    stretch_bench_main.cpp
  le-latency-selftest latency_selftest_main.cpp
  le-load-bench       load_bench_main.cpp
  le-gen-fixtures     gen_fixtures_main.cpp
  le-engine-bench     engine_bench_main.cpp
  le-fuzz-api         fuzz_api_main.cpp
  le-soak             soak_main.cpp
  le-gen-kits         gen_kits_main.cpp
  le-gen-instruments  gen_instruments_main.cpp
  le-import-library   import_library_main.cpp)

while(LE_TOOLS)
  list(POP_FRONT LE_TOOLS tool src)
  add_executable(${tool} ${src})
  target_include_directories(${tool} PRIVATE ${PROJECT_SOURCE_DIR}/src)
  target_link_libraries(${tool} PRIVATE loopcore)
  target_compile_options(${tool} PRIVATE
    $<$<COMPILE_LANG_AND_ID:CXX,AppleClang,Clang>:-Wfunction-effects>)
endwhile()

# Mặc định ghi fixture vào thư mục engine/ của source (không phụ thuộc thư mục đang đứng)
target_compile_definitions(le-gen-fixtures PRIVATE LE_ENGINE_DIR="${PROJECT_SOURCE_DIR}")
target_compile_definitions(le-engine-bench PRIVATE LE_ENGINE_DIR="${PROJECT_SOURCE_DIR}")
target_compile_definitions(le-fuzz-api PRIVATE LE_ENGINE_DIR="${PROJECT_SOURCE_DIR}")
target_compile_definitions(le-soak PRIVATE LE_ENGINE_DIR="${PROJECT_SOURCE_DIR}")
target_compile_definitions(le-gen-kits PRIVATE LE_ENGINE_DIR="${PROJECT_SOURCE_DIR}")
target_compile_definitions(le-gen-instruments PRIVATE LE_ENGINE_DIR="${PROJECT_SOURCE_DIR}")
target_compile_definitions(le-import-library PRIVATE LE_ENGINE_DIR="${PROJECT_SOURCE_DIR}")
if(APPLE)
  target_link_libraries(le-fuzz-api PRIVATE "-framework CoreFoundation")   # CFRunLoop cho --mode device
  target_link_libraries(le-soak PRIVATE "-framework CoreFoundation")
endif()
