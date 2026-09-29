// P3-07: nhạc cụ tự thu — Natural/Classic (instrument.setMode) + knob ADSR (instrument.setEnvelope),
// lưu vào userInstruments[].envelope; mở lại project gửi envelope đã lưu.
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/features/instrument/envelope_editor.dart';
import 'package:music_looper/model/project.dart';

import '../../session_harness.dart';

void main() {
  const inst = UserInstrument(
    id: 'i_voice',
    name: 'Tiếng thu 1',
    source: 'instruments/i_voice/source.caf',
    rootNote: 57,
    envelope: Envelope(a: 0.01, d: 0.3, s: 0.7, r: 0.5),
  );

  Project withUserInstrument() {
    final d = demoProject();
    return d.copyWith(
      userInstruments: const [inst],
      tracks: [
        for (final t in d.tracks) t.index == 3 ? t.copyWith(instrument: const InstrumentRef.user(id: 'i_voice')) : t,
      ],
    );
  }

  Future<SessionHarness> openInstrument(WidgetTester tester, int track) async {
    final h = SessionHarness(tester);
    await h.pump(project: withUserInstrument());
    await tester.tap(find.byKey(Key('header.name.$track')));
    await tester.tap(find.byKey(const Key('panel.tab.instrument')));
    await h.settle();
    return h;
  }

  UserInstrument current(SessionHarness h) => h.session.project.userInstruments.single;

  testWidgets('mở project: envelope đã lưu được gửi ngay sau createFromRecording', (tester) async {
    final h = await openInstrument(tester, 3);
    final ops = h.fake.calls.map((c) => c.op).toList();
    final i = ops.indexOf('instrument.createFromRecording');
    expect(ops[i + 1], 'instrument.setEnvelope');
    expect(h.fake.envelopes['i_voice'], (a: 0.01, d: 0.3, s: 0.7, r: 0.5));
    await h.unmount();
  });

  testWidgets('chỉ track có nhạc cụ tự thu mới hiện khối ADSR', (tester) async {
    final h = await openInstrument(tester, 1);
    expect(find.byKey(const Key('instrument.editor')), findsNothing);
    await tester.tap(find.byKey(const Key('header.name.3')));
    await h.settle();
    expect(find.byKey(const Key('instrument.editor')), findsOneWidget);
    expect(find.text('Tiếng thu 1'), findsOneWidget);
    await h.unmount();
  });

  testWidgets('Natural → Classic: instrument.setMode rồi model', (tester) async {
    final h = await openInstrument(tester, 3);
    await tester.tap(find.text('Classic'));
    await h.settle();
    expect(h.fake.calls.last.request, {'op': 'instrument.setMode', 'instrumentId': 'i_voice', 'mode': 'classic'});
    expect(current(h).mode, InstrumentMode.classic);
    await h.unmount();
  });

  testWidgets('kéo knob R: setEnvelope ≤ 1 lần/frame lúc kéo (giữ A/D/S), thả tay → userInstruments[].envelope', (
    tester,
  ) async {
    final h = await openInstrument(tester, 3);
    final before = h.fake.calls.length;
    final g = await tester.startGesture(tester.getCenter(find.byKey(const Key('instrument.env.r'))));
    for (var i = 0; i < 6; i++) {
      await g.moveBy(const Offset(0, -6));
    }
    await tester.pump();
    final during = h.fake.calls.skip(before).where((c) => c.op == 'instrument.setEnvelope').toList();
    expect(during.length, 2, reason: '6 lần move trong 1 frame → gửi ngay 1 + gộp 1 ở frame sau');
    expect(current(h).envelope.r, 0.5, reason: 'đang kéo: chưa ghi model');
    final req = during.last.request;
    expect([req['a'], req['d'], req['s']], [0.01, 0.3, 0.7]);
    expect(req['r'] as double, greaterThan(0.5));

    await g.up();
    await tester.pump();
    expect(current(h).envelope.r, closeTo(h.fake.envelopes['i_voice']!.r, 1e-9));
    expect(current(h).envelope.r, greaterThan(0.5));
    expect(current(h).envelope.a, 0.01);
    await tester.pump(const Duration(seconds: 3));
    expect(h.repo.saves.last.$2.userInstruments.single.envelope.r, current(h).envelope.r);
    await h.unmount();
  });

  test('dải ADSR: sàn khớp engine (attack ≥ 0.5 ms, decay/release ≥ 5 ms), mặc định = Envelope()', () {
    const e = Envelope();
    expect([for (final s in envelopeSpecs) s.defaultValue], [e.a, e.d, e.s, e.r]);
    expect(envelopeSpecs[0].min, 0.0005);
    expect(envelopeSpecs[1].min, 0.005);
    expect(envelopeSpecs[3].min, 0.005);
    expect(envelopeSpecs[3].format(1.5), '1.50 s');
    expect(envelopeSpecs[0].format(0.005), '5 ms');
  });
}
