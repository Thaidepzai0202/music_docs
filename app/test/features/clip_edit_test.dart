// P2-18 thu MIDI · P2-19 xem/sửa MIDI · P2-20 waveform · P2-21 sửa audio clip + kéo/copy ở Edit.
import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/features/clip/piano_roll.dart';
import 'package:music_looper/features/clip/waveform.dart';
import 'package:music_looper/features/session/session_ui.dart';
import 'package:music_looper/features/settings/app_settings.dart';
import 'package:music_looper/model/project.dart';

import '../session_harness.dart';

void main() {
  Finder cell(int t, int s) => find.byKey(Key('cell.$t.$s'));

  Future<void> enterEdit(WidgetTester tester) async {
    await tester.tap(find.byKey(const Key('transport.edit')));
    await tester.pump();
  }

  /// Thu 1 take trên [track]/[slot] (track đã arm), chơi các nốt `(beat, pitch, dài)` trong lúc thu.
  Future<void> recordTake(SessionHarness h, int track, int slot, {List<(double, int, double)> notes = const []}) async {
    await h.tester.tap(find.byKey(Key('header.arm.$track')));
    await h.tester.pump();
    final g = await h.tester.startGesture(h.tester.getCenter(cell(track, slot)));
    await g.up();
    final bars = h.c.read(settingsProvider).recordBars;
    var at = 0.0;
    final events = <(double, bool, int)>[
      for (final n in notes) ...[(n.$1, true, n.$2), (n.$1 + n.$3, false, n.$2)],
    ]..sort((a, b) => a.$1.compareTo(b.$1));
    for (final e in events) {
      h.fake.advanceBeats(e.$1 - at);
      at = e.$1;
      h.fake.send(e.$2 ? LeCommandType.LE_CMD_NOTE_ON : LeCommandType.LE_CMD_NOTE_OFF, track: track, i0: e.$3, f0: 0.8);
    }
    h.fake.advanceBeats(bars * 4 + 0.1 - at);
    await h.tester.pump();
    await h.tester.pump();
  }

  group('P2-18 thu MIDI', () {
    testWidgets('track instrument: thu 2 bar → clip MIDI có đúng nốt (qua clip.getMidi)', (tester) async {
      final h = SessionHarness(tester);
      await h.pump();
      h.c.read(settingsProvider.notifier).setRecordBars(2);
      await recordTake(h, 2, 3, notes: [(0.5, 60, 1.0), (2.0, 64, 0.5), (5.25, 67, 0.5)]);
      final clip = h.session.project.trackAt(2)!.clipAt(3);
      expect(clip, isA<MidiClip>());
      final m = clip! as MidiClip;
      expect(m.lengthBeats, 8);
      expect(m.notes.map((n) => n.p), [60, 64, 67]);
      expect(m.notes.map((n) => n.s), [0.5, 2.0, 5.25]);
      expect(m.notes.first.d, 1.0);
      expect(m.notes.first.v, 102);
      expect(h.fake.calls.map((c) => c.op), containsAllInOrder(['clip.info', 'clip.getMidi']));
      expect(h.fake.calls.where((c) => c.op == 'midiClip.quantize'), isEmpty, reason: 'Settings: quantize tắt');
      await h.unmount();
    });

    testWidgets('Settings quantize 1/16 → midi.setRecordQuantize; engine quantize lúc thu, app không tự quantize', (
      tester,
    ) async {
      final h = SessionHarness(tester);
      await h.pump();
      await tester.tap(find.byKey(const Key('transport.more')));
      await h.settle();
      await tester.tap(find.byKey(const Key('transport.settings')));
      await h.settle();
      await tester.tap(find.text('1/16').last);
      await tester.tap(find.text('1 bar'));
      await h.settle();
      expect(h.c.read(settingsProvider).midiRecordQuantize, RecordQuantize.sixteenth);
      expect(h.fake.calls.last.request, {'op': 'midi.setRecordQuantize', 'grid': 0.25});
      Navigator.of(tester.element(find.byKey(const Key('settings.recordBars')))).pop();
      await h.settle();
      await recordTake(h, 2, 4, notes: [(0.13, 60, 0.5), (1.9, 62, 0.25)]);
      expect(h.fake.calls.where((c) => c.op == 'midiClip.quantize'), isEmpty);
      final m = h.session.project.trackAt(2)!.clipAt(4)! as MidiClip;
      expect(m.lengthBeats, 4);
      expect(m.notes.map((n) => n.s), [0.25, 2.0]);
      await h.unmount();
    });

    testWidgets('track audio vẫn thu ra clip audio', (tester) async {
      final h = SessionHarness(tester);
      await h.pump();
      h.c.read(settingsProvider.notifier).setRecordBars(1);
      await recordTake(h, 5, 0);
      expect(h.session.project.trackAt(5)!.clipAt(0), isA<AudioClip>());
      await h.unmount();
    });
  });

  group('P2-19 piano roll', () {
    Future<SessionHarness> openMidi(WidgetTester tester) async {
      final h = SessionHarness(tester);
      await h.pump();
      h.c.read(settingsProvider.notifier).setRecordBars(1);
      await recordTake(h, 2, 5, notes: [(0.1, 60, 0.5), (1.3, 62, 0.5), (2.6, 64, 0.5)]);
      await enterEdit(tester);
      await tester.tap(cell(2, 5));
      await h.settle();
      return h;
    }

    Offset noteCenter(WidgetTester tester, MidiClip clip, int i) {
      final r = tester.getRect(find.byKey(const Key('pianoRoll')));
      final rows = pianoRollRows(notes: clip.notes); // track Keys: nhạc cụ → phím đàn
      final geo = PianoRollGeometry(
        rows: [for (final x in rows) x.pitch],
        rowH: r.height / rows.length,
        lengthBeats: clip.lengthBeats,
        width: r.width,
      );
      return r.topLeft + geo.rectOf(clip.notes[i])!.center;
    }

    testWidgets('hiện nốt; chạm chọn 2 nốt → Xoá → clip.setMidi với nốt còn lại', (tester) async {
      final h = await openMidi(tester);
      expect(find.byKey(const Key('pianoRoll')), findsOneWidget);
      expect(find.textContaining('3 nốt'), findsOneWidget);
      final clip = h.session.project.trackAt(2)!.clipAt(5)! as MidiClip;
      await tester.tapAt(noteCenter(tester, clip, 0));
      await tester.tapAt(noteCenter(tester, clip, 2));
      await tester.pump();
      expect(find.byTooltip('Xoá nốt đã chọn (2)'), findsOneWidget);
      await tester.tap(find.byKey(const Key('midi.deleteNotes')));
      await h.settle();
      final after = h.session.project.trackAt(2)!.clipAt(5)! as MidiClip;
      expect(after.notes.map((n) => n.p), [62]);
      final req = h.fake.calls.lastWhere((c) => c.op == 'clip.setMidi').request;
      expect((req['notes'] as List).length, 1);
      expect(h.fake.session.midiNotes(2, 5).single['p'], 62, reason: 'engine có đúng nốt còn lại');
      await h.unmount();
    });

    testWidgets('Quantize 1/4 → midiClip.quantize rồi đọc lại nốt, lưu vào model', (tester) async {
      final h = await openMidi(tester);
      await tester.tap(find.byKey(const Key('midi.quantize')));
      await h.settle();
      await tester.tap(find.byKey(const Key('midi.quantize.1/4')));
      await h.settle();
      expect(h.fake.calls.map((c) => c.op), containsAllInOrder(['midiClip.quantize', 'clip.getMidi']));
      final m = h.session.project.trackAt(2)!.clipAt(5)! as MidiClip;
      expect(m.notes.map((n) => n.s), [0.0, 1.0, 3.0]);
      await h.unmount();
    });

    testWidgets('Clear → xoá hết nốt nhưng giữ clip', (tester) async {
      final h = await openMidi(tester);
      await tester.tap(find.byKey(const Key('midi.clear')));
      await h.settle();
      final m = h.session.project.trackAt(2)!.clipAt(5)! as MidiClip;
      expect(m.notes, isEmpty);
      expect(h.fake.calls.lastWhere((c) => c.op == 'clip.setMidi').request['notes'], isEmpty);
      await h.unmount();
    });
  });

  group('P2-20/21 clip audio', () {
    Future<(SessionHarness, String)> openAudio(WidgetTester tester) async {
      final h = SessionHarness(tester);
      await h.pump();
      h.c.read(settingsProvider.notifier).setRecordBars(1);
      await recordTake(h, 5, 1);
      final id = h.session.project.trackAt(5)!.clipAt(1)!.id;
      await enterEdit(tester);
      await tester.tap(cell(5, 1));
      await h.settle();
      return (h, id);
    }

    test('chọn mức peaks: thô nhất vẫn ≥ 1 điểm/pixel', () {
      expect(peakLevelFor(totalSamples: 48000 * 60, widthPx: 900), 1); // 3200 spp → mức 2048
      expect(peakLevelFor(totalSamples: 48000 * 60, widthPx: 3600), 0); // 800 spp → mức 256
      expect(peakLevelFor(totalSamples: 48000 * 600, widthPx: 900), 2);
      expect(peakLevelFor(totalSamples: 1000, widthPx: 900), 0);
    });

    testWidgets('waveform: peaks lấy 1 lần mỗi (clipId, zoom); rebuild dùng lại Picture', (tester) async {
      final (h, id) = await openAudio(tester);
      expect(find.byKey(const Key('audio.waveform')), findsOneWidget);
      expect(h.fake.peakRequests.where((r) => r.$1 == id).length, 1);

      await tester.drag(find.byKey(const Key('audio.gain')), const Offset(20, 0)); // rebuild view
      await h.settle();
      expect(h.fake.peakRequests.where((r) => r.$1 == id).length, 1, reason: 'không vẽ lại từ peaks khi rebuild');

      await tester.tap(find.text('4×'));
      await h.settle();
      expect(h.fake.peakRequests.where((r) => r.$1 == id).length, 2);
      await tester.tap(find.text('1×'));
      await h.settle();
      expect(h.fake.peakRequests.where((r) => r.$1 == id).length, 2, reason: 'zoom 1× đã có trong cache');
      await h.unmount();
    });

    testWidgets('kéo handle đầu loop → clip.setLoopRegion (startSample > 0), model cập nhật', (tester) async {
      final (h, _) = await openAudio(tester);
      await tester.drag(find.byKey(const Key('audio.loopStart')), const Offset(120, 0));
      await h.settle();
      final req = h.fake.calls.lastWhere((c) => c.op == 'clip.setLoopRegion').request;
      expect(req['track'], 5);
      expect(req['startSample'] as int, greaterThan(0));
      final clip = h.session.project.trackAt(5)!.clipAt(1)! as AudioClip;
      expect(clip.loop.startSample, req['startSample']);
      expect(clip.lengthBeats, req['lengthBeats']);
      expect(clip.lengthBeats, lessThan(4));
      await h.unmount();
    });

    testWidgets('gain + warp → clip.setParams (không gửi lại clip.setAudio)', (tester) async {
      final (h, _) = await openAudio(tester);
      await tester.drag(find.byKey(const Key('audio.gain')), const Offset(-60, 0));
      await h.settle();
      final setAudioCount = h.fake.calls.where((c) => c.op == 'clip.setAudio').length;
      var req = h.fake.calls.lastWhere((c) => c.op == 'clip.setParams').request;
      expect(req['gainDb'] as double, lessThan(0));
      expect(req.containsKey('warp'), isFalse, reason: 'chỉ gửi trường đổi');
      await tester.tap(find.text('Re-Pitch'));
      await h.settle();
      req = h.fake.calls.lastWhere((c) => c.op == 'clip.setParams').request;
      expect(req, {'op': 'clip.setParams', 'track': 5, 'slot': 1, 'warp': 'repitch'});
      expect(h.fake.calls.where((c) => c.op == 'clip.setAudio').length, setAudioCount, reason: 'không decode lại');
      final clip = h.session.project.trackAt(5)!.clipAt(1)! as AudioClip;
      expect(clip.warp, WarpMode.repitch);
      expect(clip.gainDb, lessThan(0));
      expect(h.fake.session.clipInfo(5, 1)!['warp'], 'repitch');
      await h.unmount();
    });
  });

  group('P2-21 kéo/copy clip ở Edit', () {
    Future<void> dragCell(WidgetTester tester, (int, int) from, (int, int) to) async {
      final a = tester.getCenter(cell(from.$1, from.$2));
      final b = tester.getCenter(cell(to.$1, to.$2));
      final g = await tester.startGesture(a);
      await g.moveBy(const Offset(0, 20));
      await tester.pump();
      await g.moveTo(b);
      await tester.pump();
      await g.up();
      await tester.pump();
      await tester.pump(const Duration(milliseconds: 300));
    }

    testWidgets('kéo sang ô trống → Di chuyển: engine clear ô cũ + setMidi ô mới, giữ id', (tester) async {
      final h = SessionHarness(tester);
      await h.pump();
      await enterEdit(tester);
      final id = h.session.project.trackAt(0)!.clipAt(0)!.id;
      await dragCell(tester, (0, 0), (5, 6));
      expect(find.byKey(const Key('drop.move')), findsOneWidget);
      await tester.tap(find.byKey(const Key('drop.move')));
      await tester.pump(const Duration(milliseconds: 300));
      expect(h.session.project.trackAt(0)!.clipAt(0), isNull);
      expect(h.session.project.trackAt(5)!.clipAt(6)!.id, id);
      final ops = h.fake.calls.map((c) => c.op).where((op) => op != 'clip.info').toList(); // bỏ lệnh đọc
      expect(ops.sublist(ops.length - 2), ['clip.clear', 'clip.setMidi']);
      expect(h.c.read(sessionUiProvider).selected, const CellRef(5, 6));
      await h.unmount();
    });

    testWidgets('kéo sang ô trống → Copy: ô cũ giữ nguyên, ô mới là bản sao id mới', (tester) async {
      final h = SessionHarness(tester);
      await h.pump();
      await enterEdit(tester);
      await dragCell(tester, (1, 0), (1, 5));
      await tester.tap(find.byKey(const Key('drop.copy')));
      await tester.pump(const Duration(milliseconds: 300));
      final a = h.session.project.trackAt(1)!.clipAt(0)!;
      final b = h.session.project.trackAt(1)!.clipAt(5)!;
      expect(b.name, a.name);
      expect(b.id, isNot(a.id));
      await h.unmount();
    });

    testWidgets('thả lên ô đã có clip → báo, không đổi gì', (tester) async {
      final h = SessionHarness(tester);
      await h.pump();
      await enterEdit(tester);
      final before = h.session.project;
      await dragCell(tester, (0, 0), (0, 1));
      expect(find.text('Ô đích đã có clip'), findsOneWidget);
      expect(h.session.project, before);
      await h.unmount();
    });
  });
}
