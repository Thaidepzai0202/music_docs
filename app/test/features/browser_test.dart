// P2-22: Browser đọc Library/manifest.json (06 §4) — thư viện khởi đầu tự tổng hợp (P2-26 tạm).
import 'dart:convert';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/data/library_repository.dart';
import 'package:music_looper/features/browser/browser_panel.dart';
import 'package:music_looper/features/clip/pad_names.dart';
import 'package:music_looper/l10n/l10n.dart';
import 'package:music_looper/model/project.dart';
import 'package:music_looper/model/project_codec.dart';

import '../session_harness.dart';

class _FakeLibrary extends LibraryRepository {
  _FakeLibrary(this.m);
  final LibraryManifest m;
  final imports = <(String, String)>[];

  @override
  Future<LibraryManifest> manifest() async => m;

  @override
  Future<String> importLoop(LibraryItem loop, {required String projectDir, required String clipId}) async {
    imports.add((loop.id, clipId));
    return 'audio/$clipId.wav';
  }
}

/// Chữ nằm trong panel Browser (header track cũng có thể mang tên kit, 07 §4.1c).
Finder inBrowser(String text) => find.descendant(of: find.byType(BrowserPanel), matching: find.text(text));

void main() {
  final manifestJson = jsonDecode(File('assets/library/manifest.json').readAsStringSync()) as Map<String, dynamic>;
  final manifest = LibraryManifest.fromJson(manifestJson);

  test('manifest thật: đủ kit / nhạc cụ / loop, mọi file tham chiếu đều có trong assets', () {
    const kits = [
      'kit_808',
      'kit_909',
      'kit_perc',
      'kit_trap',
      'kit_lofi',
      'kit_606',
      'kit_707',
      'kit_linn',
      'kit_acoustic',
    ];
    expect(manifest.kits.map((k) => k.id), [...kits, 'kit_synth']);
    expect(manifest.kits.where((k) => k.hidden).map((k) => k.id), ['kit_synth'], reason: 'kit cũ ẩn, file vẫn còn');
    expect(manifest.kits.take(3).map((k) => k.nameFor('vi')), ['Kit 808', 'Kit 909', 'Bộ gõ']);
    expect(manifest.kits.every((k) => k.category == 'Drums'), isTrue);
    expect(manifest.kits.take(3).map((k) => k.tags.first), ['drums', 'drums', 'percussion']);
    // P2-30 / P2-35 (06 §4): mỗi kit 16 pad GM 36–51, pad nào cũng có region_label (tên hiện trên pad / piano roll).
    for (final k in manifest.kits.where((k) => !k.hidden)) {
      final pads = SfzPads.parse(File('assets/library/${k.path}').readAsStringSync());
      expect(pads.keys, [for (var n = 36; n <= 51; n++) n], reason: k.id);
      expect(pads[36], switch (k.id) {
        'kit_perc' => 'Conga Lo',
        'kit_trap' => '808 Kick',
        _ => 'Kick',
      }, reason: k.id);
      expect(pads.values.where((v) => v.endsWith('.wav') || v.endsWith('.flac')), isEmpty);
    }
    expect(SfzPads.parse(File('assets/library/kits/kit_808/kit_808.sfz').readAsStringSync())[41], 'Floor Tom L');
    expect(SfzPads.parse(File('assets/library/kits/kit_trap/kit_trap.sfz').readAsStringSync())[51], 'Riser');
    // Kit acoustic (Big Rusty Drums, CC0): 2 lớp velocity mỗi pad (32 region) → vẫn 16 pad, tên theo region đầu tiên.
    final acoustic = SfzPads.parse(File('assets/library/kits/kit_acoustic/kit_acoustic.sfz').readAsStringSync());
    expect([acoustic[39], acoustic[40], acoustic.length], ['Rim Click', 'Rimshot', 16]);
    // P2-35: Phím (Salamander + tổng hợp) → Dây / Kèn & sáo (VSCO-2 CE) → Synth.
    final byCategory = <String, List<String>>{};
    for (final i in manifest.instruments) {
      byCategory.putIfAbsent(i.category, () => []).add(i.id);
    }
    expect(byCategory, {
      'Instruments/Keys': ['inst_piano', 'inst_epiano', 'inst_organ'],
      'Instruments/Strings': [
        'inst_violin',
        'inst_viola',
        'inst_cello',
        'inst_contrabass',
        'inst_strings',
        'inst_pizzicato',
        'inst_harp',
      ],
      'Instruments/Winds & Brass': [
        'inst_flute',
        'inst_clarinet',
        'inst_oboe',
        'inst_trumpet',
        'inst_horn',
        'inst_trombone',
      ],
      'Instruments/Synth': ['inst_synth'],
    });
    expect(manifest.instruments.first.nameFor('vi'), 'Đại dương cầm');
    expect(manifest.instruments.first.license, contains('CC BY 3.0'));
    expect(manifest.loops.length, 3);
    for (final it in [...manifest.kits, ...manifest.instruments, ...manifest.loops]) {
      expect(File('assets/library/${it.path}').existsSync(), isTrue, reason: it.path);
      expect(it.license, isNotEmpty, reason: 'P2-26: mọi nội dung có license');
    }
    // Sample trong SFZ cũng phải có (engine đọc theo default_path=samples/).
    for (final sfz in [...manifest.kits, ...manifest.instruments]) {
      final dir = File('assets/library/${sfz.path}').parent.path;
      final text = File('assets/library/${sfz.path}').readAsStringSync();
      for (final m in RegExp(r'sample=(\S+)').allMatches(text)) {
        expect(File('$dir/samples/${m.group(1)}').existsSync(), isTrue, reason: m.group(1));
      }
    }
    expect(manifest.loops.first.bpm, 120);
    expect(manifest.loops.first.beats, 4);
  });

  test('pubspec khai báo đủ thư mục asset của thư viện', () {
    final pub = File('pubspec.yaml').readAsStringSync();
    for (final d in Directory('assets/library').listSync(recursive: true).whereType<Directory>()) {
      expect(pub, contains('- ${d.path}/'), reason: 'Flutter không quét thư mục con: ${d.path}');
    }
  });

  testWidgets('tab Browser: gán kit cho track đang chọn, thêm loop vào ô trống đầu tiên', (tester) async {
    final lib = _FakeLibrary(manifest);
    final h = SessionHarness(tester);
    await h.pump(extraOverrides: [libraryRepositoryProvider.overrideWithValue(lib)]);
    await tester.tap(find.byKey(const Key('header.name.4')));
    await tester.tap(find.byKey(const Key('panel.tab.browser')));
    await h.settle();
    expect(inBrowser('Kit 808'), findsOneWidget);
    expect(inBrowser('Bộ gõ'), findsNWidgets(2), reason: 'tên kit + nhãn tag percussion đã dịch');
    expect(find.text('Kit tổng hợp'), findsNothing, reason: 'kit_synth hidden');
    expect(find.byKey(const Key('browser.pick.kit_synth')), findsNothing);

    await tester.tap(find.byKey(const Key('browser.pick.kit_808')));
    await h.settle();
    final t4 = h.session.project.trackAt(4)!;
    expect(t4.kind, TrackKind.instrument);
    expect(t4.instrument, const InstrumentRef.sfz(path: 'kits/kit_808/kit_808.sfz'));
    expect(t4.name, 'Kit 808', reason: '07 §4.1c: "Track 5" chưa có tên riêng → mang tên kit');
    final ops = h.fake.calls.map((c) => c.op).toList();
    expect(ops.sublist(ops.length - 2), ['track.configure', 'track.setInstrument']);

    await tester.tap(find.byKey(const Key('browser.cat.loops'))); // 07 §4.1e: loop nằm ở danh mục Loops
    await h.settle();
    expect(find.text('Click 4 beat · 120'), findsOneWidget);
    await tester.tap(find.byKey(const Key('browser.pick.click_120')));
    await h.settle();
    final clip = h.session.project.trackAt(4)!.clipAt(0)! as AudioClip;
    expect(clip.name, 'Click 4 beat · 120');
    expect(clip.file, 'audio/${clip.id}.wav');
    expect([clip.lengthBeats, clip.originalBpm], [4.0, 120.0]);
    expect(lib.imports.single, ('click_120', clip.id));
    final req = h.fake.calls.lastWhere((c) => c.op == 'clip.setAudio').request;
    expect([req['track'], req['slot'], req['file']], [4, 0, clip.file]);
    expect(find.textContaining('Đã thêm'), findsOneWidget);
    expect(clip.warp, WarpMode.repitch, reason: 'manifest: click_120 defaultWarp = repitch');
    expect(req['warp'], 'repitch');

    await tester.pump(const Duration(seconds: 5)); // snackbar "Đã thêm" đóng — không che nút
    await h.settle();
    await tester.ensureVisible(find.byKey(const Key('browser.pick.sine_120')));
    await h.settle();
    await tester.tap(find.byKey(const Key('browser.pick.sine_120')));
    await h.settle();
    final sine = h.session.project.trackAt(4)!.clipAt(1)! as AudioClip;
    expect(sine.warp, WarpMode.stretch);
    expect(h.fake.calls.lastWhere((c) => c.op == 'clip.setAudio').request['warp'], 'stretch');
    await h.unmount();
  });

  testWidgets('06 §4 defaultWarp: loop trống gán Re-Pitch; đổi sang Stretch → hiện gợi ý Re-Pitch (en + vi)', (
    tester,
  ) async {
    final lib = _FakeLibrary(
      LibraryManifest.fromJson({
        'version': 1,
        'kits': const [],
        'instruments': const [],
        'loops': [
          {
            'id': 'beat_90',
            'name': 'Beat 90',
            'file': 'loops/clip_click_4beats_120.wav',
            'bpm': 90,
            'beats': 4,
            'tags': ['drums'],
            'license': 'test',
            'defaultWarp': 'repitch',
          },
          {
            'id': 'pad_90',
            'name': 'Pad 90',
            'file': 'loops/clip_sine_4beats_120.wav',
            'bpm': 90,
            'beats': 4,
            'tags': ['pad'],
            'license': 'test',
          },
        ],
      }),
    );
    for (final locale in L10n.supportedLocales) {
      final h = SessionHarness(tester);
      await h.pump(extraOverrides: [libraryRepositoryProvider.overrideWithValue(lib)], locale: locale);
      await tester.tap(find.byKey(const Key('header.name.4')));
      await tester.tap(find.byKey(const Key('panel.tab.browser')));
      await h.settle();
      await tester.tap(find.byKey(const Key('browser.cat.loops')));
      await h.settle();
      await tester.tap(find.byKey(const Key('browser.pick.beat_90')));
      await h.settle();
      await tester.tap(find.byKey(const Key('browser.pick.pad_90')));
      await h.settle();
      expect((h.session.project.trackAt(4)!.clipAt(0)! as AudioClip).warp, WarpMode.repitch);
      expect((h.session.project.trackAt(4)!.clipAt(0)! as AudioClip).tags, ['drums'], reason: '06 §4: chép tags');
      expect(
        (h.session.project.trackAt(4)!.clipAt(1)! as AudioClip).warp,
        WarpMode.stretch,
        reason: 'không khai → Stretch',
      );

      await tester.tap(find.byKey(const Key('transport.edit')));
      await tester.pump();
      await tester.tap(find.byKey(const Key('cell.4.0')));
      await h.settle();
      expect(find.byKey(const Key('audio.drumsHint')), findsNothing, reason: 'đang Re-Pitch');
      await tester.tap(find.descendant(of: find.byKey(const Key('audio.warp')), matching: find.text('Stretch')));
      await h.settle();
      expect(find.byKey(const Key('audio.drumsHint')), findsOneWidget);
      expect(find.text(S.clipDrumsHint), findsOneWidget);
      expect(S.clipDrumsHint, locale.languageCode == 'vi' ? contains('Loop trống') : contains('Drum loops'));

      // Loop không phải trống: Stretch mà không gợi ý.
      await tester.tap(find.byKey(const Key('cell.4.1')));
      await h.settle();
      expect(find.byKey(const Key('audio.drumsHint')), findsNothing);
      await h.unmount();
    }
  });

  test('manifest: name là chuỗi (= en) hoặc {en, vi}; thiếu ngôn ngữ → dùng en', () {
    final a = LibraryItem.fromJson(
      {'id': 'x', 'name': 'E-Piano', 'path': 'p.sfz'},
      pathKey: 'path',
      kind: LibraryKind.instrument,
    );
    expect([a.nameFor('vi'), a.nameFor('en')], ['E-Piano', 'E-Piano']);
    final b = LibraryItem.fromJson(
      {
        'id': 'y',
        'name': {'en': '808 Classic', 'vi': '808 Cổ điển'},
        'path': 'p.sfz',
      },
      pathKey: 'path',
      kind: LibraryKind.kit,
    );
    expect([b.nameFor('vi'), b.nameFor('en'), b.nameFor('fr')], ['808 Cổ điển', '808 Classic', '808 Classic']);
    final c = LibraryItem.fromJson(
      {
        'id': 'z',
        'name': {'en': 'Only en'},
        'path': 'p.sfz',
      },
      pathKey: 'path',
      kind: LibraryKind.kit,
    );
    expect(c.nameFor('vi'), 'Only en');
  });

  testWidgets('[en] Browser: tên tiếng Anh + nhãn tag đã dịch; clip gán lấy tên theo ngôn ngữ lúc gán', (tester) async {
    final lib = _FakeLibrary(manifest);
    final h = SessionHarness(tester);
    await h.pump(extraOverrides: [libraryRepositoryProvider.overrideWithValue(lib)], locale: const Locale('en'));
    await tester.tap(find.byKey(const Key('header.name.4')));
    await tester.tap(find.byKey(const Key('panel.tab.browser')));
    await h.settle();
    expect(inBrowser('808 Kit'), findsOneWidget);
    expect(inBrowser('Percussion'), findsNWidgets(2), reason: 'tags id tiếng Anh → nhãn ARB (percussion)');
    await tester.tap(find.byKey(const Key('browser.cat.loops')));
    await h.settle();
    await tester.tap(find.byKey(const Key('browser.pick.click_120')));
    await h.settle();
    expect(h.session.project.trackAt(4)!.clipAt(0)!.name, 'Click 4 beats · 120');
    await h.unmount();
  });

  testWidgets('clips[].tags lưu vào project: mở lại → gợi ý Re-Pitch của loop trống vẫn còn', (tester) async {
    final d = demoProject();
    final drumsLoop = const Clip.audio(
      slot: 0,
      id: 'c_drums',
      name: 'Beat 90',
      file: 'audio/c_drums.wav',
      lengthBeats: 4,
      originalBpm: 90,
      tags: ['drums'],
    );
    final saved = ProjectCodec().encode(
      d.copyWith(
        tracks: [
          for (final t in d.tracks) t.index == 4 ? t.copyWith(clips: [drumsLoop]) : t,
        ],
      ),
    );
    final reopened = ProjectCodec().decode(saved).project;
    expect((reopened.trackAt(4)!.clipAt(0)! as AudioClip).tags, ['drums']);
    final h = SessionHarness(tester);
    await h.pump(project: reopened);
    await tester.tap(find.byKey(const Key('transport.edit')));
    await tester.pump();
    await tester.tap(find.byKey(const Key('cell.4.0')));
    await h.settle();
    expect(find.byKey(const Key('audio.drumsHint')), findsOneWidget, reason: 'Stretch + tag drums sau khi mở lại');
    await h.unmount();
  });

  test('LibraryRepository.importLoop chép file vào audio/ của project', () async {
    TestWidgetsFlutterBinding.ensureInitialized();
    final dir = Directory.systemTemp.createTempSync('lib_import_');
    addTearDown(() => dir.deleteSync(recursive: true));
    final bytes = File('assets/library/loops/clip_sine_4beats_120.wav').readAsBytesSync();
    final repo = LibraryRepository(bundle: _BytesBundle({'assets/library/loops/clip_sine_4beats_120.wav': bytes}));
    final rel = await repo.importLoop(manifest.loops.last, projectDir: dir.path, clipId: 'c_x');
    expect(rel, 'audio/c_x.wav');
    expect(File('${dir.path}/$rel').lengthSync(), bytes.length);
  });
}

class _BytesBundle extends CachingAssetBundle {
  _BytesBundle(this.files);
  final Map<String, List<int>> files;

  @override
  Future<ByteData> load(String key) async => ByteData.sublistView(Uint8List.fromList(files[key]!));
}
