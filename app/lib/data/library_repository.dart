import 'dart:convert';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../model/project.dart';

/// Loại mục thư viện — theo mảng của manifest (`kits` / `instruments` / `loops`).
enum LibraryKind { kit, instrument, loop }

/// Một mục trong `Library/manifest.json` (06 §4).
@immutable
final class LibraryItem {
  const LibraryItem({
    required this.id,
    required this.kind,
    required this.names,
    required this.path,
    required this.license,
    this.category = '',
    this.range,
    this.bpm,
    this.beats,
    this.tags = const [],
    this.defaultWarp,
    this.hidden = false,
  });

  factory LibraryItem.fromJson(Map<String, dynamic> j, {required String pathKey, required LibraryKind kind}) =>
      LibraryItem(
        id: j['id'] as String,
        kind: kind,
        names: _names(j['name'], fallback: j['id'] as String),
        path: j[pathKey] as String,
        license: j['license'] as String? ?? '',
        category: (j['category'] as String?)?.trim() ?? _defaultCategory[kind]!,
        range: switch (j['range']) {
          [final int lo, final int hi] when lo <= hi => (lo, hi),
          _ => null,
        },
        bpm: (j['bpm'] as num?)?.toDouble(),
        beats: (j['beats'] as num?)?.toDouble(),
        tags: [for (final t in (j['tags'] as List? ?? const [])) '$t'],
        defaultWarp: WarpMode.values.asNameMap()[j['defaultWarp']],
        hidden: j['hidden'] == true,
      );

  final String id;
  final LibraryKind kind;

  /// Thư mục trong Browser (06 §4: đường dẫn như "Instruments/Keys", "Loops/Drums"); thiếu thì theo loại.
  final String category;

  static const _defaultCategory = {
    LibraryKind.kit: 'Drums',
    LibraryKind.instrument: 'Instruments',
    LibraryKind.loop: 'Loops',
  };

  /// Dải phím tự nhiên của nhạc cụ (manifest `range`, MIDI [thấp, cao]); null = không ghi. Ngoài dải vẫn kêu (engine
  /// kéo cao độ), bàn phím chỉ đánh dấu nhạt hơn.
  final (int, int)? range;

  /// Khoá ổn định (★ Yêu thích lưu trong settings.json): "kit:kit_808".
  String get key => '${kind.name}:$id';

  /// Không hiện trong Browser nhưng vẫn nằm trong bundle (kit cũ `kit_synth`: project cũ còn mở được, P2-30).
  final bool hidden;

  /// Tên theo ngôn ngữ (06 §4: `name` là object `{en, vi}` hoặc chuỗi = tiếng Anh).
  final Map<String, String> names;

  /// Tên theo [lang] ("vi" / "en"); thiếu thì dùng `en`.
  String nameFor(String lang) => names[lang] ?? names['en'] ?? names.values.first;

  static Map<String, String> _names(Object? v, {required String fallback}) => switch (v) {
    String() => {'en': v},
    Map() when v.isNotEmpty => {for (final e in v.entries) '${e.key}': '${e.value}'},
    _ => {'en': fallback},
  };

  /// Tương đối so với thư mục thư viện (kit/instrument: `.sfz`, loop: file audio).
  final String path;
  final String license;
  final double? bpm;
  final double? beats;
  final List<String> tags;

  /// `tags`: id tiếng Anh cố định (`drums`, `bass`…) — UI hiện nhãn đã dịch qua ARB `libraryTag` (06 §4).
  /// Warp ban đầu khi gán loop (06 §4 `defaultWarp`): loop trống (`drums`) để `repitch`; null = stretch.
  final WarpMode? defaultWarp;

  bool get isDrums => tags.contains('drums');
}

@immutable
final class LibraryManifest {
  const LibraryManifest({required this.kits, required this.instruments, required this.loops});

  factory LibraryManifest.fromJson(Map<String, dynamic> j) => LibraryManifest(
    kits: [
      for (final e in (j['kits'] as List? ?? const []))
        LibraryItem.fromJson(e as Map<String, dynamic>, pathKey: 'path', kind: LibraryKind.kit),
    ],
    instruments: [
      for (final e in (j['instruments'] as List? ?? const []))
        LibraryItem.fromJson(e as Map<String, dynamic>, pathKey: 'path', kind: LibraryKind.instrument),
    ],
    loops: [
      for (final e in (j['loops'] as List? ?? const []))
        LibraryItem.fromJson(e as Map<String, dynamic>, pathKey: 'file', kind: LibraryKind.loop),
    ],
  );

  final List<LibraryItem> kits;
  final List<LibraryItem> instruments;
  final List<LibraryItem> loops;

  /// Mọi mục (kit → nhạc cụ → loop), kể cả `hidden`.
  List<LibraryItem> get all => [...kits, ...instruments, ...loops];

  /// Kit / nhạc cụ theo đường dẫn SFZ (`InstrumentRef.sfz.path`), kể cả mục `hidden`.
  LibraryItem? instrumentAt(String path) {
    for (final it in [...kits, ...instruments]) {
      if (it.path == path) return it;
    }
    return null;
  }
}

/// Thư viện âm thanh đóng gói trong app (P2-22). Nằm trong Flutter asset `assets/library/`;
/// engine đọc cùng thư mục đó qua đường dẫn thật (`EnginePlatform.assetPath`, truyền vào LeConfig.libraryDir).
class LibraryRepository {
  LibraryRepository({AssetBundle? bundle}) : _bundle = bundle ?? rootBundle;

  static const root = 'assets/library';
  final AssetBundle _bundle;

  Future<LibraryManifest> manifest() async =>
      LibraryManifest.fromJson(jsonDecode(await _bundle.loadString('$root/manifest.json')) as Map<String, dynamic>);

  /// Chép loop vào `audio/` của project để project tự đủ file (06 §2: đường dẫn tương đối so với project).
  /// Trả đường dẫn tương đối, ví dụ `audio/c_<uuid>.wav`.
  Future<String> importLoop(LibraryItem loop, {required String projectDir, required String clipId}) async {
    final data = await _bundle.load('$root/${loop.path}');
    final dot = loop.path.lastIndexOf('.');
    final ext = dot < 0 ? '.wav' : loop.path.substring(dot);
    final rel = 'audio/$clipId$ext';
    final f = File('$projectDir/$rel');
    await f.parent.create(recursive: true);
    await f.writeAsBytes(data.buffer.asUint8List(data.offsetInBytes, data.lengthInBytes), flush: true);
    return rel;
  }
}

final libraryRepositoryProvider = Provider<LibraryRepository>((ref) => LibraryRepository());

/// Dải phím tự nhiên của nhạc cụ SFZ [sfzPath] (manifest `range`) — bàn phím đánh dấu dải này; null nếu không có.
final instrumentRangeProvider = Provider.family<(int, int)?, String>(
  (ref, sfzPath) => ref.watch(libraryManifestProvider).value?.instrumentAt(sfzPath)?.range,
);

final libraryManifestProvider = FutureProvider<LibraryManifest>(
  (ref) => ref.watch(libraryRepositoryProvider).manifest(),
);
