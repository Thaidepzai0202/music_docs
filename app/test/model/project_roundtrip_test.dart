// P2-04 DoD: round-trip mọi fixture trong test/fixtures/projects/ + migrator khung v1 → v1.
import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/model/migrations/project_migrator.dart';
import 'package:music_looper/model/project.dart';
import 'package:music_looper/model/project_codec.dart';

const fixtureDir = 'test/fixtures/projects';

List<File> projectFixtures() =>
    Directory(fixtureDir).listSync().whereType<File>().where((f) => f.path.endsWith('.json')).toList()
      ..sort((a, b) => a.path.compareTo(b.path));

void main() {
  final codec = ProjectCodec();

  test('có fixture để test', () => expect(projectFixtures().length, greaterThanOrEqualTo(3)));

  group('round-trip', () {
    for (final file in projectFixtures()) {
      final name = file.uri.pathSegments.last;

      test('$name: JSON → Project → JSON giữ nguyên nội dung', () {
        final original = jsonDecode(file.readAsStringSync()) as Map<String, dynamic>;
        final decoded = codec.decode(file.readAsStringSync());
        expect(decoded.readOnly, isFalse);
        // So sánh theo giá trị (0 == 0.0), đúng nghĩa "không mất dữ liệu".
        expect(jsonDecode(jsonEncode(decoded.project.toJson())), equals(original));
      });

      test('$name: Project → file → Project bằng nhau', () {
        final p1 = codec.decode(file.readAsStringSync()).project;
        final p2 = codec.decode(codec.encode(p1)).project;
        expect(p2, p1);
      });
    }
  });

  group('đọc đúng giá trị theo 06 §2', () {
    late Project p;
    setUp(() => p = codec.decode(File('$fixtureDir/example_v1.json').readAsStringSync()).project);

    test('transport + thời gian', () {
      expect(p.transport.quantize, QuantizeGrid.bar1);
      expect(p.transport.quantize.leValue, 5); // LE_Q_1_BAR
      expect(p.transport.metronome.mode, MetronomeMode.recordOnly);
      expect(p.transport.metronome.mode.leValue, 2);
      expect(p.createdAt, DateTime.utc(2027, 3, 20, 10, 15));
    });

    test('track, instrument union, clip union', () {
      final drums = p.trackAt(0)!;
      expect(drums.instrument, const InstrumentRef.sfz(path: 'kits/808/808.sfz'));
      expect(drums.fx.single.params, {'0': 0.0, '1': 18000.0, '2': 0.7});
      final beat = drums.clipAt(0)! as MidiClip;
      expect(beat.notes.first, const Note(p: 36, v: 110, s: 0, d: 0.25));

      final vocal = p.trackAt(1)!;
      expect(vocal.instrument, isNull);
      expect(vocal.monitor, MonitorMode.auto);
      final take = vocal.clipAt(0)! as AudioClip;
      expect(take.file, 'audio/c_3f….caf');
      expect(take.warp, WarpMode.stretch);

      expect(p.trackAt(2)!.instrument, const InstrumentRef.user(id: 'i_55…'));
      expect(p.trackAt(3), isNull);
    });

    test('track audio không ghi "instrument": null khi lưu', () {
      final vocalJson = (p.toJson()['tracks'] as List)[1] as Map<String, dynamic>;
      expect(vocalJson.containsKey('instrument'), isFalse);
    });
  });

  test('"on" trong JSON ↔ MonitorMode.always / MetronomeMode.always', () {
    final p = codec.decode(File('$fixtureDir/all_enums_v1.json').readAsStringSync()).project;
    expect(p.tracks.single.monitor, MonitorMode.always);
    expect(p.tracks.single.monitor.leValue, 2);
    expect(p.transport.metronome.mode, MetronomeMode.always);
    expect(p.transport.quantize.leValue, 1); // LE_Q_1_16
  });

  group('ProjectMigrator', () {
    Map<String, dynamic> v(int n) => {'schemaVersion': n, 'name': 'x'};

    test('v1 → v1 là no-op', () {
      final input = jsonDecode(File('$fixtureDir/example_v1.json').readAsStringSync()) as Map<String, dynamic>;
      final r = ProjectMigrator().migrate(input);
      expect(r.fromVersion, 1);
      expect(r.readOnly, isFalse);
      expect(r.json, equals(input));
      expect(identical(r.json, input), isFalse, reason: 'phải là bản copy');
    });

    test('file của app mới hơn → chỉ đọc, không đổi dữ liệu', () {
      final r = ProjectMigrator().migrate(v(kCurrentSchemaVersion + 1));
      expect(r.readOnly, isTrue);
      expect(r.json['schemaVersion'], kCurrentSchemaVersion + 1);
    });

    test('chạy tuần tự từng bước và tăng schemaVersion', () {
      final order = <int>[];
      final m = ProjectMigrator(
        currentVersion: 3,
        migrations: {
          1: (j) {
            order.add(1);
            return j..['a'] = 1;
          },
          2: (j) {
            order.add(2);
            return j..['b'] = j['a'] + 1;
          },
        },
      );
      final input = v(1);
      final r = m.migrate(input);
      expect(order, [1, 2]);
      expect(r.json, {'schemaVersion': 3, 'name': 'x', 'a': 1, 'b': 2});
      expect(input, v(1), reason: 'không sửa JSON của người gọi');
    });

    test('thiếu bước migrate → lỗi rõ ràng', () {
      expect(() => ProjectMigrator(currentVersion: 2, migrations: {}).migrate(v(1)), throwsStateError);
    });

    test('schemaVersion sai → FormatException', () {
      expect(() => ProjectMigrator().migrate({'name': 'x'}), throwsFormatException);
      expect(() => ProjectMigrator().migrate(v(0)), throwsFormatException);
    });
  });
}
