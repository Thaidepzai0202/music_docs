// SINH TỰ ĐỘNG bởi scripts/gen_bindings.sh từ engine/include/le/engine_api.h — KHÔNG sửa tay.
//
// Tham chiếu mọi hàm le_* để linker không bỏ chúng khỏi libloopcore.a (09 §5).
// Khai báo chỉ để lấy địa chỉ (không include header, tránh phụ thuộc đường dẫn header trong pod).
// Chỉ được biên dịch khi có LoopCore.xcframework (xem engine_ffi.podspec).

extern void le_api_version(void);
extern void le_create(void);
extern void le_destroy(void);
extern void le_audio_start(void);
extern void le_audio_stop(void);
extern void le_send(void);
extern void le_call(void);
extern void le_free_string(void);
extern void le_read_state(void);
extern void le_set_event_callback(void);
extern void le_get_peaks(void);

__attribute__((used, visibility("hidden")))
void *const loopcore_keep_alive[] = {
    (void *)&le_api_version,
    (void *)&le_create,
    (void *)&le_destroy,
    (void *)&le_audio_start,
    (void *)&le_audio_stop,
    (void *)&le_send,
    (void *)&le_call,
    (void *)&le_free_string,
    (void *)&le_read_state,
    (void *)&le_set_event_callback,
    (void *)&le_get_peaks,
};
