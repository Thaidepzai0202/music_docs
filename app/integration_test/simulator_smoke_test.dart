// Smoke test chạy APP THẬT + ENGINE THẬT trên simulator iPad (không Fake): Onboarding → mở Demo → launch Scene 1 →
// clip phát → dừng. Chạy (vòng lặp cấp quyền micro ngay khi app vừa được cài, vì cài lại làm mất quyền):
//   (for i in $(seq 1 240); do xcrun simctl privacy <udid> grant microphone <bundle id> 2>/dev/null; sleep 0.5; done &)
//   flutter test integration_test/simulator_smoke_test.dart -d <udid> --dart-define=LOOPCORE_START=projects
import 'dart:ffi';

import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:music_looper/engine/engine_providers.dart';
import 'package:music_looper/features/session/project_controller.dart';
import 'package:music_looper/features/session/session_screen.dart';
import 'package:music_looper/main.dart' as app;

/// Op phase sau: engine thật được phép trả NOT_IMPLEMENTED.
const laterOps = {'midi.setMappings', 'link.enable'};

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  Future<void> pumpUntil(
    WidgetTester tester,
    bool Function() done,
    String what, {
    int seconds = 30,
    String Function()? diag,
  }) async {
    final end = DateTime.now().add(Duration(seconds: seconds));
    while (!done()) {
      if (DateTime.now().isAfter(end)) fail('quá $seconds s: $what${diag == null ? '' : ' — ${diag()}'}');
      await tester.pump(const Duration(milliseconds: 100));
    }
  }

  testWidgets('engine thật trên simulator: mở Demo, launch Scene 1, clip phát, không crash', (tester) async {
    await app.main();
    await pumpUntil(
      tester,
      () =>
          find.byKey(const Key('onboarding.mic')).evaluate().isNotEmpty ||
          find.byKey(const Key('projects.demo')).evaluate().isNotEmpty,
      'màn Onboarding hoặc Projects',
    );

    if (find.byKey(const Key('onboarding.mic')).evaluate().isNotEmpty) {
      // Quyền micro phải được cấp sẵn (simctl privacy grant ngay sau khi cài): hộp xin quyền của iOS chặn test.
      await tester.tap(find.byKey(const Key('onboarding.allowMic')));
      await pumpUntil(tester, () => find.byKey(const Key('onboarding.openDemo')).evaluate().isNotEmpty, 'bước Demo');
      await tester.tap(find.byKey(const Key('onboarding.openDemo')));
    } else {
      await tester.tap(find.byKey(const Key('projects.demo')));
    }
    await pumpUntil(tester, () => find.byType(SessionScreen).evaluate().isNotEmpty, 'màn Session');

    final c = ProviderScope.containerOf(tester.element(find.byType(SessionScreen)));
    final boot = c.read(engineBootstrapProvider);
    expect(boot.isFake, isFalse, reason: 'phải là engine thật (không LOOPCORE_FAKE)');
    expect(boot.createResult, LeError.LE_OK);
    await pumpUntil(tester, () => c.read(projectControllerProvider)?.isLoading == false, 'mở project Demo xong');
    final errors = c.read(projectControllerProvider)!.errors;
    expect(errors.where((e) => !(laterOps.contains(e.op) && e.code == 'NOT_IMPLEMENTED')), isEmpty);

    final engine = c.read(engineProvider);
    final audio = c.read(engineAudioProvider);
    final s0 = engine.readState();
    // ignore: avoid_print
    print(
      'SMOKE audio running=${audio.isRunning} outputOnly=${audio.outputOnly} '
      'sr=${s0.sampleRate} buf=${s0.bufferSize} info=${engine.call({'op': 'engine.info'})}',
    );
    await tester.tap(find.byKey(const Key('scene.0')));
    await pumpUntil(
      tester,
      () => engine.readState().clipState[0][0] == LeClipState.LE_CLIP_PLAYING,
      'clip Drums/Scene 1 phát',
      seconds: 10,
      diag: () {
        final st = engine.readState();
        return 'clipState[0][0]=${st.clipState[0][0]} beat=${st.beat} playing=${st.playing} '
            'publish=${st.publishCounter} audioRunning=${audio.isRunning}';
      },
    );
    final st = engine.readState();
    expect([st.clipState[0][0], st.clipState[1][0], st.clipState[2][0]], List.filled(3, LeClipState.LE_CLIP_PLAYING));
    expect(st.sampleRate, greaterThan(0));
    // Phát 3 giây rồi dừng: transport chạy, không xrun hàng loạt.
    await tester.pump(const Duration(seconds: 3));
    final beat = engine.readState().beat;
    expect(beat, greaterThan(2), reason: 'transport phải chạy');
    // ignore: avoid_print
    print('SMOKE: sampleRate=${st.sampleRate} buffer=${st.bufferSize} beat=$beat xrun=${engine.readState().xrunCount}');
    await tester.tap(find.byKey(const Key('session.stop')));
    await tester.pump(const Duration(milliseconds: 500));
  });
}
