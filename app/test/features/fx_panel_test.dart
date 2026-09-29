// P3-16: panel FX — thêm/đổi loại/xoá (fx.set / fx.remove), knob FX_PARAM ≤ 1 lệnh/frame, bypass, master EQ3,
// lưu vào tracks[].fx.
import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/features/fx/fx_specs.dart';
import 'package:music_looper/model/project.dart';

import '../session_harness.dart';

void main() {
  Future<SessionHarness> openFx(WidgetTester tester, {Project? project}) async {
    final h = SessionHarness(tester);
    await h.pump(project: project);
    await tester.tap(find.byKey(const Key('panel.tab.fx')));
    await h.settle();
    return h;
  }

  Project withFx(List<FxSlot> fx) {
    final d = demoProject();
    return d.copyWith(tracks: [for (final t in d.tracks) t.index == 0 ? t.copyWith(fx: fx) : t]);
  }

  List<FxSlot> fxOf(SessionHarness h, [int track = 0]) => h.session.project.trackAt(track)!.fx;

  testWidgets('thêm FX: fx.set với tham số mặc định 04 §9, model + engine khớp; slot kế tiếp mới có nút thêm', (
    tester,
  ) async {
    final h = await openFx(tester);
    expect(find.byKey(const Key('fx.add.0')), findsOneWidget);
    expect(
      find.byKey(const Key('fx.add.1')),
      findsNothing,
      reason: 'slot liền nhau: chỉ slot trống đầu tiên thêm được',
    );
    await tester.tap(find.byKey(const Key('fx.add.0')));
    await h.settle();
    await tester.tap(find.byKey(const Key('fx.pick.delay')));
    await h.settle();

    final call = h.fake.calls.lastWhere((c) => c.op == 'fx.set').request;
    expect(call, {
      'op': 'fx.set',
      'track': 0,
      'index': 0,
      'type': 'delay',
      'params': {'0': 4.0, '1': 0.35, '2': 0.3, '3': 0.0},
      'bypass': false,
    });
    expect(fxOf(h).single.type, FxType.delay);
    expect(h.fake.fx[(0, 0)]!.type, 'delay');
    expect(find.byKey(const Key('fx.add.1')), findsOneWidget);
    await h.unmount();
  });

  testWidgets('kéo knob: FX_PARAM tối đa 1 lệnh/frame, chưa ghi model; thả tay → model = giá trị cuối', (tester) async {
    final h = await openFx(
      tester,
      project: withFx([FxSlot(type: FxType.reverb, params: fxDefaultParams(FxType.reverb))]),
    );
    final before = h.fake.sent.length;
    final g = await tester.startGesture(tester.getCenter(find.byKey(const Key('fx.param.0.3')))); // mix
    for (var i = 0; i < 8; i++) {
      await g.moveBy(const Offset(0, -5));
    }
    await tester.pump();
    final during = h.sentSince(before).where((s) => s.name == 'FX_PARAM').toList();
    expect(during.length, 2, reason: '8 lần move trong 1 frame → gửi ngay 1 + gộp 1 ở frame sau');
    expect(during.every((s) => s.track == 0 && s.slot == 0 && s.i0 == 3), isTrue);
    expect(fxOf(h).single.params['3'], 0.25, reason: 'đang kéo: chưa ghi model (không rebuild card mỗi frame)');

    await g.up();
    await tester.pump();
    final last = h.fake.sent.lastWhere((s) => s.name == 'FX_PARAM');
    expect(fxOf(h).single.params['3'], closeTo(last.f0, 1e-6));
    expect(fxOf(h).single.params['3']!, greaterThan(0.25));
    expect(h.fake.fx[(0, 0)]!.params['3'], closeTo(last.f0, 1e-6));
    await h.unmount();
  });

  testWidgets('bypass: FX_BYPASS rồi model; đổi loại → fx.set loại mới; chọn nhịp delay → FX_PARAM chỉ số', (
    tester,
  ) async {
    final h = await openFx(
      tester,
      project: withFx([FxSlot(type: FxType.delay, params: fxDefaultParams(FxType.delay))]),
    );
    await tester.tap(find.byKey(const Key('fx.bypass.0')));
    await tester.pump();
    final b = h.fake.sent.last;
    expect([b.name, b.track, b.slot, b.i0], ['FX_BYPASS', 0, 0, 1]);
    expect(fxOf(h).single.bypass, isTrue);
    expect(h.fake.fx[(0, 0)]!.bypass, isTrue);

    await tester.tap(find.byKey(const Key('fx.param.0.0'))); // nhịp delay
    await h.settle();
    await tester.tap(find.byKey(const Key('fx.choice.5')));
    await h.settle();
    final p = h.fake.sent.last;
    expect([p.name, p.slot, p.i0, p.f0], ['FX_PARAM', 0, 0, 5.0]);
    expect(find.text(delayNoteLabels[5]), findsOneWidget);

    await tester.tap(find.byKey(const Key('fx.type.0')));
    await h.settle();
    await tester.tap(find.byKey(const Key('fx.pick.comp')).last);
    await h.settle();
    expect(h.fake.calls.last.request['type'], 'comp');
    expect(fxOf(h).single.type, FxType.comp);
    expect(fxOf(h).single.params.length, 5);
    expect(find.byKey(const Key('fx.param.0.4')), findsOneWidget);
    await h.unmount();
  });

  testWidgets('xoá slot 0 trong 3: slot sau dồn lên (fx.set lại) rồi fx.remove slot cuối; engine = model', (
    tester,
  ) async {
    final h = await openFx(
      tester,
      project: withFx([
        FxSlot(type: FxType.filter, params: fxDefaultParams(FxType.filter)),
        const FxSlot(type: FxType.delay, bypass: true, params: {'0': 7}),
        FxSlot(type: FxType.reverb, params: fxDefaultParams(FxType.reverb)),
      ]),
    );
    final before = h.fake.log.length;
    await tester.tap(find.byKey(const Key('fx.remove.0')));
    await h.settle();
    final ops = h.fake.log.sublist(before).whereType<FakeCall>().map((c) => c.request).toList();
    expect(ops.map((r) => [r['op'], r['index'], r['type']]), [
      ['fx.set', 0, 'delay'],
      ['fx.set', 1, 'reverb'],
      ['fx.remove', 2, null],
    ]);
    expect(ops.first['bypass'], isTrue, reason: 'bypass đi kèm fx.set');
    expect(fxOf(h).map((f) => f.type), [FxType.delay, FxType.reverb]);
    expect(h.fake.fx.keys.where((k) => k.$1 == 0).toSet(), {(0, 0), (0, 1)});
    expect(find.byKey(const Key('fx.add.2')), findsOneWidget);
    await h.unmount();
  });

  testWidgets('master EQ3: kéo → FX_PARAM track −1 slot 0; thả tay → master.eq3; autosave lưu cả tracks[].fx', (
    tester,
  ) async {
    final h = await openFx(
      tester,
      project: withFx([FxSlot(type: FxType.eq3, params: fxDefaultParams(FxType.eq3))]),
    );
    final g = await tester.startGesture(tester.getCenter(find.byKey(const Key('fx.master.2'))));
    await g.moveBy(const Offset(0, -20));
    await tester.pump();
    await g.up();
    await tester.pump();
    final s = h.fake.sent.lastWhere((x) => x.name == 'FX_PARAM');
    expect([s.track, s.slot, s.i0], [-1, 0, 2]);
    expect(h.session.project.master.eq3[2], closeTo(s.f0, 1e-6));
    expect(s.f0, greaterThan(0));

    await tester.pump(const Duration(seconds: 3)); // autosave debounce 2 s
    final saved = h.repo.saves.last.$2;
    expect(saved.master.eq3[2], closeTo(s.f0, 1e-6));
    expect(saved.trackAt(0)!.fx.single.type, FxType.eq3);
    await h.unmount();
  });

  testWidgets('trần limiter master: kéo → FX_PARAM track −1 slot 1 p0; thả tay → master.limiterCeilingDb (−12..0)', (
    tester,
  ) async {
    final h = await openFx(tester);
    final g = await tester.startGesture(tester.getCenter(find.byKey(const Key('fx.master.limiter'))));
    await g.moveBy(const Offset(0, 40)); // kéo xuống = hạ trần
    await tester.pump();
    await g.up();
    await tester.pump();
    final s = h.fake.sent.lastWhere((x) => x.name == 'FX_PARAM');
    expect([s.track, s.slot, s.i0], [-1, 1, 0]);
    expect(s.f0, inInclusiveRange(-12, -0.3));
    expect(s.f0, lessThan(-0.3));
    expect(h.session.project.master.limiterCeilingDb, closeTo(s.f0, 1e-6));
    expect(h.fake.master[(1, 0)], closeTo(s.f0, 1e-6));
    await h.unmount();
  });

  test('ParamSpec: log/tuyến tính đi-về 0..1, định dạng đơn vị', () {
    final cutoff = fxSpecs[FxType.filter]![1];
    expect(cutoff.denormalize(cutoff.normalize(1000)), closeTo(1000, 1e-6));
    expect(cutoff.normalize(20), 0);
    expect(cutoff.normalize(20000), closeTo(1, 1e-9));
    expect(cutoff.format(1500), '1.5 kHz');
    expect(fxSpecs[FxType.comp]![1].format(4), '4.0:1');
    expect(fxSpecs[FxType.eq3]![0].format(-3), '-3.0 dB');
    expect(fxSpecs[FxType.delay]![0].format(4), '1/8');
    expect(delayNoteLabels, hasLength(11), reason: 'khớp DelayFx::kNoteBeats');
  });
}
