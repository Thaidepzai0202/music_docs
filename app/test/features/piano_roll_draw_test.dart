// P2-29 (07 §4.1b): vẽ nốt trong piano roll — clip MIDI trống, thêm / xoá / dời / đổi độ dài (snap), nghe thử,
// undo/redo, độ dài clip, hàng pad của kit, và không rebuild widget khi kéo nốt.
import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/widgets.dart' as widgets show debugOnRebuildDirtyWidget;
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/features/clip/piano_roll.dart';
import 'package:music_looper/model/project.dart';
import 'package:music_looper/ui_kit/keyboard_view.dart';

import '../session_harness.dart';

void main() {
  Finder cell(int t, int s) => find.byKey(Key('cell.$t.$s'));

  /// Keys (track 2, nhạc cụ): Edit → nhấn giữ ô 3 (trống) → "Clip MIDI trống · 2 bar" → piano roll chế độ Vẽ.
  Future<SessionHarness> openEmpty(WidgetTester tester, {int track = 2, int slot = 3, int bars = 2}) async {
    final h = SessionHarness(tester);
    await h.pump();
    await tester.tap(find.byKey(const Key('transport.edit')));
    await tester.pump();
    await tester.longPress(cell(track, slot));
    await h.settle();
    await tester.tap(find.byKey(Key('menu.emptyMidi.$bars')));
    await h.settle();
    return h;
  }

  MidiClip clipOf(SessionHarness h, [int t = 2, int s = 3]) => h.session.project.trackAt(t)!.clipAt(s)! as MidiClip;

  PianoRollGeometry geoOf(WidgetTester tester, MidiClip clip) {
    final editor = tester.widget<PianoRollEditor>(find.byType(PianoRollEditor));
    final r = tester.getRect(find.byKey(const Key('pianoRoll')));
    return PianoRollGeometry(
      rows: [for (final x in editor.rows) x.pitch],
      rowH: r.height / editor.rows.length,
      lengthBeats: clip.lengthBeats,
      width: r.width,
    );
  }

  /// Điểm trên màn hình của (beat, cao độ) trong lưới.
  Offset at(WidgetTester tester, MidiClip clip, double beat, int pitch) {
    final geo = geoOf(tester, clip);
    final r = tester.getRect(find.byKey(const Key('pianoRoll')));
    final row = geo.rows.indexOf(pitch);
    return r.topLeft + Offset(geo.xOf(beat) + 3, row * geo.rowH + geo.rowH / 2);
  }

  Offset noteCenter(WidgetTester tester, MidiClip clip, int i) =>
      tester.getRect(find.byKey(const Key('pianoRoll'))).topLeft + geoOf(tester, clip).rectOf(clip.notes[i])!.center;

  int setMidiCount(SessionHarness h) => h.fake.calls.where((c) => c.op == 'clip.setMidi').length;

  testWidgets('Edit → nhấn giữ ô trống track nhạc cụ → Clip MIDI trống 2 bar → piano roll chế độ Vẽ', (tester) async {
    final h = await openEmpty(tester);
    final c = clipOf(h);
    expect([c.lengthBeats, c.notes], [8.0, isEmpty]);
    final req = h.fake.calls.lastWhere((x) => x.op == 'clip.setMidi').request;
    expect([req['track'], req['slot'], req['lengthBeats'], req['clipId']], [2, 3, 8.0, c.id]);
    expect(find.byType(PianoRollEditor), findsOneWidget);
    expect(tester.widget<PianoRollEditor>(find.byType(PianoRollEditor)).mode, PianoRollMode.draw);
    await h.unmount();
  });

  testWidgets('Vẽ: chạm ô trống → nốt snap 1/16, dài 1 ô, velocity 100; nghe thử NOTE_ON → NOTE_OFF sau 150 ms', (
    tester,
  ) async {
    final h = await openEmpty(tester);
    await tester.tapAt(at(tester, clipOf(h), 1.3, 60)); // ô 1/16 chứa beat 1.3 là 1.25
    await tester.pump();
    expect(clipOf(h).notes, const [Note(p: 60, v: 100, s: 1.25, d: 0.25)]);
    final req = h.fake.calls.lastWhere((x) => x.op == 'clip.setMidi').request;
    expect((req['notes'] as List).single, {'p': 60, 'v': 100, 's': 1.25, 'd': 0.25});
    final on = h.fake.sent.lastWhere((x) => x.type == LeCommandType.LE_CMD_NOTE_ON);
    expect([on.track, on.i0], [2, 60]);
    expect(h.fake.sent.where((x) => x.type == LeCommandType.LE_CMD_NOTE_OFF && x.i0 == 60), isEmpty);
    await tester.pump(const Duration(milliseconds: 160));
    expect(h.fake.sent.where((x) => x.type == LeCommandType.LE_CMD_NOTE_OFF && x.i0 == 60), isNotEmpty);

    // Lưới 1/4 → nốt dài 1 beat, bắt đầu ở beat.
    await tester.tap(find.byKey(const Key('midi.grid')));
    await h.settle();
    await tester.tap(find.byKey(const Key('midi.grid.1/4')));
    await h.settle();
    await tester.tapAt(at(tester, clipOf(h), 2.6, 62));
    await tester.pump(const Duration(milliseconds: 200));
    expect(clipOf(h).notes.last, const Note(p: 62, v: 100, s: 2, d: 1));
    await h.unmount();
  });

  testWidgets('Vẽ: chạm nốt → xoá; kéo thân → dời (snap) + đổi cao độ; kéo mép phải → đổi độ dài; mỗi lần thả '
      'tay mới gửi clip.setMidi đúng 1 lần', (tester) async {
    final h = await openEmpty(tester);
    await tester.tapAt(at(tester, clipOf(h), 1.0, 60));
    await tester.pump(const Duration(milliseconds: 200));
    await tester.tapAt(at(tester, clipOf(h), 3.0, 62));
    await tester.pump(const Duration(milliseconds: 200));
    expect(clipOf(h).notes.length, 2);

    // Chạm nốt thứ hai → xoá.
    await tester.tapAt(noteCenter(tester, clipOf(h), 1));
    await tester.pump();
    expect(clipOf(h).notes, const [Note(p: 60, v: 100, s: 1, d: 0.25)]);

    // Kéo thân: +1 beat, lên 2 hàng (cao độ 62).
    final geo = geoOf(tester, clipOf(h));
    final before = setMidiCount(h);
    final start = noteCenter(tester, clipOf(h), 0) - Offset(geo.xOf(0.25) / 2 - 2, 0); // giữa thân, xa mép phải
    final g = await tester.startGesture(start);
    for (var k = 1; k <= 10; k++) {
      await g.moveTo(start + Offset(geo.xOf(1) * k / 10, -2 * geo.rowH * k / 10));
      await tester.pump();
    }
    expect(setMidiCount(h), before, reason: 'đang kéo: chưa gửi engine');
    await g.up();
    await tester.pump();
    expect(setMidiCount(h), before + 1);
    expect(clipOf(h).notes.single, const Note(p: 62, v: 100, s: 2, d: 0.25));

    // Kéo mép phải tới beat 3 → dài 1 beat.
    final r = geoOf(tester, clipOf(h)).rectOf(clipOf(h).notes.single)!;
    final roll = tester.getRect(find.byKey(const Key('pianoRoll'))).topLeft;
    final edge = roll + Offset(r.right - 3, r.center.dy);
    await tester.dragFrom(edge, Offset(geo.xOf(0.75), 0));
    await tester.pump();
    expect(clipOf(h).notes.single, const Note(p: 62, v: 100, s: 2, d: 1));
    await h.unmount();
  });

  testWidgets('undo / redo theo clip; đổi độ dài clip 1 bar → bỏ nốt ngoài clip (hoàn tác được)', (tester) async {
    final h = await openEmpty(tester);
    await tester.tapAt(at(tester, clipOf(h), 1.0, 60));
    await tester.pump(const Duration(milliseconds: 200));
    await tester.tapAt(at(tester, clipOf(h), 6.0, 62)); // hàng còn trong vùng nhìn (panel thường)
    await tester.pump(const Duration(milliseconds: 200));
    expect(clipOf(h).notes.length, 2);
    await tester.tap(find.byKey(const Key('midi.undo')));
    await tester.pump();
    expect(clipOf(h).notes.length, 1);
    await tester.tap(find.byKey(const Key('midi.redo')));
    await tester.pump();
    expect(clipOf(h).notes.length, 2);

    await tester.tap(find.byKey(const Key('midi.length')));
    await h.settle();
    await tester.tap(find.byKey(const Key('midi.length.1')));
    await h.settle();
    expect(clipOf(h).lengthBeats, 4);
    expect(clipOf(h).notes.map((n) => n.p), [60], reason: 'nốt ở beat 6 nằm ngoài 1 bar');
    expect(h.fake.calls.lastWhere((x) => x.op == 'clip.setMidi').request['lengthBeats'], 4.0);
    await tester.tap(find.byKey(const Key('midi.undo')));
    await tester.pump();
    expect([clipOf(h).lengthBeats, clipOf(h).notes.length], [8.0, 2]);
    await h.unmount();
  });

  testWidgets('track kit: mỗi hàng là một pad (tên từ SFZ), không có phím ±quãng tám; pad Instrument hiện tên', (
    tester,
  ) async {
    final h = SessionHarness(tester);
    await h.pump();
    await tester.tap(find.byKey(const Key('transport.edit')));
    await tester.pump();
    await tester.tap(cell(0, 0)); // Drums · Beat A (kit_808, P2-30)
    await tester.runAsync(() => Future<void>.delayed(const Duration(milliseconds: 50))); // đọc SFZ từ asset
    await h.settle();
    final editor = tester.widget<PianoRollEditor>(find.byType(PianoRollEditor));
    expect(editor.kit, isTrue);
    final labels = {for (final r in editor.rows) r.pitch: r.label};
    expect(labels.keys, [for (var n = 51; n >= 36; n--) n], reason: '16 pad GM 36–51, cao ở trên');
    expect(labels, containsPair(36, 'Kick'));
    expect(labels, containsPair(38, 'Snare'));
    expect(labels, containsPair(42, 'Closed Hat'));
    expect(labels, containsPair(41, 'Floor Tom L'));
    expect(find.byKey(const Key('midi.octUp')), findsNothing);

    // 06 §4: pad 4×4 ở tab Instrument hiện region_label.
    await tester.tap(find.byKey(const Key('panel.tab.instrument')));
    await h.settle();
    final pads = tester
        .renderObjectList(find.byType(CustomPaint))
        .whereType<RenderCustomPaint>()
        .map((r) => r.painter)
        .whereType<PadPainter>()
        .single;
    expect([pads.names[36], pads.names[46], pads.names[51]], ['Kick', 'Open Hat', 'Ride']);
    await h.unmount();
  });

  testWidgets('hiệu năng: kéo nốt 20 frame và playhead chạy → 0 widget rebuild', (tester) async {
    final h = await openEmpty(tester);
    await tester.tapAt(at(tester, clipOf(h), 1.0, 60));
    await tester.pump(const Duration(milliseconds: 200));
    final geo = geoOf(tester, clipOf(h));
    final start = noteCenter(tester, clipOf(h), 0) - Offset(geo.xOf(0.25) / 2 - 2, 0);
    h.fake.send(LeCommandType.LE_CMD_CLIP_LAUNCH, track: 2, slot: 3);
    await tester.pump(const Duration(milliseconds: 600)); // hiệu ứng nút ↶ vừa bật + ô clip đổi trạng thái xong
    final g = await tester.startGesture(start);
    await g.moveBy(const Offset(0, -1)); // qua ngưỡng bắt đầu kéo
    await tester.pump();
    var rebuilds = 0;
    final types = <String, int>{};
    widgets.debugOnRebuildDirtyWidget = (e, _) {
      rebuilds++;
      types.update(e.widget.runtimeType.toString(), (v) => v + 1, ifAbsent: () => 1);
    };
    addTearDown(() => widgets.debugOnRebuildDirtyWidget = null);
    for (var k = 1; k <= 20; k++) {
      await g.moveTo(start + Offset(geo.xOf(2) * k / 20, 0));
      h.fake.advanceBeats(0.05);
      await tester.pump(const Duration(milliseconds: 16));
    }
    widgets.debugOnRebuildDirtyWidget = null;
    expect(rebuilds, 0, reason: 'nốt đang kéo + playhead chỉ vẽ lại lớp của chúng; rebuild: $types');
    await g.up();
    await tester.pump();
    await h.unmount();
  });

  testWidgets('repaint: đang kéo nốt → chỉ lớp nốt-đang-kéo bẩn, lớp nốt + cột phím không', (tester) async {
    final h = await openEmpty(tester);
    await tester.tapAt(at(tester, clipOf(h), 1.0, 60));
    await tester.pump(const Duration(milliseconds: 200));
    RenderObject boundaryOf<T>() {
      RenderObject? n = tester
          .renderObjectList(find.byType(CustomPaint))
          .firstWhere((r) => r is RenderCustomPaint && r.painter is T);
      while (n != null && !n.isRepaintBoundary) {
        n = n.parent;
      }
      return n!;
    }

    final notes = boundaryOf<PianoRollPainter>();
    final drag = boundaryOf<NoteDragPainter>();
    final keys = boundaryOf<PianoKeysPainter>();
    final geo = geoOf(tester, clipOf(h));
    final start = noteCenter(tester, clipOf(h), 0) - Offset(geo.xOf(0.25) / 2 - 2, 0);
    final g = await tester.startGesture(start);
    await g.moveBy(const Offset(PianoRollEditor.dragSlop + 2, 0));
    await tester.pump(); // qua ngưỡng kéo: lớp nốt vẽ bản mờ một lần
    await g.moveBy(Offset(geo.xOf(1), 0));
    await tester.pump(null, EnginePhase.layout); // dừng trước paint
    expect(drag.debugNeedsPaint, isTrue);
    expect(notes.debugNeedsPaint, isFalse);
    expect(keys.debugNeedsPaint, isFalse);
    await tester.pump();
    await g.up();
    await tester.pump();
    await h.unmount();
  });
}
