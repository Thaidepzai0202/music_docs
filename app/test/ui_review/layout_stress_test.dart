// Rà bố cục 1080×810 (07 §2) với tên rất dài ở mọi tab / chế độ: tràn chữ → FlutterError → test đỏ.
// Kèm kiểm repaint của các lớp 60Hz mới (playhead, readout transport).
import 'dart:io';

import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:flutter/rendering.dart';
import 'package:music_looper/engine/engine_providers.dart';
import 'package:music_looper/features/clip/piano_roll.dart';
import 'package:music_looper/features/export/jam_recorder.dart';
import 'package:music_looper/features/fx/fx_specs.dart';
import 'package:music_looper/features/mixer/mixer_panel.dart';
import 'package:music_looper/features/session/widgets/transport_bar.dart';
import 'package:music_looper/features/settings/app_settings.dart';
import 'package:music_looper/l10n/l10n.dart';
import 'package:music_looper/model/project.dart';

import '../session_harness.dart';

Project stressProject() {
  final d = demoProject();
  const long = 'Tên rất rất dài để kiểm tra tràn chữ trên iPad';
  return d.copyWith(
    name: 'Buổi tập chiều thứ Bảy với ban nhạc — bản thử nghiệm số 12 rất dài',
    transport: d.transport.copyWith(
      metronome: const Metronome(mode: MetronomeMode.recordOnly),
      countInBars: 2,
      quantize: QuantizeGrid.sixteenth,
    ),
    scenes: [for (final sc in d.scenes) sc.copyWith(name: '$long scene ${sc.index + 1}')],
    // FX đủ 5 loại (track 0: 3 slot đầy) + nhạc cụ tự thu trên track 3 (ADSR ở tab Instrument).
    userInstruments: const [
      UserInstrument(
        id: 'i_stress',
        name: '$long nhạc cụ tự thu',
        source: 'instruments/i_stress/source.caf',
        rootNote: 57,
      ),
    ],
    tracks: [
      for (final t in d.tracks)
        t.copyWith(
          name: '$long ${t.index + 1}',
          instrument: t.index == 3 ? const InstrumentRef.user(id: 'i_stress') : t.instrument,
          fx: switch (t.index) {
            0 => [
              FxSlot(type: FxType.comp, params: fxDefaultParams(FxType.comp)),
              const FxSlot(type: FxType.delay, params: {'0': 0, '1': 0.5, '2': 0.3, '3': 1}),
              FxSlot(type: FxType.filter, bypass: true, params: fxDefaultParams(FxType.filter)),
            ],
            1 => [
              FxSlot(type: FxType.reverb, params: fxDefaultParams(FxType.reverb)),
              FxSlot(type: FxType.eq3, params: fxDefaultParams(FxType.eq3)),
            ],
            _ => t.fx,
          },
          clips: [for (final c in t.clips) c.copyWith(name: '$long clip ${c.slot + 1}')],
        ),
    ],
  );
}

