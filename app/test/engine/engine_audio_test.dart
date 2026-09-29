// 07 §4.0 (chốt 29/09): start() KHÔNG hỏi quyền mic — chưa có quyền → chế độ chỉ phát (audio.setInputEnabled
// false); ensureMic() hỏi lần đầu khi cần thu, được cấp → bật input. Lên lại app mà đã có quyền → bật lại input. P2-03: xuống nền khi không phát → le_audio_stop, lên lại → start.
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

    test('chưa hỏi → start KHÔNG hiện hộp thoại: chỉ phát; ensureMic hỏi lần đầu, được cấp → bật input', () async {
      final calls = mockEnginePlatform(permission: 'undetermined', grantOnRequest: true);
      final r = await audio.start();
      expect(r.status, AudioStartStatus.outputOnly);
      expect(calls, ['micPermission'], reason: 'không hỏi khi chỉ mở app / project');
      expect(audio.micPermission, MicPermission.undetermined);
      expect(await audio.ensureMic(), isTrue);
      expect(calls, ['micPermission', 'micPermission', 'requestMicPermission']);
      expect(fake.inputEnabled, isTrue);
      expect(audio.canRecordNow, isTrue);
      expect(await audio.ensureMic(), isTrue, reason: 'lần sau không hỏi lại');
      expect(calls.where((c) => c == 'requestMicPermission').length, 1);
    });

    test('ensureMic: người dùng từ chối → false, vẫn chỉ phát, quyền = denied', () async {
      mockEnginePlatform(permission: 'undetermined', grantOnRequest: false);
      await audio.start();
      expect(await audio.ensureMic(), isFalse);
      expect(audio.micPermission, MicPermission.denied);
      expect(audio.outputOnly, isTrue);
    });

    test('chưa hỏi → outputOnly: tắt input TRƯỚC rồi mới le_audio_start', () async {
      mockEnginePlatform(permission: 'undetermined', grantOnRequest: false);
      final r = await audio.start();
      expect(r.status, AudioStartStatus.outputOnly);
      expect(r.ok, isTrue);
      expect(r.canRecord, isFalse);
      expect(fake.calls.single.request, {'op': 'audio.setInputEnabled', 'enabled': false});
      expect(fake.inputEnabled, isFalse);
      expect(fake.audioStartCount, 1);
      expect(audio.isRunning, isTrue);
      expect(audio.outputOnly, isTrue);
    });

    test('đã từ chối từ trước → outputOnly, không hỏi lại', () async {
      final calls = mockEnginePlatform(permission: 'denied');
      expect((await audio.start()).status, AudioStartStatus.outputOnly);
      expect(calls, ['micPermission']);
      expect(fake.audioStartCount, 1);
    });

    test('có quyền → không gọi audio.setInputEnabled', () async {
      mockEnginePlatform();
      expect((await audio.start()).canRecord, isTrue);
      expect(fake.calls, isEmpty);
      expect(audio.outputOnly, isFalse);
    });

    test('le_audio_start lỗi → engineError kèm mã', () async {
      mockEnginePlatform();
      fake.audioStartResult = LeError.LE_ERR_AUDIO_DEVICE;
      final r = await audio.start();
      expect(r.status, AudioStartStatus.engineError);
      expect(r.errorCode, LeError.LE_ERR_AUDIO_DEVICE);
      expect(audio.isRunning, isFalse);
    });

    test('không có plugin Swift (không phải iOS) → outputOnly, không crash', () async {
      expect((await audio.start()).status, AudioStartStatus.outputOnly);
      expect(fake.audioStartCount, 1);
    });
  });

  group('vòng đời (P2-03)', () {
    setUp(() => mockEnginePlatform());

    test('xuống nền khi không phát → stop; lên lại → start', () async {
      await audio.start();
      audio.onBackground(engineBusy: false);
      expect(fake.audioStopCount, 1);
      expect(audio.isRunning, isFalse);
      await audio.onForeground();
      expect(fake.audioStartCount, 2);
      expect(audio.isRunning, isTrue);
    });

    test('đang phát/thu → giữ audio khi xuống nền', () async {
      await audio.start();
      audio.onBackground(engineBusy: true);
      expect(fake.audioStopCount, 0);
      expect(audio.isRunning, isTrue);
      await audio.onForeground();
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
      await audio.onForeground();
      expect(fake.audioStartCount, 1);
      expect(audio.isRunning, isFalse);
    });
  });

  group('chế độ chỉ phát (07 §4.0)', () {
    test('lên lại app sau khi cấp quyền trong Cài đặt → bật lại input, báo listener', () async {
      var permission = 'denied';
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.setMockMethodCallHandler(
        EnginePlatform.channel,
        (call) async => call.method == 'micPermission' ? permission : null,
      );
      addTearDown(
        () => TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.setMockMethodCallHandler(
          EnginePlatform.channel,
          null,
        ),
      );
      await audio.start();
      expect(audio.outputOnly, isTrue);
      var notified = 0;
      audio.addListener(() => notified++);

      await audio.onForeground(); // vẫn chưa có quyền
      expect(audio.outputOnly, isTrue);
      expect(notified, 0);

      permission = 'granted';
      await audio.onForeground();
      expect(audio.outputOnly, isFalse);
      expect(fake.calls.last.request, {'op': 'audio.setInputEnabled', 'enabled': true});
      expect(fake.inputEnabled, isTrue);
      expect(notified, 1);
      expect(fake.audioStartCount, 1, reason: 'không khởi động lại audio, chỉ bật input');
    });
  });
}
