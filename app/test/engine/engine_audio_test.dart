// P0-06: quyền mic trước le_audio_start. P2-03: xuống nền khi không phát → le_audio_stop, lên lại → start.
import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/engine/engine_audio.dart';

import '../test_utils.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  late FakeEngineClient fake;
  late EngineAudio audio;

  setUp(() {
    fake = createFakeEngine();
    audio = EngineAudio(fake, const EnginePlatform());
    addTearDown(audio.dispose);
  });

  group('start', () {
    test('đã có quyền → không hỏi lại, start ngay', () async {
      final calls = mockEnginePlatform(permission: 'granted');
      final r = await audio.start();
      expect(r.ok, isTrue);
      expect(calls, ['micPermission']);
      expect(fake.audioStartCount, 1);
      expect(audio.isRunning, isTrue);
    });

    test('chưa hỏi → hiện hộp thoại hệ thống, được cấp → start', () async {
      final calls = mockEnginePlatform(permission: 'undetermined', grantOnRequest: true);
      expect((await audio.start()).ok, isTrue);
      expect(calls, ['micPermission', 'requestMicPermission']);
    });

    test('chưa hỏi → người dùng từ chối → micDenied, KHÔNG gọi le_audio_start', () async {
      mockEnginePlatform(permission: 'undetermined', grantOnRequest: false);
      final r = await audio.start();
      expect(r.status, AudioStartStatus.micDenied);
      expect(fake.audioStartCount, 0);
      expect(audio.isRunning, isFalse);
    });

    test('đã từ chối từ trước → micDenied, không hỏi lại', () async {
      final calls = mockEnginePlatform(permission: 'denied');
      expect((await audio.start()).status, AudioStartStatus.micDenied);
      expect(calls, ['micPermission']);
      expect(fake.audioStartCount, 0);
    });

    test('le_audio_start lỗi → engineError kèm mã', () async {
      mockEnginePlatform();
      fake.audioStartResult = LeError.LE_ERR_AUDIO_DEVICE;
      final r = await audio.start();
      expect(r.status, AudioStartStatus.engineError);
      expect(r.errorCode, LeError.LE_ERR_AUDIO_DEVICE);
      expect(audio.isRunning, isFalse);
    });

    test('không có plugin Swift (không phải iOS) → micDenied, không crash', () async {
      expect((await audio.start()).status, AudioStartStatus.micDenied);
      expect(fake.audioStartCount, 0);
    });
  });

  group('vòng đời (P2-03)', () {
    setUp(() => mockEnginePlatform());

    test('xuống nền khi không phát → stop; lên lại → start', () async {
      await audio.start();
      audio.onBackground(engineBusy: false);
      expect(fake.audioStopCount, 1);
      expect(audio.isRunning, isFalse);
      audio.onForeground();
      expect(fake.audioStartCount, 2);
      expect(audio.isRunning, isTrue);
    });

    test('đang phát/thu → giữ audio khi xuống nền', () async {
      await audio.start();
      audio.onBackground(engineBusy: true);
      expect(fake.audioStopCount, 0);
      expect(audio.isRunning, isTrue);
      audio.onForeground();
      expect(fake.audioStartCount, 1, reason: 'không start lại khi chưa từng stop');
    });

    test('keepRunningInBackground (màn spike) → giữ audio', () async {
      await audio.start();
      audio.keepRunningInBackground = true;
      audio.onBackground(engineBusy: false);
      expect(audio.isRunning, isTrue);
    });

    test('người dùng tự stop → lên lại KHÔNG tự bật', () async {
      await audio.start();
      audio.stop();
      audio.onBackground(engineBusy: false);
      audio.onForeground();
      expect(fake.audioStartCount, 1);
      expect(audio.isRunning, isFalse);
    });
  });
}
