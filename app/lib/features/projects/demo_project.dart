import 'package:flutter/services.dart';

import '../../data/project_repository.dart';
import '../../l10n/l10n.dart';
import '../../model/ids.dart';
import '../../model/project.dart';
import '../../model/project_codec.dart';

const demoAsset = 'assets/demo/demo_project.json';

/// Tạo bản sao project demo (asset) trong `Projects/` với id + thời điểm mới, tên theo ngôn ngữ LÚC TẠO.
/// Trả thư mục `.loopproj`.
Future<String> createDemoProject(ProjectRepository repo) async {
  final text = await rootBundle.loadString(demoAsset);
  final demo = localizeDemo(ProjectCodec().decode(text).project, S);
  final now = DateTime.now().toUtc();
  return repo.create(demo.copyWith(id: newId('p'), createdAt: now, modifiedAt: now));
}

/// Tên trong asset demo là tên gốc tiếng Anh ("Drums", "Beat A", "Track 5"…). Dịch sang ngôn ngữ [l] qua ARB
/// (`demoName`, `sessionTrackName`, `sessionSceneName`). Chỉ dùng lúc tạo: project đã tạo là dữ liệu người dùng,
/// đổi ngôn ngữ máy không đổi tên. Tên không có trong ARB giữ nguyên.
Project localizeDemo(Project p, AppLocalizations l) {
  String name(String n) {
    if (RegExp(r'^Track (\d+)$').firstMatch(n) case final m?) return l.sessionTrackName(int.parse(m[1]!));
    if (RegExp(r'^Scene (\d+)$').firstMatch(n) case final m?) return l.sessionSceneName(int.parse(m[1]!));
    final token = _token(n);
    final out = l.demoName(token);
    return out == token ? n : out;
  }

  return p.copyWith(
    name: name(p.name),
    scenes: [for (final s in p.scenes) s.copyWith(name: name(s.name))],
    tracks: [
      for (final t in p.tracks)
        t.copyWith(
          name: name(t.name),
          clips: [for (final c in t.clips) c.copyWith(name: name(c.name))],
        ),
    ],
  );
}

/// "Beat A" → "beatA", "Half-time" → "halfTime".
String _token(String n) {
  final words = n.split(RegExp('[^A-Za-z0-9]+')).where((w) => w.isNotEmpty).toList();
  if (words.isEmpty) return n;
  return words.first.toLowerCase() + words.skip(1).map((w) => w[0].toUpperCase() + w.substring(1).toLowerCase()).join();
}
