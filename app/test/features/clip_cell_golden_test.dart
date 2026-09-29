// P2-07: 8 trạng thái ô (07 §2) + thiếu file + ô trống khi track arm.
// Hàng trên: beat 0.25 (nhấp nháy sáng). Hàng dưới: beat 0.75 (nhấp nháy tối).
// Đổi hình vẽ có chủ đích: flutter test --update-goldens test/features/clip_cell_golden_test.dart
import 'dart:ffi' hide Size;

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/app/theme.dart';
import 'package:music_looper/engine/engine_state.dart';
import 'package:music_looper/engine/engine_state_ticker.dart';
import 'package:music_looper/features/session/widgets/clip_cell.dart';

import '../test_utils.dart';

typedef _Case = (ClipState state, {bool armed, bool missing});

const cases = <_Case>[
  (ClipState.empty, armed: false, missing: false),
  (ClipState.empty, armed: true, missing: false),
  (ClipState.stopped, armed: false, missing: false),
  (ClipState.queuedPlay, armed: false, missing: false),
  (ClipState.queuedStop, armed: false, missing: false),
  (ClipState.playing, armed: false, missing: false),
  (ClipState.queuedRecord, armed: true, missing: false),
  (ClipState.recording, armed: true, missing: false),
  (ClipState.overdubbing, armed: false, missing: false),
  (ClipState.stopped, armed: false, missing: true),
];

void main() {
  testWidgets('ClipCell: 8 trạng thái + missing', (tester) async {
    tester.view.physicalSize = const Size(1400, 300);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.reset);
    final rows = <Widget>[];
    for (final beat in [0.25, 0.75]) {
      final fake = createFakeEngine();
      final ticker = EngineStateTicker(fake);
      addTearDown(ticker.dispose);
      fake.state
        ..publishCounter = 1
        ..beat = beat;
      for (var i = 0; i < cases.length; i++) {
        fake.state.trackPlayingSlot[i % 8] = 0;
      }
      for (var t = 0; t < 8; t++) {
        fake.state.trackClipProgress[t] = 0.6;
      }
      ticker.poll();
      rows.add(
        Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            for (var i = 0; i < cases.length; i++)
              SizedBox(
                width: 128,
                height: 54,
                child: CustomPaint(
                  painter: ClipCellPainter(
                    state: cases[i].$1,
                    color: AppColors.tracks[i % 8],
                    armed: cases[i].armed,
                    missing: cases[i].missing,
                    ticker: ClipCellPainter.animates(cases[i].$1) ? ticker : null,
                    track: i % 8,
                    slot: 0,
                  ),
                ),
              ),
          ],
        ),
      );
    }
    await tester.pumpWidget(
      MaterialApp(
        debugShowCheckedModeBanner: false,
        home: ColoredBox(
          color: AppColors.background,
          child: Center(
            child: RepaintBoundary(
              key: const Key('golden'),
              child: ColoredBox(
                color: AppColors.background,
                child: Column(mainAxisSize: MainAxisSize.min, children: rows),
              ),
            ),
          ),
        ),
      ),
    );
    await expectLater(find.byKey(const Key('golden')), matchesGoldenFile('goldens/clip_cell_states.png'));
  });
}
