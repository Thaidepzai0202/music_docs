// P2-29 (07 §4.1b): phép sửa nốt thuần dữ liệu + undo/redo 50 bước + tên pad từ SFZ.
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/features/clip/note_edit.dart';
import 'package:music_looper/features/clip/pad_names.dart';
import 'package:music_looper/model/project.dart';

void main() {
  const g = NoteEdit.defaultGrid; // 1/16 = 0.25 beat

  test('thêm nốt: snap XUỐNG ô chứa điểm chạm, dài 1 ô, velocity 100, không vượt cuối clip', () {
    final a = NoteEdit.add(const [], beat: 1.37, pitch: 60, grid: g, length: 4);
    expect(a.single, const Note(p: 60, v: 100, s: 1.25, d: 0.25));
    final b = NoteEdit.add(const [], beat: 3.99, pitch: 200, grid: 1, length: 4);
    expect([b.single.s, b.single.d, b.single.p], [3.0, 1.0, 127]);
  });

  test('dời nốt: snap gần nhất, giữ độ dài, không ra ngoài clip; cao độ kẹp 0..127', () {
    const n = [Note(p: 60, v: 100, s: 1, d: 0.5)];
    expect(
      NoteEdit.move(n, 0, dBeats: 0.3, dPitch: 2, grid: g, length: 4).single,
      const Note(p: 62, v: 100, s: 1.25, d: 0.5),
    );
    expect(NoteEdit.move(n, 0, dBeats: 10, dPitch: 0, grid: g, length: 4).single.s, 3.5);
    expect(
      NoteEdit.move(n, 0, dBeats: -5, dPitch: -100, grid: g, length: 4).single,
      const Note(p: 0, v: 100, s: 0, d: 0.5),
    );
  });

  test('kéo mép phải: snap, tối thiểu 1 ô, không vượt cuối clip', () {
    const n = [Note(p: 60, v: 100, s: 1, d: 0.25)];
    expect(NoteEdit.resize(n, 0, endBeat: 2.1, grid: g, length: 4).single.d, 1.0);
    expect(NoteEdit.resize(n, 0, endBeat: 0.2, grid: g, length: 4).single.d, 0.25);
    expect(NoteEdit.resize(n, 0, endBeat: 9, grid: g, length: 4).single.d, 3.0);
  });

  test('đổi độ dài clip: bỏ nốt từ cuối trở đi, cắt nốt tràn', () {
    const n = [Note(p: 60, v: 90, s: 0, d: 1), Note(p: 62, v: 90, s: 3.5, d: 1), Note(p: 64, v: 90, s: 4, d: 1)];
    expect(NoteEdit.fitLength(n, 4), const [Note(p: 60, v: 90, s: 0, d: 1), Note(p: 62, v: 90, s: 3.5, d: 0.5)]);
  });

  test('undo/redo: đúng thứ tự, thao tác mới xoá nhánh redo, giữ tối đa 50 bước', () {
    final h = NoteHistory();
    const a = <Note>[];
    const b = [Note(p: 60, v: 100, s: 0, d: 1)];
    h.push(a, 4); // trước khi thêm nốt → b
    expect(h.canUndo, isTrue);
    final u = h.undo(b, 4)!;
    expect([u.notes, u.length], [a, 4]);
    expect(h.canRedo, isTrue);
    final r = h.redo(a, 4)!;
    expect(r.notes, b);
    h.push(b, 4);
    expect(h.canRedo, isFalse);
    for (var i = 0; i < 80; i++) {
      h.push(b, 4);
    }
    var n = 0;
    while (h.undo(b, 4) != null) {
      n++;
    }
    expect(n, NoteHistory.maxSteps);
  });

  test('SFZ kit → tên pad theo key: region_label ưu tiên, không thì tên file sample', () {
    const sfz = '''
<control> default_path=samples/
<region> key=36 sample=kick.wav
<region> key=38 sample=snare_top.wav   // comment
<region> lokey=42 hikey=42 sample=hat_closed.wav group=1
<region> key=49 sample=crash.wav region_label=Crash 1
<region> lokey=50 hikey=52 sample=tom.wav
''';
    expect(SfzPads.parse(sfz), {36: 'Kick', 38: 'Snare top', 42: 'Hat closed', 49: 'Crash'});
  });
}
