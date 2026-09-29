// Settings lưu ra đĩa (bản tạm cho P4-13) + quantize khi thu MIDI đi qua engine (P1-30).
import 'dart:io';

import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/features/settings/app_settings.dart';

import '../test_utils.dart';

void main() {
  test('JSON round-trip + giá trị lạ → mặc định an toàn', () {
    const s = AppSettings(recordBars: 8, midiRecordQuantize: RecordQuantize.eighth, haptics: false);
    expect(AppSettings.fromJson(s.toJson()), s);
    expect(AppSettings.fromJson({'recordBars': 99, 'midiRecordQuantize': '1/3'}), const AppSettings(recordBars: 16));
  });

  test('SettingsRepository: ghi rồi đọc lại; file hỏng hoặc chưa có → mặc định', () async {
    final dir = Directory.systemTemp.createTempSync('settings_');
    addTearDown(() => dir.deleteSync(recursive: true));
    final repo = SettingsRepository(File('${dir.path}/settings.json'));
    expect(await repo.load(), const AppSettings());
    const s = AppSettings(recordBars: 2, midiRecordQuantize: RecordQuantize.sixteenth);
    await repo.save(s);
    expect(await repo.load(), s);
    expect(File('${dir.path}/settings.json.tmp').existsSync(), isFalse);
    File('${dir.path}/settings.json').writeAsStringSync('{hỏng');
    expect(await repo.load(), const AppSettings());
  });

  test('controller: dùng Settings đã lưu làm giá trị đầu, lưu mỗi lần đổi, quantize gửi engine', () async {
    final fake = createFakeEngine();
    final mem = MemorySettingsRepository();
    final c = ProviderContainer(
      overrides: [
        ...engineOverrides(fake, settings: mem),
        initialSettingsProvider.overrideWithValue(const AppSettings(recordBars: 2)),
      ],
    );
    addTearDown(c.dispose);
    expect(c.read(settingsProvider).recordBars, 2);

    c.read(settingsProvider.notifier).setMidiRecordQuantize(RecordQuantize.eighth);
    c.read(settingsProvider.notifier).setHaptics(false);
    c.read(settingsProvider.notifier).setHaptics(false); // không đổi → không lưu thêm
    await Future<void>.delayed(Duration.zero);
    expect(fake.calls.single.request, {'op': 'midi.setRecordQuantize', 'grid': 0.5});
    expect(fake.session.recordQuantize, 0.5);
    expect(mem.saved.length, 2);
    expect(mem.stored, const AppSettings(recordBars: 2, midiRecordQuantize: RecordQuantize.eighth, haptics: false));
  });
}
