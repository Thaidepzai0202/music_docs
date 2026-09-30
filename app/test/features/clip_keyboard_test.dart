// P2-33 (07 §4.1d): bàn phím / pad dưới piano roll ở tab Clip — chơi NOTE_ON/OFF, ● Ghi Live (overdub MIDI của engine,
// ô trống thì tạo clip MIDI rỗng), ⇥ Step (con trỏ, hợp âm, Nghỉ, ⌫, chạm lưới đặt con trỏ), ↶; 0 rebuild khi chơi.
import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/widgets.dart' as widgets show debugOnRebuildDirtyWidget;
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/data/library_repository.dart';
import 'package:music_looper/features/clip/clip_input.dart';
import 'package:music_looper/features/clip/piano_roll.dart';
import 'package:music_looper/features/session/session_ui.dart';
import 'package:music_looper/l10n/l10n.dart';
import 'package:music_looper/model/project.dart';
import 'package:music_looper/ui_kit/keyboard_view.dart';

import '../session_harness.dart';

class _Library extends LibraryRepository {
  @override
  Future<LibraryManifest> manifest() async => testLibrary();
}

void main() {
  final vi = lookupAppLocalizations(const Locale('vi'));
  Finder cell(int t, int s) => find.byKey(Key('cell.$t.$s'));
  MidiClip? clipAt(SessionHarness h, int t, int s) => h.session.project.trackAt(t)!.clipAt(s) as MidiClip?;

  Future<SessionHarness> editCell(WidgetTester tester, int t, int s) async {
    final h = SessionHarness(tester);
    await h.pump();
    await tester.tap(find.byKey(const Key('transport.edit')));
    await tester.pump();
    await tester.tap(cell(t, s));
    await h.settle();
    return h;
  }

  /// Điểm chạm phím [pitch] trên bàn phím của tab Clip (dò theo hình học, hàng phím trắng).
  Offset keyOf(WidgetTester tester, int pitch) {
    final r = tester.getRect(find.byKey(const Key('clip.keyboard')));
    const layout = KeyboardLayout(baseNote: 48);
    for (var x = 2.0; x < r.width; x += 3) {
      final p = Offset(x, r.height - 12);
      if (layout.noteAt(p, r.size) == pitch) return r.topLeft + p;
    }
    throw StateError('không thấy phím $pitch');
  }

  Iterable<FakeSend> sent(SessionHarness h, int type) => h.fake.sent.where((c) => c.type == type);

  testWidgets('🎹 ở thanh công cụ → mở ⤢ + bàn phím; chơi phím → NOTE_ON/OFF trên track; thu ⤢ thì bàn phím ẩn', (
    tester,
  ) async {
    final h = await editCell(tester, 2, 0); // Tone tổng hợp · Hợp âm
    expect(find.byKey(const Key('clip.keyboard')), findsNothing);
    await tester.tap(find.byKey(const Key('midi.keyboard')));
    await h.settle();
    expect(h.c.read(sessionUiProvider).panelExpanded, isTrue);
    expect(find.byKey(const Key('clip.keyboard')), findsOneWidget);

    await tester.tapAt(keyOf(tester, 60));
    await tester.pump();
    expect(sent(h, LeCommandType.LE_CMD_NOTE_ON).last.track, 2);
    expect(sent(h, LeCommandType.LE_CMD_NOTE_ON).last.i0, 60);
    expect(sent(h, LeCommandType.LE_CMD_NOTE_OFF).last.i0, 60);

    await tester.tap(find.byKey(const Key('session.panel.expand')));
    await h.settle();
    expect(find.byKey(const Key('clip.keyboard')), findsNothing, reason: 'panel thường không đủ chỗ');
    await h.unmount();
  });

  testWidgets('● Ghi: clip đang dừng → launch, chạy thì bật overdub; nốt đánh được engine ghi chồng; bấm lại → dừng '
      'ghi, RECORDING_FINISHED → nốt mới vào model; ↶ bỏ cả lượt ghi', (tester) async {
    final h = await editCell(tester, 2, 0);
    await tester.tap(find.byKey(const Key('midi.keyboard')));
    await h.settle();
    final before = clipAt(h, 2, 0)!.notes;

    await tester.tap(find.byKey(const Key('clip.liveRecord')));
    await tester.pump();
    expect(sent(h, LeCommandType.LE_CMD_CLIP_LAUNCH).last.slot, 0);

    h.fake.advanceBeats(4.1); // clip vào vòng (Q 1 bar) → Playing → app bật overdub đúng một lần
    await tester.pump();
    await tester.pump();
    expect(sent(h, LeCommandType.LE_CMD_OVERDUB_TOGGLE).length, 1);
    await tester.pump();
    expect(find.text(vi.clipDangGhi), findsOneWidget);
    await tester.pump(const Duration(milliseconds: 600)); // hiệu ứng nút ● vừa chuyển sang "Đang ghi" chạy xong

    // LE_EVT_CLIP_CHANGED: nốt hiện ngay trên lớp nốt trong lúc ghi (model chưa đổi), không widget nào rebuild.
    var rebuilds = 0;
    final types = <String, int>{};
    widgets.debugOnRebuildDirtyWidget = (e, _) {
      rebuilds++;
      types.update(e.widget.runtimeType.toString(), (v) => v + 1, ifAbsent: () => 1);
    };
    addTearDown(() => widgets.debugOnRebuildDirtyWidget = null);
    final g = await tester.startGesture(keyOf(tester, 62));
    await tester.pump();
    h.fake.advanceBeats(0.5);
    await tester.pump();
    await g.up();
    await tester.pump();
    widgets.debugOnRebuildDirtyWidget = null;
    final live = tester
        .renderObjectList(find.byType(CustomPaint))
        .whereType<RenderCustomPaint>()
        .map((r) => r.painter)
        .whereType<PianoRollPainter>()
        .single
        .live!
        .value;
    expect(live?.where((n) => n.p == 62 && !before.contains(n)), hasLength(1), reason: 'lớp nốt đã có nốt vừa đánh');
    expect(clipAt(h, 2, 0)!.notes, before, reason: 'model chỉ đổi khi RECORDING_FINISHED');
    expect(rebuilds, 0, reason: 'CLIP_CHANGED chỉ vẽ lại lớp nốt; rebuild: $types');

    await tester.tap(find.byKey(const Key('clip.liveRecord')));
    await tester.pump();
    await tester.pump();
    expect(sent(h, LeCommandType.LE_CMD_OVERDUB_TOGGLE).length, 2);
    final after = clipAt(h, 2, 0)!.notes;
    expect(after.length, before.length + 1);
    expect(after.where((n) => n.p == 62 && !before.contains(n)), hasLength(1));

    await tester.tap(find.byKey(const Key('midi.undo')));
    await tester.pump();
    expect(clipAt(h, 2, 0)!.notes, before);
    await h.unmount();
  });

  testWidgets('ô trống track nhạc cụ → nút Bàn phím; ⇥ Step: phím → nốt 1 ô tại con trỏ (tạo clip rỗng), hợp âm, '
      'Nghỉ, ⌫ lùi và xoá, chạm lưới đặt con trỏ; ↶', (tester) async {
    final h = await editCell(tester, 2, 3);
    expect(find.text(vi.clipOTrongBanPhim), findsOneWidget);
    await tester.tap(find.byKey(const Key('clip.emptyKeyboard')));
    await h.settle();
    await tester.tap(find.byKey(const Key('clip.step')));
    await h.settle();
    final cursor = h.c.read(stepCursorProvider);

    await tester.tapAt(keyOf(tester, 60));
    await h.settle();
    final c = clipAt(h, 2, 3)!;
    expect(c.lengthBeats, 16.0, reason: 'Settings "Độ dài thu" mặc định 4 bar');
    expect([c.notes.single.p, c.notes.single.s, c.notes.single.d], [60, 0.0, 0.25]);
    expect(cursor.value, 0.25);

    // Hợp âm: hai ngón cùng giữ → ghi khi nhấc ngón cuối, cùng một vị trí.
    final g1 = await tester.startGesture(keyOf(tester, 64), pointer: 11);
    final g2 = await tester.startGesture(keyOf(tester, 67), pointer: 12);
    await tester.pump();
    await g1.up();
    await tester.pump();
    expect(clipAt(h, 2, 3)!.notes.length, 1, reason: 'còn ngón giữ → chưa ghi');
    await g2.up();
    await tester.pump();
    expect(clipAt(h, 2, 3)!.notes.where((n) => n.s == 0.25).map((n) => n.p).toSet(), {64, 67});
    expect(cursor.value, 0.5);

    await tester.tap(find.byKey(const Key('clip.stepRest')));
    await tester.pump();
    expect(cursor.value, 0.75);
    await tester.tapAt(keyOf(tester, 65));
    await tester.pump();
    expect(clipAt(h, 2, 3)!.notes.last, isA<Note>().having((n) => [n.p, n.s], 'p, s', [65, 0.75]));
    expect(cursor.value, 1.0);
    await tester.tap(find.byKey(const Key('clip.stepBack')));
    await tester.pump();
    expect(cursor.value, 0.75);
    expect(clipAt(h, 2, 3)!.notes.map((n) => n.p), isNot(contains(65)));

    // Chạm lưới ở chế độ Step → chỉ đặt con trỏ, không vẽ nốt.
    final editor = tester.widget<PianoRollEditor>(find.byType(PianoRollEditor));
    expect(editor.stepCursor, isNotNull);
    final roll = tester.getRect(find.byKey(const Key('pianoRoll')));
    final top = tester.getRect(find.byType(PianoRollEditor)).top + PianoRollEditor.rulerHeight + 10;
    final count = clipAt(h, 2, 3)!.notes.length;
    await tester.tapAt(Offset(roll.left + roll.width * 2 / 16 + 3, top));
    await tester.pump();
    expect(cursor.value, 2.0);
    expect(clipAt(h, 2, 3)!.notes.length, count);

    await tester.tap(find.byKey(const Key('midi.undo')));
    await tester.pump();
    expect(clipAt(h, 2, 3)!.notes.map((n) => n.p), contains(65), reason: '↶ lấy lại nốt vừa ⌫');
    await h.unmount();
  });

  testWidgets('bàn phím đánh dấu dải phím tự nhiên của nhạc cụ (manifest range): tab Clip + tab Nhạc cụ', (
    tester,
  ) async {
    final h = SessionHarness(tester);
    await h.pump(extraOverrides: [libraryRepositoryProvider.overrideWithValue(_Library())]);
    await tester.tap(find.byKey(const Key('transport.edit')));
    await tester.pump();
    await tester.tap(cell(2, 0)); // Piano điện · Hợp âm
    await h.settle();
    await tester.tap(find.byKey(const Key('midi.keyboard')));
    await h.settle();
    KeyboardPainter keys() => tester
        .renderObjectList(find.byType(CustomPaint))
        .whereType<RenderCustomPaint>()
        .map((r) => r.painter)
        .whereType<KeyboardPainter>()
        .single;
    expect(keys().range, (28, 100));

    await tester.tap(find.byKey(const Key('session.panel.expand'))); // về panel thường
    await h.settle();
    await tester.tap(find.byKey(const Key('header.name.1'))); // Tone tổng hợp
    await tester.tap(find.byKey(const Key('panel.tab.instrument')));
    await h.settle();
    expect(keys().range, (12, 108), reason: 'inst_synth bản thư viện (B2): dải tự nhiên C0–C8');
    expect([keys().inRange(11), keys().inRange(24), keys().inRange(108), keys().inRange(109)], [false, true, true, false]);
    await h.unmount();
  });

  testWidgets('track kit: pad 4×4 có tên, gõ velocity cố định 0.8', (tester) async {
    final h = await editCell(tester, 0, 3); // Kit 808 · ô trống
    await tester.tap(find.byKey(const Key('clip.emptyKeyboard')));
    await tester.runAsync(() => Future<void>.delayed(const Duration(milliseconds: 30))); // đọc SFZ
    await h.settle();
    final pads = tester.getRect(find.byKey(const Key('clip.pads')));
    final painter = tester
        .renderObjectList(find.byType(CustomPaint))
        .whereType<RenderCustomPaint>()
        .map((r) => r.painter)
        .whereType<PadPainter>()
        .single;
    expect(painter.names[36], 'Kick');
    await tester.tapAt(pads.bottomLeft + const Offset(10, -10)); // hàng dưới cùng, pad đầu = 36
    await tester.pump();
    final on = sent(h, LeCommandType.LE_CMD_NOTE_ON).last;
    expect([on.track, on.i0], [0, 36]);
    expect(on.f0, closeTo(0.8, 1e-6));
    await h.unmount();
  });

  testWidgets('hiệu năng: chơi phím 10 lần → 0 widget rebuild; chỉ lớp bàn phím vẽ lại (piano roll đứng yên)', (
    tester,
  ) async {
    final h = await editCell(tester, 2, 0);
    await tester.tap(find.byKey(const Key('midi.keyboard')));
    await h.settle();
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

    final keys = boundaryOf<KeyboardPainter>();
    final quiet = [boundaryOf<PianoRollPainter>(), boundaryOf<NoteDragPainter>(), boundaryOf<VelocityLanePainter>()];
    var rebuilds = 0;
    final types = <String, int>{};
    widgets.debugOnRebuildDirtyWidget = (e, _) {
      rebuilds++;
      types.update(e.widget.runtimeType.toString(), (v) => v + 1, ifAbsent: () => 1);
    };
    addTearDown(() => widgets.debugOnRebuildDirtyWidget = null);
    for (final pitch in const [48, 50, 52, 53, 55, 57, 59, 60, 62, 64]) {
      final g = await tester.startGesture(keyOf(tester, pitch));
      await tester.pump(null, EnginePhase.layout);
      expect(keys.debugNeedsPaint, isTrue);
      for (final q in quiet) {
        expect(q.debugNeedsPaint, isFalse, reason: '$q');
      }
      await tester.pump();
      await g.up();
      await tester.pump();
    }
    widgets.debugOnRebuildDirtyWidget = null;
    expect(rebuilds, 0, reason: 'chơi phím chỉ vẽ lại mặt phím; rebuild: $types');
    await h.unmount();
  });
}
