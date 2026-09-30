// P2-31 (07 §4.1b, cập nhật 29/09 từ iPad): piano roll v2 — chạm giữ kéo để vẽ nốt dài, tay nắm 32pt ở cuối nốt,
// chọn nhiều (khung, thước bar, tất cả), dời / đổi độ dài / nhân bản cả nhóm, thanh velocity, cuộn 2 ngón.
// Mỗi thao tác gửi clip.setMidi đúng 1 lần lúc nhấc tay và hoàn tác được; kéo nhóm: 0 rebuild, chỉ lớp kéo vẽ lại.
import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/widgets.dart' as widgets show debugOnRebuildDirtyWidget;
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/features/clip/piano_roll.dart';
import 'package:music_looper/model/project.dart';

import '../session_harness.dart';

void main() {
  Finder cell(int t, int s) => find.byKey(Key('cell.$t.$s'));

  /// Keys (track 2, nhạc cụ): Edit → nhấn giữ ô 3 (trống) → "Clip MIDI trống · 2 bar" → piano roll chế độ Vẽ.
  Future<SessionHarness> openEmpty(WidgetTester tester) async {
    final h = SessionHarness(tester);
    await h.pump();
    await tester.tap(find.byKey(const Key('transport.edit')));
    await tester.pump();
    await tester.longPress(cell(2, 3));
    await h.settle();
    await tester.tap(find.byKey(const Key('menu.emptyMidi.2')));
    await h.settle();
    return h;
  }

  MidiClip clipOf(SessionHarness h) => h.session.project.trackAt(2)!.clipAt(3)! as MidiClip;
  PianoRollEditor editor(WidgetTester tester) => tester.widget<PianoRollEditor>(find.byType(PianoRollEditor));
  Rect roll(WidgetTester tester) => tester.getRect(find.byKey(const Key('pianoRoll')));

  PianoRollGeometry geoOf(WidgetTester tester, MidiClip clip) {
    final e = editor(tester);
    final r = roll(tester);
    return PianoRollGeometry(
      rows: [for (final x in e.rows) x.pitch],
      rowH: r.height / e.rows.length,
      lengthBeats: clip.lengthBeats,
      width: r.width,
    );
  }

  /// Điểm trên màn hình của (beat, cao độ). Cao độ 59..61 luôn nằm trong vùng nhìn (clip trống → căn giữa C4).
  Offset at(WidgetTester tester, MidiClip clip, double beat, int pitch) {
    final geo = geoOf(tester, clip);
    return roll(tester).topLeft + Offset(geo.xOf(beat) + 3, geo.rows.indexOf(pitch) * geo.rowH + geo.rowH / 2);
  }

  /// Thân nốt (gần đầu nốt) và tay nắm (sát cuối nốt).
  Offset body(WidgetTester tester, MidiClip clip, int i) {
    final r = geoOf(tester, clip).rectOf(clip.notes[i])!;
    return roll(tester).topLeft + Offset(r.left + 3, r.center.dy);
  }

  Offset end(WidgetTester tester, MidiClip clip, int i) {
    final r = geoOf(tester, clip).rectOf(clip.notes[i])!;
    return roll(tester).topLeft + Offset(r.right - 3, r.center.dy);
  }

  int setMidiCount(SessionHarness h) => h.fake.calls.where((c) => c.op == 'clip.setMidi').length;

  Future<void> tapNotes(WidgetTester tester, SessionHarness h, List<(double, int)> notes) async {
    for (final (beat, pitch) in notes) {
      await tester.tapAt(at(tester, clipOf(h), beat, pitch));
      await tester.pump(const Duration(milliseconds: 200));
    }
  }

  Future<void> drag(WidgetTester tester, Offset from, Offset to, {int steps = 10}) async {
    final g = await tester.startGesture(from);
    for (var k = 1; k <= steps; k++) {
      await g.moveTo(Offset.lerp(from, to, k / steps)!);
      await tester.pump();
    }
    await g.up();
    await tester.pump();
  }

  Future<void> tapKey(WidgetTester tester, SessionHarness h, String key) async {
    await tester.tap(find.byKey(Key(key)));
    await h.settle();
  }

  T painterOf<T>(WidgetTester tester) => tester
      .renderObjectList(find.byType(CustomPaint))
      .whereType<RenderCustomPaint>()
      .map((r) => r.painter)
      .whereType<T>()
      .single;

  testWidgets('Vẽ: chạm giữ rồi kéo sang phải → nốt dài theo ngón (snap), nhấc tay mới gửi; chạm sau đó → dài bằng '
      'nốt vừa vẽ; đổi lưới → về 1 ô', (tester) async {
    final h = await openEmpty(tester);
    final before = setMidiCount(h);
    final g = await tester.startGesture(at(tester, clipOf(h), 1.3, 60));
    for (var k = 1; k <= 8; k++) {
      await g.moveTo(at(tester, clipOf(h), 1.3 + 0.8 * k / 8, 60));
      await tester.pump();
    }
    expect(setMidiCount(h), before, reason: 'đang kéo: chưa gửi engine');
    expect(clipOf(h).notes, isEmpty);
    expect(painterOf<NoteDragPainter>(tester).overlay.value.notes.single.d, 1.0, reason: 'nốt xem trước ở lớp kéo');
    await g.up();
    await tester.pump(const Duration(milliseconds: 200));
    expect(clipOf(h).notes, const [Note(p: 60, v: 100, s: 1.25, d: 1)], reason: 'phủ tới hết ô chứa beat 2.1');
    expect(setMidiCount(h), before + 1);
    expect(h.fake.sent.where((x) => x.type == LeCommandType.LE_CMD_NOTE_ON && x.i0 == 60), isNotEmpty);

    await tapNotes(tester, h, [(4.1, 61)]);
    expect(clipOf(h).notes.last, const Note(p: 61, v: 100, s: 4, d: 1), reason: 'chạm → dài bằng nốt vừa vẽ');

    await tapKey(tester, h, 'midi.grid');
    await tapKey(tester, h, 'midi.grid.1/8');
    await tapNotes(tester, h, [(6.2, 59)]);
    expect(clipOf(h).notes.last, const Note(p: 59, v: 100, s: 6, d: 0.5));
    await h.unmount();
  });

  testWidgets('tay nắm cuối nốt 32pt: nốt ngắn thì lòi ra ngoài — kéo phần lòi ra → đổi độ dài; chạm vào đó ở '
      'chế độ Vẽ → vẽ nốt mới (không xoá nốt ngắn)', (tester) async {
    final h = await openEmpty(tester);
    await tapNotes(tester, h, [(1.0, 60)]);
    final geo = geoOf(tester, clipOf(h));
    final r = geo.rectOf(clipOf(h).notes.single)!;
    expect(geo.handleOf(clipOf(h).notes.single)!.right, greaterThan(r.right + 12));
    final out = roll(tester).topLeft + Offset(r.right + 8, r.center.dy);
    await drag(tester, out, out + Offset(geo.xOf(1), 0));
    expect(clipOf(h).notes.single, const Note(p: 60, v: 100, s: 1, d: 1.25));

    await tapNotes(tester, h, [(4.0, 60)]);
    final r2 = geoOf(tester, clipOf(h)).rectOf(clipOf(h).notes[1])!;
    await tester.tapAt(roll(tester).topLeft + Offset(r2.right + 6, r2.center.dy));
    await tester.pump(const Duration(milliseconds: 200));
    expect(clipOf(h).notes.sublist(1), const [
      Note(p: 60, v: 100, s: 4, d: 0.25),
      Note(p: 60, v: 100, s: 4.25, d: 0.25),
    ]);
    await h.unmount();
  });

  testWidgets('chọn nhiều: kéo thước → mọi nốt bắt đầu trong đoạn (tự sang chế độ Chọn); chạm nền → bỏ chọn; '
      'kéo khung; chọn tất cả', (tester) async {
    final h = await openEmpty(tester);
    await tapNotes(tester, h, [(1.0, 60), (2.0, 61), (5.0, 59)]);
    final geo = geoOf(tester, clipOf(h));
    final ruler = tester.getRect(find.byKey(const Key('pianoRoll.ruler')));
    await drag(tester, ruler.topLeft + Offset(geo.xOf(0.9), 10), ruler.topLeft + Offset(geo.xOf(2.4), 10));
    await h.settle();
    expect(editor(tester).selected, {0, 1});
    expect(editor(tester).mode, PianoRollMode.select);

    await tester.tapAt(at(tester, clipOf(h), 3.0, 60));
    await tester.pump();
    expect(editor(tester).selected, isEmpty);

    await drag(tester, at(tester, clipOf(h), 0.5, 61), at(tester, clipOf(h), 2.6, 60));
    expect(editor(tester).selected, {0, 1}, reason: 'khung chạm nốt beat 1 (60) và beat 2 (61), không chạm beat 5');

    await tapKey(tester, h, 'midi.selectAll');
    expect(editor(tester).selected, {0, 1, 2});
    await h.unmount();
  });

  testWidgets('dời nhóm: kéo một nốt đã chọn → cả nhóm đi tự do (thời gian + cao độ, snap) có bản mờ; dừng ở biên '
      'clip; 1 lần setMidi; lựa chọn giữ nguyên; hoàn tác được', (tester) async {
    final h = await openEmpty(tester);
    await tapNotes(tester, h, [(1.0, 60), (2.0, 59), (5.0, 60)]);
    await tapKey(tester, h, 'midi.mode.select');
    await tester.tapAt(body(tester, clipOf(h), 0));
    await tester.pump();
    await tester.tapAt(body(tester, clipOf(h), 1));
    await tester.pump();
    expect(editor(tester).selected, {0, 1});

    final geo = geoOf(tester, clipOf(h));
    final before = setMidiCount(h);
    final from = body(tester, clipOf(h), 0);
    final g = await tester.startGesture(from);
    for (var k = 1; k <= 10; k++) {
      await g.moveTo(from + Offset(geo.xOf(1) * k / 10, -geo.rowH * k / 10));
      await tester.pump();
    }
    expect(painterOf<PianoRollPainter>(tester).ghost.value, {0, 1}, reason: 'bản mờ ở vị trí cũ');
    expect(painterOf<NoteDragPainter>(tester).overlay.value.notes.length, 2);
    expect(setMidiCount(h), before);
    await g.up();
    await tester.pump();
    expect(clipOf(h).notes, const [
      Note(p: 61, v: 100, s: 2, d: 0.25),
      Note(p: 60, v: 100, s: 3, d: 0.25),
      Note(p: 60, v: 100, s: 5, d: 0.25),
    ]);
    expect(setMidiCount(h), before + 1);
    expect(editor(tester).selected, {0, 1});

    final far = body(tester, clipOf(h), 1);
    await drag(tester, far, far + Offset(geo.xOf(20), 0));
    expect([clipOf(h).notes[0].s, clipOf(h).notes[1].s], [6.75, 7.75], reason: 'cả nhóm dừng ở cuối clip 8 beat');
    await tapKey(tester, h, 'midi.undo');
    expect([clipOf(h).notes[0].s, clipOf(h).notes[1].s], [2.0, 3.0]);
    await h.unmount();
  });

  testWidgets('đổi độ dài nhóm: kéo tay nắm của một nốt đã chọn → mọi nốt trong nhóm đổi cùng một lượng', (
    tester,
  ) async {
    final h = await openEmpty(tester);
    await tapNotes(tester, h, [(1.0, 60), (3.0, 61), (5.0, 59)]);
    await tapKey(tester, h, 'midi.mode.select');
    await tester.tapAt(body(tester, clipOf(h), 0));
    await tester.pump();
    await tester.tapAt(body(tester, clipOf(h), 1));
    await tester.pump();
    final geo = geoOf(tester, clipOf(h));
    final e = end(tester, clipOf(h), 0);
    await drag(tester, e, e + Offset(geo.xOf(0.5), 0));
    expect(clipOf(h).notes.map((n) => n.d), [0.75, 0.75, 0.25]);
    expect(editor(tester).selected, {0, 1});
    await h.unmount();
  });

  testWidgets('nhân bản nhóm: đặt ngay sau nhóm (lệch = độ dài đoạn, làm tròn lên theo lưới), chọn bản chép; hết '
      'chỗ → hỏi tăng độ dài clip (Huỷ / Tăng); hoàn tác được', (tester) async {
    final h = await openEmpty(tester);
    await tapNotes(tester, h, [(1.0, 60), (2.0, 61)]);
    await tapKey(tester, h, 'midi.mode.select');
    await tapKey(tester, h, 'midi.selectAll');
    await tapKey(tester, h, 'midi.duplicate');
    expect(clipOf(h).notes.sublist(2), const [
      Note(p: 60, v: 100, s: 2.25, d: 0.25),
      Note(p: 61, v: 100, s: 3.25, d: 0.25),
    ]);
    expect(editor(tester).selected, {2, 3});
    await tapKey(tester, h, 'midi.duplicate');
    expect(clipOf(h).notes.sublist(4).map((n) => n.s), [3.5, 4.5], reason: 'bấm tiếp → chép tiếp về sau');

    await tapKey(tester, h, 'midi.selectAll'); // đoạn 1 → 4.75 (3.75 beat) → chép tới 8.5 > 8
    await tapKey(tester, h, 'midi.duplicate');
    await tapKey(tester, h, 'midi.extend.cancel');
    expect([clipOf(h).notes.length, clipOf(h).lengthBeats], [6, 8.0]);
    await tapKey(tester, h, 'midi.duplicate');
    await tapKey(tester, h, 'midi.extend.ok');
    expect([clipOf(h).notes.length, clipOf(h).lengthBeats], [12, 16.0], reason: 'cần 3 bar → bar kế tiếp có sẵn là 4');
    expect(h.fake.calls.lastWhere((x) => x.op == 'clip.setMidi').request['lengthBeats'], 16.0);
    await tapKey(tester, h, 'midi.undo');
    expect([clipOf(h).notes.length, clipOf(h).lengthBeats], [6, 8.0]);
    await h.unmount();
  });

  testWidgets('thanh velocity: kéo cột đổi velocity (1–127), nhấc tay mới gửi; nốt đang chọn → cả nhóm theo cùng '
      'tỉ lệ', (tester) async {
    final h = await openEmpty(tester);
    await tapNotes(tester, h, [(1.0, 60), (3.0, 61), (5.0, 59)]);
    final geo = geoOf(tester, clipOf(h));
    final lane = tester.getRect(find.byKey(const Key('pianoRoll.velocity')));
    final usable = VelocityLanePainter.usable(lane.height);
    Offset col(int i) {
      final n = clipOf(h).notes[i];
      return lane.topLeft + Offset(geo.xOf(n.s) + VelocityLanePainter.stemInset, lane.height - 4 - usable * n.v / 127);
    }

    final before = setMidiCount(h);
    await drag(tester, col(2), col(2) + Offset(0, usable * 20 / 127));
    expect(clipOf(h).notes.map((n) => n.v), [100, 100, 80]);
    expect(setMidiCount(h), before + 1);

    await tapKey(tester, h, 'midi.mode.select');
    await tester.tapAt(body(tester, clipOf(h), 0));
    await tester.pump();
    await tester.tapAt(body(tester, clipOf(h), 1));
    await tester.pump();
    await drag(tester, col(0), col(0) + Offset(0, usable * 50 / 127));
    expect(clipOf(h).notes.map((n) => n.v), [50, 50, 80]);
    expect(editor(tester).selected, {0, 1});
    await tapKey(tester, h, 'midi.undo');
    expect(clipOf(h).notes.map((n) => n.v), [100, 100, 80]);
    await h.unmount();
  });

  testWidgets('velocity hợp âm: nốt cùng đầu chung một cột (×N, cao theo nốt to nhất); không chọn → đổi cả hợp âm; '
      'có chọn → đổi nốt đã chọn; cột không có nốt chọn → chỉ cột đó', (tester) async {
    final h = await openEmpty(tester);
    await tapNotes(tester, h, [(1.0, 60), (1.0, 61), (3.0, 59)]);
    final lane = painterOf<VelocityLanePainter>(tester);
    expect(lane.columns, [
      [0, 1],
      [2],
    ]);
    final geo = geoOf(tester, clipOf(h));
    final r = tester.getRect(find.byKey(const Key('pianoRoll.velocity')));
    final usable = VelocityLanePainter.usable(r.height);
    Offset col(double beat) => r.topLeft + Offset(geo.xOf(beat) + VelocityLanePainter.stemInset, r.height / 2);
    Future<void> dragDown(double beat, int dv) => drag(tester, col(beat), col(beat) + Offset(0, usable * dv / 127));

    await dragDown(1, 20);
    expect(clipOf(h).notes.map((n) => n.v), [80, 80, 100], reason: 'không chọn → cả hợp âm theo cùng tỉ lệ');

    await tapKey(tester, h, 'midi.mode.select');
    await tester.tapAt(body(tester, clipOf(h), 1));
    await tester.pump();
    expect(editor(tester).selected, {1});
    await dragDown(1, 40);
    expect(clipOf(h).notes.map((n) => n.v), [80, 40, 100], reason: 'có chọn → chỉ nốt đã chọn');
    expect(VelocityLanePainter.shown(clipOf(h).notes, const [0, 1]), 80, reason: 'thân cột = velocity lớn nhất');

    await dragDown(3, 30);
    expect(clipOf(h).notes.map((n) => n.v), [80, 40, 70], reason: 'cột không có nốt chọn → chỉ cột đó');
    await tapKey(tester, h, 'midi.undo');
    expect(clipOf(h).notes.map((n) => n.v), [80, 40, 100]);
    await h.unmount();
  });

  testWidgets('2 ngón: cuộn lưới (zoom 2×), không vẽ / sửa nốt, không gửi engine', (tester) async {
    final h = await openEmpty(tester);
    await tapNotes(tester, h, [(1.0, 60)]);
    await tapKey(tester, h, 'midi.zoom');
    await tapKey(tester, h, 'midi.zoom.2');
    ScrollPosition horizontal() => tester
        .stateList<ScrollableState>(
          find.descendant(of: find.byType(PianoRollEditor), matching: find.byType(Scrollable)),
        )
        .firstWhere((s) => s.position.axis == Axis.horizontal)
        .position;
    final notes = clipOf(h).notes;
    final before = setMidiCount(h);
    final p = at(tester, clipOf(h), 3.0, 60);
    final g1 = await tester.startGesture(p, pointer: 7);
    final g2 = await tester.startGesture(p + const Offset(0, 20), pointer: 8);
    for (var k = 0; k < 5; k++) {
      await g1.moveBy(const Offset(-40, 0));
      await g2.moveBy(const Offset(-40, 0));
      await tester.pump();
    }
    await g1.up();
    await g2.up();
    await tester.pump();
    expect(horizontal().pixels, closeTo(200, 1));
    expect(clipOf(h).notes, notes);
    expect(setMidiCount(h), before);
    await h.unmount();
  });

  testWidgets('hiệu năng: kéo nhóm 20 frame + playhead → 0 widget rebuild; chỉ lớp kéo bẩn (lớp nốt, cột phím, '
      'thước, velocity không)', (tester) async {
    final h = await openEmpty(tester);
    await tapNotes(tester, h, [(1.0, 60), (2.0, 59)]);
    await tapKey(tester, h, 'midi.mode.select');
    await tapKey(tester, h, 'midi.selectAll');
    h.fake.send(LeCommandType.LE_CMD_CLIP_LAUNCH, track: 2, slot: 3);
    await tester.pump(const Duration(milliseconds: 600));
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
    final dragLayer = boundaryOf<NoteDragPainter>();
    final quiet = [boundaryOf<PianoKeysPainter>(), boundaryOf<RulerPainter>(), boundaryOf<VelocityLanePainter>()];
    final geo = geoOf(tester, clipOf(h));
    final start = body(tester, clipOf(h), 0);
    final g = await tester.startGesture(start);
    await g.moveBy(const Offset(PianoRollEditor.dragSlop + 2, 0));
    await tester.pump(); // qua ngưỡng kéo: lớp nốt vẽ bản mờ một lần
    var rebuilds = 0;
    final types = <String, int>{};
    widgets.debugOnRebuildDirtyWidget = (e, _) {
      rebuilds++;
      types.update(e.widget.runtimeType.toString(), (v) => v + 1, ifAbsent: () => 1);
    };
    addTearDown(() => widgets.debugOnRebuildDirtyWidget = null);
    for (var k = 1; k <= 20; k++) {
      await g.moveTo(start + Offset(10 + geo.xOf(2) * k / 20, -geo.rowH * k / 20));
      h.fake.advanceBeats(0.05);
      await tester.pump(const Duration(milliseconds: 16));
    }
    widgets.debugOnRebuildDirtyWidget = null;
    expect(rebuilds, 0, reason: 'kéo nhóm + playhead chỉ vẽ lại lớp của chúng; rebuild: $types');

    await g.moveBy(Offset(geo.xOf(0.5), 0));
    await tester.pump(null, EnginePhase.layout); // dừng trước paint
    expect(dragLayer.debugNeedsPaint, isTrue);
    expect(notes.debugNeedsPaint, isFalse);
    for (final q in quiet) {
      expect(q.debugNeedsPaint, isFalse, reason: '$q');
    }
    await tester.pump();
    await g.up();
    await tester.pump();
    await h.unmount();
  });

  testWidgets('hiệu năng: kéo cột velocity → 0 widget rebuild; chỉ thanh velocity bẩn (lớp nốt, lớp kéo, cột phím, '
      'thước không)', (tester) async {
    final h = await openEmpty(tester);
    await tapNotes(tester, h, [(1.0, 60), (3.0, 61)]);
    await tester.pump(const Duration(milliseconds: 600)); // hiệu ứng nút ↶ vừa bật chạy xong (không thuộc thao tác kéo)
    RenderObject boundaryOf<T>() {
      RenderObject? n = tester
          .renderObjectList(find.byType(CustomPaint))
          .firstWhere((r) => r is RenderCustomPaint && r.painter is T);
      while (n != null && !n.isRepaintBoundary) {
        n = n.parent;
      }
      return n!;
    }

    final lane = boundaryOf<VelocityLanePainter>();
    final quiet = [
      boundaryOf<PianoRollPainter>(),
      boundaryOf<NoteDragPainter>(),
      boundaryOf<PianoKeysPainter>(),
      boundaryOf<RulerPainter>(),
    ];
    final geo = geoOf(tester, clipOf(h));
    final r = tester.getRect(find.byKey(const Key('pianoRoll.velocity')));
    final start = r.topLeft + Offset(geo.xOf(1) + VelocityLanePainter.stemInset, r.height / 2);
    final g = await tester.startGesture(start);
    await g.moveBy(const Offset(0, 20)); // qua ngưỡng kéo dọc
    await tester.pump();
    var rebuilds = 0;
    final types = <String, int>{};
    widgets.debugOnRebuildDirtyWidget = (e, _) {
      rebuilds++;
      types.update(e.widget.runtimeType.toString(), (v) => v + 1, ifAbsent: () => 1);
    };
    addTearDown(() => widgets.debugOnRebuildDirtyWidget = null);
    for (var k = 0; k < 10; k++) {
      await g.moveBy(const Offset(0, -3));
      await tester.pump(null, EnginePhase.layout);
      expect(lane.debugNeedsPaint, isTrue);
      for (final q in quiet) {
        expect(q.debugNeedsPaint, isFalse, reason: '$q');
      }
      await tester.pump();
    }
    widgets.debugOnRebuildDirtyWidget = null;
    expect(rebuilds, 0, reason: 'kéo velocity chỉ vẽ lại thanh velocity; rebuild: $types');
    await g.up();
    await tester.pump();
    expect(clipOf(h).notes.first.v, isNot(100));
    await h.unmount();
  });
}
