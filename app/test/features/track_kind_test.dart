// P2-32 (07 §4.1c): track không chia loại cố định — project mới / demo là "Track 1–8" (demo: track đã gán nhạc cụ
// mang tên nhạc cụ); gán kit / nhạc cụ → track nhạc cụ, tự đổi tên nếu chưa có tên riêng; gán loop → track audio;
// chỉ đổi loại khi track còn trống.
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/data/library_repository.dart';
import 'package:music_looper/features/projects/demo_project.dart';
import 'package:music_looper/features/session/project_controller.dart';
import 'package:music_looper/l10n/l10n.dart';
import 'package:music_looper/model/ids.dart';
import 'package:music_looper/model/project.dart';

import '../session_harness.dart';

class _Library extends LibraryRepository {
  @override
  Future<LibraryManifest> manifest() async => testLibrary();

  @override
  Future<String> importLoop(LibraryItem loop, {required String projectDir, required String clipId}) async =>
      'audio/$clipId.wav';
}

void main() {
  final vi = lookupAppLocalizations(const Locale('vi'));

  test('project mới: 8 track trống "Track 1–8", chưa có nhạc cụ', () {
    final p = newProject('X', trackName: vi.sessionTrackName);
    expect(p.tracks.map((t) => t.name), [for (var n = 1; n <= 8; n++) 'Track $n']);
    expect(p.tracks.every((t) => t.clips.isEmpty && t.instrument == null), isTrue);
  });

  test('demo: track đã gán nhạc cụ mang tên nhạc cụ theo ngôn ngữ; không còn Drums / Bass / Keys / Lead', () {
    for (final (locale, names) in [
      ('vi', ['Kit 808', 'Tone tổng hợp', 'Piano điện', 'Organ']),
      ('en', ['808 Kit', 'Synth Tone', 'E-Piano', 'Organ']),
    ]) {
      final l = lookupAppLocalizations(Locale(locale));
      final p = localizeDemo(demoProject(), l, library: testLibrary());
      expect(p.tracks.map((t) => t.name), [...names, 'Track 5', 'Track 6', 'Track 7', 'Track 8']);
      expect(p.tracks.take(4).every((t) => t.kind == TrackKind.instrument && t.clips.isNotEmpty), isTrue);
    }
  });

  Future<SessionHarness> browserOn(WidgetTester tester, int track) async {
    final h = SessionHarness(tester);
    await h.pump(extraOverrides: [libraryRepositoryProvider.overrideWithValue(_Library())]);
    await tester.tap(find.byKey(Key('header.name.$track')));
    await tester.tap(find.byKey(const Key('panel.tab.browser')));
    await h.settle();
    return h;
  }

  Future<void> pick(WidgetTester tester, SessionHarness h, String id) async {
    // 07 §4.1e: mở đúng danh mục của mục (kit → Drums, loop → Loops).
    await tester.tap(find.byKey(Key('browser.cat.${id.startsWith('kit_') ? 'drums' : 'loops'}')));
    await h.settle();
    await tester.ensureVisible(find.byKey(Key('browser.pick.$id')));
    await h.settle();
    await tester.tap(find.byKey(Key('browser.pick.$id')));
    await h.settle();
  }

  testWidgets('gán kit cho track trống → track nhạc cụ mang tên kit; gán kit khác → đổi theo; đã đặt tên riêng → giữ', (
    tester,
  ) async {
    final h = await browserOn(tester, 5);
    await pick(tester, h, 'kit_808');
    var t = h.session.project.trackAt(5)!;
    expect([t.kind, t.name], [TrackKind.instrument, 'Kit 808']);
    final configure = h.fake.calls.lastWhere((c) => c.op == 'track.configure').request;
    expect([configure['kind'], configure['name']], ['instrument', 'Kit 808']);

    await pick(tester, h, 'kit_909');
    expect(h.session.project.trackAt(5)!.name, 'Kit 909', reason: 'tên cũ là tên kit (tự động) → đổi theo kit mới');

    h.c.read(projectControllerProvider.notifier).renameTrack(5, 'Beat của tôi');
    await h.settle();
    await pick(tester, h, 'kit_perc');
    t = h.session.project.trackAt(5)!;
    expect(t.name, 'Beat của tôi', reason: 'tên riêng của người dùng không bị đổi');
    expect(t.instrument, const InstrumentRef.sfz(path: 'kits/kit_perc/kit_perc.sfz'));
    await h.unmount();
  });

  testWidgets('loop vào track nhạc cụ còn trống → thành track audio, tên về "Track N"; track đang có clip thì không '
      'đổi loại (báo, không thêm / không gán)', (tester) async {
    final h = await browserOn(tester, 5);
    await pick(tester, h, 'kit_808');
    await pick(tester, h, 'click_120');
    final t = h.session.project.trackAt(5)!;
    expect([t.kind, t.name, t.instrument], [TrackKind.audio, 'Track 6', null]);
    expect(t.clipAt(0), isA<AudioClip>());

    // Track audio đang có clip → gán kit bị từ chối.
    await tester.pump(const Duration(seconds: 5)); // SnackBar "Đã thêm" đóng
    await h.settle();
    await pick(tester, h, 'kit_808');
    expect(find.text(vi.browserTrackAudioCoClip), findsOneWidget);
    expect(h.session.project.trackAt(5)!.kind, TrackKind.audio);

    // Track nhạc cụ đang có clip MIDI (track 1 của demo) → loop bị từ chối.
    await tester.pump(const Duration(seconds: 5));
    await h.settle();
    await tester.tap(find.byKey(const Key('header.name.0')));
    await h.settle();
    final clips = h.session.project.trackAt(0)!.clips.length;
    await pick(tester, h, 'click_120');
    expect(find.text(vi.browserTrackNhacCuCoClip), findsOneWidget);
    expect(h.session.project.trackAt(0)!.kind, TrackKind.instrument);
    expect(h.session.project.trackAt(0)!.clips.length, clips);
    await h.unmount();
  });
}
