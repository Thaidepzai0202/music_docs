import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'engine_audio.dart';
import 'engine_bootstrap.dart';
import 'engine_state_ticker.dart';
import 'job_tracker.dart';

/// Override trong `main()` (hoặc trong test) bằng kết quả [bootstrapEngine].
final engineBootstrapProvider = Provider<EngineBootstrap>(
  (ref) => throw UnimplementedError('engineBootstrapProvider phải được override trong main()'),
);

/// Engine duy nhất của app. Widget không gọi trực tiếp, trừ đường nóng (07 §5).
final engineProvider = Provider<EngineApi>((ref) => ref.watch(engineBootstrapProvider).engine);

/// Kênh Swift của plugin (quyền mic, mở Settings).
final enginePlatformProvider = Provider<EnginePlatform>((ref) => const EnginePlatform());

final jobTrackerProvider = Provider<JobTracker>((ref) {
  final t = JobTracker(ref.watch(engineProvider));
  ref.onDispose(t.dispose);
  return t;
});

final engineAudioProvider = Provider<EngineAudio>((ref) {
  final a = EngineAudio(ref.watch(engineProvider), ref.watch(enginePlatformProvider));
  ref.onDispose(a.dispose);
  return a;
});

/// false khi quyền mic ĐÃ BỊ TỪ CHỐI (chế độ chỉ phát) → khoá các nút thu audio (07 §4.0). Chưa hỏi quyền thì
/// vẫn bấm được: bấm → [EngineAudio.ensureMic] hỏi lần đầu. Thu MIDI trên track instrument không cần mic.
final canRecordAudioProvider = Provider<bool>((ref) {
  final audio = ref.watch(engineAudioProvider);
  void changed() => ref.invalidateSelf();
  audio.addListener(changed);
  ref.onDispose(() => audio.removeListener(changed));
  return !audio.outputOnly || audio.micPermission == MicPermission.undetermined;
});

/// Lấy instance qua Riverpod, nhưng widget nghe trực tiếp (Listenable / CustomPainter.repaint),
/// KHÔNG `ref.watch` dữ liệu 60Hz (07 §5–6).
final engineStateTickerProvider = Provider<EngineStateTicker>((ref) {
  final t = EngineStateTicker(ref.watch(engineProvider));
  ref.onDispose(t.dispose);
  return t;
});
