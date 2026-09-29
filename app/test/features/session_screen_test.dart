// P2-06 → P2-09: layout Session, ClipCell (pointer-down), track header, cột scene + Stop all.
// Chạy với FakeEngineClient có mô phỏng ClipScheduler (04 §3).
import 'dart:ffi';
import 'dart:io';

import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/material.dart';
import 'package:flutter/widgets.dart' as widgets show debugOnRebuildDirtyWidget;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_riverpod/misc.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/app/theme.dart';
import 'package:music_looper/data/data_providers.dart';
import 'package:music_looper/engine/engine_providers.dart';
import 'package:music_looper/engine/engine_state.dart';
import 'package:music_looper/features/session/project_controller.dart';
import 'package:music_looper/features/session/session_layout.dart';
import 'package:music_looper/features/session/session_screen.dart';
import 'package:music_looper/features/session/widgets/clip_cell.dart';
import 'package:music_looper/model/project.dart';
import 'package:music_looper/model/project_codec.dart';
import 'package:music_looper/ui_kit/fader.dart';

import '../test_utils.dart';

Project demoProject() => ProjectCodec().decode(File('assets/demo/demo_project.json').readAsStringSync()).project;

void main() {
  late FakeEngineClient fake;
  late ProviderContainer c;
  late MemoryProjectRepository repo;

  List<Override> sessionOverrides() => [
    ...engineOverrides(fake),
    projectRepositoryProvider.overrideWithValue(repo = MemoryProjectRepository()),
  ];

  setUp(() {
    fake = createFakeEngine();
    mockEnginePlatform();
  });

  Future<void> pumpSession(WidgetTester tester, {Project? project}) async {
    useIpad8Screen(tester);
    c = ProviderContainer(overrides: sessionOverrides());
    addTearDown(c.dispose);
    // runAsync: job của Fake xong bằng Timer thật; trong FakeAsync của testWidgets sẽ không tự chạy.
    await tester.runAsync(() => c.read(projectControllerProvider.notifier).open(project ?? demoProject(), dir: '/p'));
    await tester.pumpWidget(
      UncontrolledProviderScope(
        container: c,
        child: MaterialApp(theme: buildAppTheme(), home: const SessionScreen()),
      ),
    );
    await tester.pump();
  }

  /// Cho autosave (debounce 2 s) chạy xong rồi gỡ cây widget — không để Timer treo.
  Future<void> unmount(WidgetTester tester) async {
    await tester.pump(const Duration(seconds: 3));
    await tester.pumpWidget(const SizedBox());
  }

  Finder cell(int t, int s) => find.byKey(Key('cell.$t.$s'));
  ClipState cellState(int t, int s) => c.read(engineStateTickerProvider).clip(t, s).value;

  /// Chạm xuống rồi nhấc; trả số lệnh engine nhận được NGAY khi chạm (trước frame nào).
  Future<List<FakeSend>> pointerDown(WidgetTester tester, Finder f) async {
    final before = fake.sent.length;
    final g = await tester.startGesture(tester.getCenter(f));
    final sentOnDown = fake.sent.sublist(before);
    await g.up();
    return sentOnDown;
  }

  group('P2-06 layout (07 §2, màn 1080×810)', () {
    testWidgets('kích thước top bar / header / ô / cột scene / panel', (tester) async {
      await pumpSession(tester);
      expect(tester.getSize(find.byKey(const Key('session.topBar'))).height, SessionLayout.topBarHeight);
      expect(tester.getSize(find.byKey(const Key('session.headerRow'))).height, SessionLayout.headerHeight);
      final cellSize = tester.getSize(cell(0, 0));
      expect(cellSize.width, (1080 - SessionLayout.sceneColumnWidth) / 8);
      expect(cellSize.height, greaterThanOrEqualTo(SessionLayout.minCellHeight));
      expect(
        cellSize.height,
        (810 - SessionLayout.topBarHeight - SessionLayout.headerHeight - SessionLayout.panelHeight) / 8,
      );
      expect(cellSize.height, greaterThanOrEqualTo(SessionLayout.minCellHeight));
      expect(tester.getSize(find.byKey(const Key('scene.0'))).width, SessionLayout.sceneColumnWidth);
      expect(tester.getSize(find.byKey(const Key('session.panel'))).height, SessionLayout.panelHeight);
      await unmount(tester);
    });

    testWidgets('thu gọn panel → grid giãn ra, KHÔNG ô nào rebuild', (tester) async {
      await pumpSession(tester);
      final before = tester.getSize(cell(0, 0)).height;
      final rebuilt = <String>[];
      widgets.debugOnRebuildDirtyWidget = (e, _) => rebuilt.add('${e.widget.runtimeType}');
      addTearDown(() => widgets.debugOnRebuildDirtyWidget = null);

      await tester.tap(find.byKey(const Key('session.panel.toggle')));
      await tester.pump();
      widgets.debugOnRebuildDirtyWidget = null;

      expect(tester.getSize(find.byKey(const Key('session.panel'))).height, SessionLayout.panelCollapsedHeight);
      expect(
        tester.getSize(cell(0, 0)).height,
        (810 - SessionLayout.topBarHeight - SessionLayout.headerHeight - SessionLayout.panelCollapsedHeight) / 8,
      );
      expect(tester.getSize(cell(0, 0)).height, greaterThan(before));
      expect(rebuilt.where((w) => w.contains('ClipCell') || w.contains('ClipState') || w == 'TrackHeader'), isEmpty);
      await unmount(tester);
    });
  });

  group('⤢ mở rộng panel tab Clip', () {
    testWidgets('bật ⤢: panel ~70% màn hình, hàng grid giữ 44 pt và cuộn được, KHÔNG ô nào rebuild; đổi tab → về cũ', (
      tester,
    ) async {
      await pumpSession(tester);
      await tester.tap(find.byKey(const Key('transport.edit')));
      await tester.pump();
      await tester.tap(cell(0, 0)); // chọn clip → tab Clip
      await tester.pump(const Duration(milliseconds: 300));
      expect(find.byKey(const Key('session.panel.expand')), findsOneWidget);
      final rebuilt = <String>[];
      widgets.debugOnRebuildDirtyWidget = (e, _) => rebuilt.add('${e.widget.runtimeType}');
      addTearDown(() => widgets.debugOnRebuildDirtyWidget = null);
      await tester.tap(find.byKey(const Key('session.panel.expand')));
      await tester.pump();
      widgets.debugOnRebuildDirtyWidget = null;
      expect(tester.getSize(find.byKey(const Key('session.panel'))).height, closeTo(810 * 0.7, 0.01));
      expect(tester.getSize(cell(0, 0)).height, SessionLayout.minCellHeight);
      final scroll = tester.widget<SingleChildScrollView>(find.byKey(const Key('session.gridScroll')));
      expect(scroll.physics, isNull, reason: 'không đủ chỗ cho 8 × 44 pt → cho cuộn');
      expect(rebuilt.where((w) => w.contains('ClipCell') || w.contains('ClipState') || w == 'TrackHeader'), isEmpty);

      await tester.tap(find.byKey(const Key('panel.tab.mixer')));
      await tester.pump();
      expect(tester.getSize(find.byKey(const Key('session.panel'))).height, SessionLayout.panelHeight);
      expect(find.byKey(const Key('session.panel.expand')), findsNothing);
      await unmount(tester);
    });

    testWidgets('thu gọn khi đang mở rộng → mở lại là cỡ thường', (tester) async {
      await pumpSession(tester);
      await tester.tap(find.byKey(const Key('panel.tab.clip')));
      await tester.pump();
      await tester.tap(find.byKey(const Key('session.panel.expand')));
      await tester.pump();
      await tester.tap(find.byKey(const Key('session.panel.toggle')));
      await tester.pump();
      expect(tester.getSize(find.byKey(const Key('session.panel'))).height, SessionLayout.panelCollapsedHeight);
      await tester.tap(find.byKey(const Key('session.panel.toggle')));
      await tester.pump();
      expect(tester.getSize(find.byKey(const Key('session.panel'))).height, SessionLayout.panelHeight);
      await unmount(tester);
    });
  });

  group('P2-07 ClipCell', () {
    testWidgets('pointer-down ô có clip → CLIP_LAUNCH ngay trong sự kiện chạm (0 frame)', (tester) async {
      await pumpSession(tester);
      final sent = await pointerDown(tester, cell(0, 1));
      expect(sent.single.name, 'CLIP_LAUNCH');
      expect([sent.single.track, sent.single.slot], [0, 1]);
      await tester.pump();
      expect(cellState(0, 1), ClipState.playing, reason: 'transport dừng → phát ngay');
      await unmount(tester);
    });

    testWidgets('ô trống, track không arm → dừng track; arm rồi chạm → thu 4 bar', (tester) async {
      await pumpSession(tester);
      var sent = await pointerDown(tester, cell(5, 0));
      expect(sent.single.name, 'CLIP_STOP');
      expect(sent.single.track, 5);

      await tester.tap(find.byKey(const Key('header.arm.5')));
      await tester.pump();
      expect(fake.sent.last.name, 'TRACK_ARM');
      expect(fake.sent.last.i0, 1);

      sent = await pointerDown(tester, cell(5, 0));
      expect(sent.single.name, 'CLIP_RECORD');
      expect(sent.single.i0, 4);
      await tester.pump();
      expect(cellState(5, 0), ClipState.recording, reason: 'transport dừng → thu từ beat 0');
      await unmount(tester);
    });

    testWidgets('queued nhấp nháy rồi Playing đúng ranh giới bar (theo beat của engine)', (tester) async {
      await pumpSession(tester);
      await pointerDown(tester, cell(0, 0)); // khởi động transport
      fake.advanceBeats(1.5);
      await pointerDown(tester, cell(1, 0));
      await tester.pump();
      expect(cellState(1, 0), ClipState.queuedPlay);
      fake.advanceBeats(2.6); // qua beat 4
      await tester.pump();
      expect(cellState(1, 0), ClipState.playing);
      await unmount(tester);
    });

    test('nhấp nháy bám theo beat: sáng nửa đầu mỗi phách', () {
      expect(ClipCellPainter.blinkOn(0.1), isTrue);
      expect(ClipCellPainter.blinkOn(0.6), isFalse);
      expect(ClipCellPainter.blinkOn(3.25), isTrue);
      expect(ClipCellPainter.blinkOn(3.75), isFalse);
    });

    testWidgets('đổi trạng thái 1 ô → chỉ đúng 1 ô rebuild và chỉ boundary của ô đó bẩn', (tester) async {
      await pumpSession(tester);
      final ticker = c.read(engineStateTickerProvider);
      final rebuilt = <String>[];
      widgets.debugOnRebuildDirtyWidget = (e, _) => rebuilt.add('${e.widget.runtimeType}');
      addTearDown(() => widgets.debugOnRebuildDirtyWidget = null);

      fake.send(LeCommandType.LE_CMD_CLIP_LAUNCH, track: 1, slot: 0);
      ticker.poll(); // như 1 frame của Ticker

      RenderObject boundaryOf(Finder f) {
        RenderObject? r = tester.renderObject(find.descendant(of: f, matching: find.byType(CustomPaint)).first);
        while (r != null && !r.isRepaintBoundary) {
          r = r.parent;
        }
        return r!;
      }

      // Grid nằm trong LayoutBuilder (cuộn khi panel mở rộng) → ô con rebuild trong pha layout (BuildScope của
      // LayoutBuilder) → dừng sau layout, trước paint.
      await tester.pump(Duration.zero, EnginePhase.layout);
      widgets.debugOnRebuildDirtyWidget = null;
      expect(rebuilt.where((w) => w.contains('ClipState')).length, 1, reason: '$rebuilt');
      expect(boundaryOf(cell(1, 0)).debugNeedsPaint, isTrue);
      expect(boundaryOf(cell(5, 5)).debugNeedsPaint, isFalse, reason: 'ô đứng yên không vẽ lại');
      expect(boundaryOf(cell(0, 0)).debugNeedsPaint, isFalse);
      await tester.pump();
      await unmount(tester);
    });

    testWidgets('clip thiếu file → vẽ trạng thái missing', (tester) async {
      useIpad8Screen(tester);
      c = ProviderContainer(overrides: sessionOverrides());
      addTearDown(c.dispose);
      final p = ProjectCodec().decode(File('test/fixtures/projects/example_v1.json').readAsStringSync()).project;
      await tester.runAsync(
        () => c.read(projectControllerProvider.notifier).open(p, dir: '/p', missingClipIds: {'c_3f…'}),
      );
      await tester.pumpWidget(
        UncontrolledProviderScope(
          container: c,
          child: MaterialApp(theme: buildAppTheme(), home: const SessionScreen()),
        ),
      );
      final painter =
          tester.widget<CustomPaint>(find.descendant(of: cell(1, 0), matching: find.byType(CustomPaint)).first).painter
              as ClipCellPainter;
      expect(painter.missing, isTrue);
      await unmount(tester);
    });
  });

  group('P2-08 track header', () {
    testWidgets('M / S / Arm gửi đúng lệnh và cập nhật model', (tester) async {
      await pumpSession(tester);
      await tester.tap(find.byKey(const Key('header.mute.2')));
      await tester.pump();
      expect([fake.sent.last.name, fake.sent.last.track, fake.sent.last.i0], ['TRACK_MUTE', 2, 1]);
      expect(c.read(projectControllerProvider)!.project.trackAt(2)!.mixer.mute, isTrue);

      await tester.tap(find.byKey(const Key('header.solo.3')));
      await tester.pump();
      expect([fake.sent.last.name, fake.sent.last.track, fake.sent.last.i0], ['TRACK_SOLO', 3, 1]);

      await tester.tap(find.byKey(const Key('header.mute.2')));
      await tester.pump();
      expect(fake.sent.last.i0, 0);
      expect(c.read(projectControllerProvider)!.project.trackAt(2)!.mixer.mute, isFalse);

      await tester.tap(find.byKey(const Key('header.arm.1')));
      await tester.pump();
      expect([fake.sent.last.name, fake.sent.last.i0], ['TRACK_ARM', 1]);
      expect(c.read(projectControllerProvider)!.armed, {1});
      await unmount(tester);
    });

    testWidgets('meter + ô đang phát chạy 60fps mà KHÔNG rebuild widget', (tester) async {
      await pumpSession(tester);
      await pointerDown(tester, cell(0, 0));
      await tester.pump();
      final rebuilt = <String>[];
      widgets.debugOnRebuildDirtyWidget = (e, _) => rebuilt.add('${e.widget.runtimeType}');
      addTearDown(() => widgets.debugOnRebuildDirtyWidget = null);
      for (var i = 0; i < 30; i++) {
        fake.advanceBeats(0.1); // trong bar đầu: không đổi trạng thái ô
        for (var t = 0; t < 8; t++) {
          fake.state.trackPeak[t][0] = (i % 10) / 10;
          fake.state.trackPeak[t][1] = (i % 7) / 7;
        }
        await tester.pump(const Duration(milliseconds: 16));
      }
      expect(rebuilt, isEmpty);
      await unmount(tester);
    });
  });

  group('P2-09 scene + Stop all', () {
    testWidgets('▶ scene → SCENE_LAUNCH; ■ → STOP_ALL (ngay khi chạm)', (tester) async {
      await pumpSession(tester);
      var sent = await pointerDown(tester, find.byKey(const Key('scene.2')));
      expect([sent.single.name, sent.single.slot], ['SCENE_LAUNCH', 2]);
      await tester.pump();
      expect(cellState(0, 2), ClipState.playing);
      expect(cellState(2, 2), ClipState.playing);

      fake.advanceBeats(1); // rời ranh giới bar (đứng đúng ranh giới thì dừng ngay, 04 §3.2)
      sent = await pointerDown(tester, find.byKey(const Key('session.stopAll')));
      expect(sent.single.name, 'STOP_ALL');
      await tester.pump();
      expect(cellState(0, 2), ClipState.queuedStop);
      fake.advanceBeats(3.1);
      await tester.pump();
      expect(cellState(0, 2), ClipState.stopped);
      await unmount(tester);
    });
  });

  group('mixer (thử ui_kit)', () {
    testWidgets('kéo fader → TRACK_GAIN tối đa 1 lệnh/frame, model cập nhật', (tester) async {
      await pumpSession(tester);
      final before = fake.sent.where((s) => s.name == 'TRACK_GAIN').length;
      final g = await tester.startGesture(tester.getCenter(find.byKey(const Key('mixer.fader.0'))));
      for (var i = 0; i < 8; i++) {
        await g.moveBy(const Offset(0, -4));
      }
      await tester.pump();
      final during = fake.sent.where((s) => s.name == 'TRACK_GAIN').skip(before).toList();
      expect(during.length, 2, reason: '8 lần move trong 1 frame → gửi ngay 1 + gộp 1 ở frame sau');
      expect(during.every((s) => s.track == 0), isTrue);
      expect(
        c.read(projectControllerProvider)!.project.trackAt(0)!.mixer.gainDb,
        0,
        reason: 'đang kéo: chỉ gửi lệnh, chưa ghi model (strip không rebuild mỗi frame)',
      );
      await g.up();
      await tester.pump();
      final gains = fake.sent.where((s) => s.name == 'TRACK_GAIN').skip(before).toList();
      expect(gains.last.f0, closeTo(during.last.f0, 1e-6), reason: 'thả tay: commit đúng giá trị cuối');
      expect(c.read(projectControllerProvider)!.project.trackAt(0)!.mixer.gainDb, closeTo(gains.last.f0, 1e-6));
      expect(tester.widget<Fader>(find.byKey(const Key('mixer.fader.0'))).value, greaterThan(GainScale.toNorm(0)));
      await unmount(tester);
    });
  });

  group('P2-25 + thu xong', () {
    testWidgets('tai nghe Bluetooth → banner cảnh báo', (tester) async {
      await pumpSession(tester);
      fake.emit(const RouteChanged(wiredOrInterface: false, bluetooth: true));
      await tester.pump();
      expect(find.textContaining('Bluetooth'), findsOneWidget);
      fake.emit(const RouteChanged(wiredOrInterface: true, bluetooth: false));
      await tester.pump();
      expect(find.textContaining('Bluetooth'), findsNothing);
      await unmount(tester);
    });

    testWidgets('thu 4 bar xong → clip xuất hiện trong model và trên ô', (tester) async {
      await pumpSession(tester);
      await tester.tap(find.byKey(const Key('header.arm.6')));
      await tester.pump();
      await pointerDown(tester, cell(6, 3));
      fake.advanceBeats(16.1); // 4 bar
      await tester.pump();
      await tester.pump();
      final clip = c.read(projectControllerProvider)!.project.trackAt(6)!.clipAt(3);
      expect(clip, isA<AudioClip>());
      expect((clip! as AudioClip).lengthBeats, 16);
      expect(find.descendant(of: cell(6, 3), matching: find.text('Bản thu 4')), findsOneWidget);
      expect(cellState(6, 3), ClipState.playing);
      // 06 §5.2: thu xong → lưu ngay, không đợi debounce 2 s.
      expect(repo.saves, isNotEmpty);
      expect(repo.saves.last.$2.trackAt(6)!.clipAt(3), isA<AudioClip>());
      await unmount(tester);
    });

    testWidgets('đang mở project → banner tiến độ', (tester) async {
      fake = createFakeEngine(autoCompleteJobs: false);
      useIpad8Screen(tester);
      c = ProviderContainer(overrides: sessionOverrides());
      addTearDown(c.dispose);
      c.read(projectControllerProvider.notifier).open(demoProject(), dir: '/p'); // không await: job chưa xong
      await tester.pumpWidget(
        UncontrolledProviderScope(
          container: c,
          child: MaterialApp(theme: buildAppTheme(), home: const SessionScreen()),
        ),
      );
      expect(find.textContaining('Đang mở project… 0/4'), findsOneWidget);
      await unmount(tester);
    });
  });
}
