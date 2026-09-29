// P0-11: kiểm tra RTSan thật sự đang bật.
// Hàm [[clang::nonblocking]] gọi malloc → RTSan PHẢI dừng chương trình với
// "RealtimeSanitizer: unsafe-library-call ... malloc". CTest chỉ coi là PASS khi thấy dòng đó
// (tests/CMakeLists.txt). Không nằm trong le-tests; chỉ build khi LE_RTSAN_SELFCHECK=ON (preset mac-rtsan).
#include <cstdio>
#include <cstdlib>

#pragma clang diagnostic ignored "-Wfunction-effects"   // cố tình vi phạm

[[gnu::noinline]] static void* allocateInRealtime(std::size_t n) noexcept [[clang::nonblocking]] {
    return std::malloc(n);
}

int main() {
    void* volatile p = allocateInRealtime(64);
    std::printf("SELFCHECK FAILED: RTSan không bắt được malloc trong hàm nonblocking (sanitizer có bật không?)\n");
    std::free(p);
    return 1;
}
