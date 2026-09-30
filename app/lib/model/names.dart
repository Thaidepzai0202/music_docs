import 'project.dart';

/// Ký tự điều khiển C0 (U+0000–U+001F, gồm NUL, tab, xuống dòng) và DEL (U+007F). Engine từ chối JSON có NUL
/// (05 §1: JUCE không đọc được); các ký tự còn lại vô hình, làm hỏng tên thư mục `.loopproj` và hiển thị.
final controlChars = RegExp('[\u0000-\u001F\u007F]');

/// Tên người dùng nhập (project, track, scene, clip, nhạc cụ): bỏ ký tự điều khiển rồi trim.
/// Kết quả rỗng = coi như không nhập → nơi gọi giữ tên cũ.
String cleanName(String s) => s.replaceAll(controlChars, '').trim();

/// Project đọc từ file (bản cũ, sửa tay): làm sạch mọi tên để không chuỗi nào có ký tự điều khiển đi xuống engine
/// (`track.configure {name}`) hay vào tên thư mục. Tên thành rỗng: project → "Project", còn lại → "" (UI hiện tên
/// mặc định "Track N" / "Scene N").
Project cleanNames(Project p) {
  bool dirty(String s) => controlChars.hasMatch(s) || s != s.trim();
  final any =
      dirty(p.name) ||
      p.scenes.any((s) => dirty(s.name)) ||
      p.userInstruments.any((u) => dirty(u.name)) ||
      p.tracks.any((t) => dirty(t.name) || t.clips.any((c) => dirty(c.name)));
  if (!any) return p;
  final name = cleanName(p.name);
  return p.copyWith(
    name: name.isEmpty ? 'Project' : name,
    scenes: [for (final s in p.scenes) s.copyWith(name: cleanName(s.name))],
    userInstruments: [for (final u in p.userInstruments) u.copyWith(name: cleanName(u.name))],
    tracks: [
      for (final t in p.tracks)
        t.copyWith(
          name: cleanName(t.name),
          clips: [for (final c in t.clips) c.copyWith(name: cleanName(c.name))],
        ),
    ],
  );
}

/// Tìm không phân biệt hoa thường và dấu tiếng Việt ("bo go" khớp "Bộ gõ").
String foldSearch(String s) {
  const from = 'àáạảãâầấậẩẫăằắặẳẵèéẹẻẽêềếệểễìíịỉĩòóọỏõôồốộổỗơờớợởỡùúụủũưừứựửữỳýỵỷỹđ';
  const to = 'aaaaaaaaaaaaaaaaaeeeeeeeeeeeiiiiiooooooooooooooooouuuuuuuuuuuyyyyyd';
  final b = StringBuffer();
  for (final ch in s.toLowerCase().split('')) {
    final i = from.indexOf(ch);
    b.write(i < 0 ? ch : to[i]);
  }
  return b.toString();
}
