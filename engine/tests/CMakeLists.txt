# Unit test Catch2 v3. Glob tests/unit/**/*.cpp để agent khác thêm test không cần sửa CMake (11 §6).
file(GLOB_RECURSE LE_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp)
# rtsan_selfcheck.cpp CỐ TÌNH vi phạm luật RT → không nằm trong binary test chung.
list(FILTER LE_TEST_SOURCES EXCLUDE REGEX ".*/rtsan_selfcheck\\.cpp$")

add_executable(le-tests ${LE_TEST_SOURCES})
target_include_directories(le-tests PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include
                                            ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(le-tests PRIVATE loopcore Catch2::Catch2WithMain)
# clang upstream (mac-rtsan): TEST_CASE dùng __COUNTER__ → -Wc2y-extensions dưới -Wpedantic của JUCE.
# Đặt ở mức source vì cờ source đứng SAU cờ interface của loopcore (-Wpedantic) trên dòng lệnh.
set_source_files_properties(${LE_TEST_SOURCES} PROPERTIES COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:Clang>:-Wno-c2y-extensions>")
target_compile_definitions(le-tests PRIVATE
  LE_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
  LE_TEST_SCENARIOS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/scenarios"
  LE_TEST_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
add_test(NAME le-unit COMMAND le-tests --colour-mode none)

# P0-11: RTSan phải bắt được malloc trong hàm [[clang::nonblocking]].
# Test PASS khi output có báo cáo của RealtimeSanitizer (tức là sanitizer thật sự đang chạy).
if(LE_RTSAN_SELFCHECK)
  add_executable(le-rtsan-selfcheck unit/rtsan_selfcheck.cpp)
  target_compile_features(le-rtsan-selfcheck PRIVATE cxx_std_20)
  add_test(NAME rtsan-selfcheck COMMAND le-rtsan-selfcheck)
  # abort_on_error=0: trên macOS sanitizer mặc định abort() → CTest coi là crash và bỏ qua regex.
  set_tests_properties(rtsan-selfcheck PROPERTIES
    ENVIRONMENT "RTSAN_OPTIONS=halt_on_error=1:abort_on_error=0"
    PASS_REGULAR_EXPRESSION "RealtimeSanitizer: unsafe-library-call.*malloc")
endif()