void main() {
  Finder cell(int t, int s) => find.byKey(Key('cell.$t.$s'));

  // 07 §8: chạy ở cả en và vi — nhãn tiếng Anh/tiếng Việt dài ngắn khác nhau.
  for (final locale in L10n.supportedLocales) {
    testWidgets('[${locale.languageCode}] mọi tab + Edit + Settings với tên dài: không tràn bố cục', (tester) async {
      final h = SessionHarness(tester);
      await h.pump(project: stressProject(), locale: locale);
      for (final tab in ['mixer', 'instrument', 'clip', 'fx', 'browser']) {
        await tester.tap(find.byKey(Key('panel.tab.$tab')));
        await h.settle();
      }
      // FX: track 0 (3 slot đầy), track 1 (2 slot + nút thêm) + master.
      await tester.tap(find.byKey(const Key('header.name.0')));
      await tester.tap(find.byKey(const Key('panel.tab.fx')));
      await h.settle();
      expect(find.byKey(const Key('fx.remove.2')), findsOneWidget);
      await tester.tap(find.byKey(const Key('header.name.1')));
      await h.settle();
      expect(find.byKey(const Key('fx.add.2')), findsOneWidget);
      // Nhạc cụ tự thu: Natural/Classic + ADSR cạnh bàn phím.
      await tester.tap(find.byKey(const Key('header.name.3')));
      await tester.tap(find.byKey(const Key('panel.tab.instrument')));
      await h.settle();
      expect(find.byKey(const Key('instrument.editor')), findsOneWidget);
      // Sheet Export: 2 tab.
      await tester.tap(find.byKey(const Key('transport.more')));
      await h.settle();
      await tester.tap(find.byKey(const Key('more.export')));
      await h.settle();
      await tester.tap(find.byKey(const Key('export.tab.scene')));
      await h.settle();
      await tester.tap(find.byKey(const Key('export.close')));
      await h.settle();
      // Instrument: cả Pad và Bàn phím.
      await tester.tap(find.byKey(const Key('header.name.0')));
      await tester.tap(find.byKey(const Key('panel.tab.instrument')));
      await h.settle();
      await tester.tap(find.byKey(const Key('header.name.2')));
      await h.settle();
      // Edit + chọn clip MIDI (piano roll) và clip audio (waveform).
      h.c.read(settingsProvider.notifier).setRecordBars(1);
      await tester.tap(find.byKey(const Key('header.arm.5')));
      await tester.pump();
      final g = await tester.startGesture(tester.getCenter(cell(5, 1)));
      await g.up();
      h.fake.advanceBeats(4.2);
      await tester.pump();
      await tester.pump();
      await tester.tap(find.byKey(const Key('transport.edit')));
      await tester.pump();
      for (final c in [cell(2, 0), cell(5, 1)]) {
        await tester.tap(c);
        await h.settle();
      }
      // Piano roll P2-29: clip kit (hàng pad) + chế độ Vẽ (thanh công cụ khác chế độ Chọn).
      await tester.tap(cell(0, 0));
      await tester.runAsync(() => Future<void>.delayed(const Duration(milliseconds: 30))); // đọc SFZ
      await h.settle();
      await tester.tap(find.byKey(const Key('midi.mode.draw')));
      await h.settle();
      await tester.tap(cell(2, 0));
      await h.settle();
      // ⤢ panel tab Clip ~70% màn hình: piano roll lớn, grid cuộn.
      await tester.tap(find.byKey(const Key('session.panel.expand')));
      await h.settle();
      // P2-33: bàn phím dưới piano roll + hàng ● Ghi / ⇥ Step (Nghỉ, ⌫) / quãng tám.
      await tester.tap(find.byKey(const Key('midi.keyboard')));
      await h.settle();
      await tester.tap(find.byKey(const Key('clip.step')));
      await h.settle();
      await tester.tap(find.byKey(const Key('clip.step')));
      await h.settle();
      await tester.tap(find.byKey(const Key('midi.keyboard')));
      await h.settle();
      await tester.tap(find.byKey(const Key('session.panel.expand')));
      await h.settle();
      await tester.tap(find.byKey(const Key('transport.more')));
      await h.settle();
      await tester.tap(find.byKey(const Key('transport.settings')));
      await h.settle();
      // Settings đầy đủ: đi qua mọi mục (project có tên track rất dài → MIDI learn, Link).
      for (final sec in ['audio', 'latency', 'midi', 'link', 'licenses', 'about']) {
        await tester.tap(find.byKey(Key('settings.nav.$sec')));
        await tester.runAsync(() => Future<void>.delayed(const Duration(milliseconds: 20))); // đọc asset giấy phép
        await h.settle();
      }
      await tester.tap(find.byKey(const Key('settings.back')));
      await h.settle();
      await h.unmount();
    });
  }

  testWidgets('mixer: nhãn dB của strip Master cùng hàng (top + bottom) với strip track', (tester) async {
    final h = SessionHarness(tester);
    await h.pump();
    await tester.tap(find.byKey(const Key('panel.tab.mixer')));
    await h.settle();
    Rect labelOf(Finder strip) => tester.getRect(find.descendant(of: strip, matching: find.text('+0.0')).first);
    final track = labelOf(find.byWidgetPredicate((w) => w is MixerStrip && w.track == 4)); // track 5: 0 dB
    final master = labelOf(find.byType(MasterStrip));
    expect(master.top, track.top);
    expect(master.bottom, track.bottom);
    await h.unmount();
  });

  testWidgets('repaint: nút LOOP vẽ theo ticker trong boundary riêng — không làm bẩn header / grid', (tester) async {
    final h = SessionHarness(tester);
    await h.pump();
    h.fake.send(LeCommandType.LE_CMD_CLIP_LAUNCH, track: 0, slot: 0); // track đang chọn = 0
    await tester.pump();
    RenderObject boundaryAbove(RenderObject r) {
      RenderObject? n = r;
      while (n != null && !n.isRepaintBoundary) {
        n = n.parent;
      }
      return n!;
    }

    final loopLayer = boundaryAbove(
      tester.renderObject(
        find.descendant(of: find.byKey(const Key('session.loop')), matching: find.byType(CustomPaint)),
      ),
    );
    final header = boundaryAbove(tester.renderObject(find.byKey(const Key('header.name.0'))));
    h.fake.advanceBeats(0.5);
    h.c.read(engineStateTickerProvider).poll();
    expect(loopLayer.debugNeedsPaint, isTrue, reason: 'vòng tiến độ chạy');
    expect(header.debugNeedsPaint, isFalse);
    expect(identical(loopLayer, header), isFalse);
    await tester.pump();
    await h.unmount();
  });

  testWidgets('repaint: playhead piano roll + readout transport không làm bẩn lớp bên dưới', (tester) async {
    final h = SessionHarness(tester);
    await h.pump();
    await tester.tap(find.byKey(const Key('transport.edit')));
    await tester.pump();
    await tester.tap(cell(2, 0));
    await h.settle();
    h.fake.send(LeCommandType.LE_CMD_CLIP_LAUNCH, track: 2, slot: 0);
    await tester.pump();

    RenderObject boundaryAbove(RenderObject r) {
      RenderObject? n = r;
      while (n != null && !n.isRepaintBoundary) {
        n = n.parent;
      }
      return n!;
    }

    final roll = find.byKey(const Key('pianoRoll'));
    final paints = find.descendant(of: roll, matching: find.byType(CustomPaint));
    RenderObject paintOf<T>() =>
        tester.renderObjectList(paints).firstWhere((r) => r is RenderCustomPaint && r.painter is T);
    final notesLayer = boundaryAbove(paintOf<PianoRollPainter>());
    final playheadLayer = boundaryAbove(paintOf<ClipPlayheadPainter>());
    final readout = boundaryAbove(tester.renderObject(find.byKey(const Key('transport.readout'))));
    final topBar = boundaryAbove(readout.parent!);

    h.fake.advanceBeats(0.5);
    h.c.read(engineStateTickerProvider).poll();
    expect(playheadLayer.debugNeedsPaint, isTrue);
    expect(notesLayer.debugNeedsPaint, isFalse, reason: 'lớp nốt không vẽ lại theo playhead');
    expect(readout.debugNeedsPaint, isTrue);
    expect(topBar.debugNeedsPaint, isFalse, reason: 'readout có boundary riêng trong transport bar');
    expect(TransportReadoutPainter.position(h.fake.readState().beat, 4), isNotEmpty);
    await tester.pump();
    await h.unmount();
  });

  testWidgets(
    'repaint: kéo knob FX chỉ bẩn knob + ô giá trị (không bẩn card/slot khác); đồng hồ jam không bẩn transport',
    (tester) async {
      final exports = Directory.systemTemp.createTempSync('stress_exports_');
      addTearDown(() => exports.deleteSync(recursive: true));
      final h = SessionHarness(tester);
      h.fake.audioStart(); // ghi jam cần audio đang chạy
      await h.pump(project: stressProject(), extraOverrides: [exportsDirProvider.overrideWithValue(exports)]);
      await tester.tap(find.byKey(const Key('header.name.0')));
      await tester.tap(find.byKey(const Key('panel.tab.fx')));
      await h.settle();

      RenderObject boundaryAbove(RenderObject r) {
        RenderObject? n = r;
        while (n != null && !n.isRepaintBoundary) {
          n = n.parent;
        }
        return n!;
      }

      RenderObject knobLayer(String key) => boundaryAbove(
        tester.renderObject(find.descendant(of: find.byKey(Key(key)), matching: find.byType(CustomPaint))),
      );
      RenderObject valueLayer(String key) => boundaryAbove(
        tester.renderObject(find.descendant(of: find.byKey(Key(key)), matching: find.byType(Text)).last),
      );

      final knob = knobLayer('fx.param.1.1'); // delay feedback
      final value = valueLayer('fx.param.1.1');
      final card = boundaryAbove(knob.parent!);
      final otherCard = boundaryAbove(knobLayer('fx.param.0.0').parent!);
      final master = boundaryAbove(knobLayer('fx.master.0').parent!);
      expect(value, isNot(same(card)));

      final g = await tester.startGesture(tester.getCenter(find.byKey(const Key('fx.param.1.1'))));
      await g.moveBy(const Offset(0, -30));
      await tester.pump(null, EnginePhase.layout); // dựng + layout, dừng trước paint
      expect(knob.debugNeedsPaint, isTrue);
      expect(value.debugNeedsPaint, isTrue);
      expect(card.debugNeedsPaint, isFalse, reason: 'card slot đang kéo không vẽ lại');
      expect(otherCard.debugNeedsPaint, isFalse);
      expect(master.debugNeedsPaint, isFalse);
      await tester.pump();
      await g.up();
      await tester.pump();

      // Đồng hồ jam (1 Hz) trong transport bar.
      await tester.tap(find.byKey(const Key('transport.jam')));
      await tester.pump();
      final clock = boundaryAbove(
        tester.renderObject(find.descendant(of: find.byKey(const Key('transport.jam')), matching: find.byType(Text))),
      );
      final readout = boundaryAbove(tester.renderObject(find.byKey(const Key('transport.readout'))));
      final topBar = boundaryAbove(readout.parent!);
      await tester.pump(const Duration(seconds: 1), EnginePhase.layout);
      expect(clock.debugNeedsPaint, isTrue);
      expect(topBar.debugNeedsPaint, isFalse, reason: 'đồng hồ jam có ô cố định + boundary riêng');
      await tester.pump();
      await tester.tap(find.byKey(const Key('transport.jam')));
      await tester.pump(const Duration(seconds: 5));
      await h.unmount();
    },
  );
}
