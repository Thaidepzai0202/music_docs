import 'package:engine_ffi/engine_ffi.dart';

import 'contract/clip_scheduler_contract.dart';

final class _FakeHarness implements ClipEngineHarness {
  _FakeHarness() {
    fake.create(const EngineConfig(dataDir: '/tmp', libraryDir: '/tmp'));
    fake.audioStart(); // capture.start cần audio đang chạy (AUDIO_DEVICE), như sim.offline của engine thật
  }

  final fake = FakeEngineClient();

  @override
  EngineApi get engine => fake;

  @override
  double get beat => fake.session.beat;

  @override
  Future<void> advanceBeats(double beats) async => fake.advanceBeats(beats);

  @override
  Future<void> settle() async {} // Fake áp dụng lệnh ngay khi nhận

  @override
  Future<void> advanceSeconds(double seconds) async => fake.advanceSeconds(seconds);

  @override
  void makeCaptureSilent() => fake.captureSilent = true;

  @override
  void addSample(String path, {required bool pitched}) => fake.addSample(path, pitched: pitched);

  @override
  void makeCalibrationSilent() => fake.calibrateFailure = 'NO_SIGNAL';

  @override
  Future<void> midiIn(List<int> bytes) async => fake.midiIn(bytes);

  @override
  bool get deliversEvents => true;

  @override
  Future<void> dispose() async => fake.dispose();
}

void main() {
  defineClipSchedulerContract('FakeEngineClient', () async => _FakeHarness());
}
