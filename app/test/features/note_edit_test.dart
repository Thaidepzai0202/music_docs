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

  test('vẽ bằng kéo: cuối nốt phủ ô dưới ngón, ít nhất 1 ô, không vượt clip; chạm → độ dài nốt vẽ gần nhất', () {
    expect(NoteEdit.drawnLength(1.25, 2.1, grid: g, length: 4), 1.0); // ô chứa beat 2.1 là 2.0..2.25
    expect(NoteEdit.drawnLength(1.25, 0.3, grid: g, length: 4), 0.25); // kéo ngược → vẫn 1 ô
    expect(NoteEdit.drawnLength(3.0, 9, grid: g, length: 4), 1.0);
    expect(NoteEdit.add(const [], beat: 1.3, pitch: 60, grid: g, length: 4, d: 1.5).single.d, 1.5);
    expect(
      NoteEdit.add(const [], beat: 3.3, pitch: 60, grid: g, length: 4, d: 1.5).single.d,
      0.75,
      reason: 'kẹp cuối clip',
    );
  });

  const rows = [64, 62, 60, 59]; // trên → dưới (kiểu hàng pad của kit: không liền bán cung)

  test('dời nhóm: đầu nốt kéo snap lưới, cả nhóm cùng lệch; cao độ theo HÀNG; dừng ở biên clip / biên trục', () {
    const n = [Note(p: 60, v: 100, s: 1, d: 0.5), Note(p: 62, v: 90, s: 2, d: 1), Note(p: 59, v: 80, s: 0, d: 1)];
    final a = NoteEdit.moveGroup(n, {0, 1}, anchor: 0, dBeats: 0.3, dRows: -1, rows: rows, grid: g, length: 4);
    expect(a, const [
      Note(p: 62, v: 100, s: 1.25, d: 0.5),
      Note(p: 64, v: 90, s: 2.25, d: 1),
      Note(p: 59, v: 80, s: 0, d: 1),
    ]);
    // Kéo quá cuối clip / quá hàng trên cùng → cả nhóm dừng ở biên, giữ khoảng cách.
    final b = NoteEdit.moveGroup(n, {0, 1}, anchor: 1, dBeats: 10, dRows: -9, rows: rows, grid: g, length: 4);
    expect([b[0].s, b[1].s, b[0].p, b[1].p], [2.0, 3.0, 62, 64]);
    final c = NoteEdit.moveGroup(n, {0, 1}, anchor: 0, dBeats: -9, dRows: 9, rows: rows, grid: g, length: 4);
    expect([c[0].s, c[1].s, c[0].p, c[1].p], [0.0, 1.0, 59, 60]);
    // Nốt đơn = nhóm một phần tử.
    expect(
      NoteEdit.moveGroup(n, {2}, anchor: 2, dBeats: 0.6, dRows: -3, rows: rows, grid: g, length: 4)[2],
      const Note(p: 64, v: 80, s: 0.5, d: 1),
    );
  });

  test('đổi độ dài nhóm: cùng một lượng, mỗi nốt ≥ 1 ô, không vượt cuối clip', () {
    const n = [Note(p: 60, v: 100, s: 1, d: 0.25), Note(p: 62, v: 100, s: 3, d: 0.5), Note(p: 64, v: 100, s: 0, d: 1)];
    final a = NoteEdit.resizeGroup(n, {0, 1}, anchor: 0, endBeat: 2.1, grid: g, length: 4);
    expect([a[0].d, a[1].d, a[2].d], [1.0, 1.0, 1.0], reason: 'nốt 1 kẹp ở cuối clip (3 + 1 = 4)');
    final b = NoteEdit.resizeGroup(n, {0, 1}, anchor: 1, endBeat: 3.1, grid: g, length: 4);
    expect([b[0].d, b[1].d], [0.25, 0.25]);
    expect(NoteEdit.resizeGroup(n, {0}, anchor: 0, endBeat: 0.2, grid: g, length: 4)[0].d, 0.25);
    expect(NoteEdit.resizeGroup(n, {0}, anchor: 0, endBeat: 9, grid: g, length: 4)[0].d, 3.0);
  });

  test('nhân bản nhóm: đặt ngay sau nhóm, lệch = độ dài đoạn làm tròn lên theo lưới; chọn bản chép', () {
    const n = [Note(p: 60, v: 100, s: 0, d: 0.25), Note(p: 62, v: 90, s: 0.5, d: 0.3), Note(p: 64, v: 80, s: 3, d: 1)];
    final r = NoteEdit.duplicate(n, {1, 0}, grid: g);
    expect(r.notes.sublist(3), const [Note(p: 60, v: 100, s: 1, d: 0.25), Note(p: 62, v: 90, s: 1.5, d: 0.3)]);
    expect(r.copies, {3, 4});
    expect(r.end, closeTo(1.8, 1e-9));
    expect(NoteEdit.duplicate(n, {2}, grid: g).end, 5.0, reason: 'vượt clip 4 beat → phải hỏi tăng độ dài');
  });

  test('chọn theo đoạn: nốt BẮT ĐẦU trong [from, to), mọi cao độ', () {
    const n = [Note(p: 60, v: 100, s: 0.75, d: 1), Note(p: 90, v: 100, s: 1, d: 0.25), Note(p: 30, v: 100, s: 2, d: 1)];
    expect(NoteEdit.inRange(n, 1, 2), {1});
    expect(NoteEdit.inRange(n, 0, 4), {0, 1, 2});
  });

  test('velocity: nốt kéo nhận giá trị mới, cả nhóm theo cùng tỉ lệ, kẹp 1..127', () {
    const n = [Note(p: 60, v: 100, s: 0, d: 1), Note(p: 62, v: 50, s: 1, d: 1), Note(p: 64, v: 120, s: 2, d: 1)];
    final a = NoteEdit.velocity(n, {0, 1, 2}, anchor: 0, v: 50);
    expect(a.map((x) => x.v), [50, 25, 60]);
    final b = NoteEdit.velocity(n, {0, 2}, anchor: 0, v: 127);
    expect(b.map((x) => x.v), [127, 50, 127]);
    expect(NoteEdit.velocity(n, {1}, anchor: 1, v: -4)[1].v, 1);
  });

  test(
    'cột velocity: hợp âm (cùng đầu nốt) chung cột; kéo cột → nốt đã chọn nếu cột có nốt chọn, không thì cả cột',
    () {
      const n = [Note(p: 60, v: 100, s: 1, d: 1), Note(p: 64, v: 90, s: 0, d: 1), Note(p: 67, v: 80, s: 1, d: 0.5)];
      final cols = NoteEdit.velocityColumns(n);
      expect(cols, [
        [1],
        [0, 2],
      ]);
      expect(NoteEdit.velocityTargets(cols[1], const {}), {0, 2});
      expect(NoteEdit.velocityTargets(cols[1], const {2, 1}), {2, 1}, reason: 'cột có nốt chọn → cả lựa chọn');
      expect(NoteEdit.velocityTargets(cols[0], const {2}), {1}, reason: 'cột không có nốt chọn → chỉ cột đó');
    },
  );

  test('Step: hợp âm dài 1 ô tại con trỏ (thay nốt trùng), con trỏ tiến / lùi vòng trong clip, ⌫ xoá cả ô', () {
    const n = [Note(p: 60, v: 90, s: 1, d: 0.25), Note(p: 64, v: 90, s: 1.1, d: 0.25)];
    final a = NoteEdit.stepChord(n, {60: 100, 67: 80}, at: 1, grid: g, length: 4);
    expect(a, const [
      Note(p: 64, v: 90, s: 1.1, d: 0.25),
      Note(p: 60, v: 100, s: 1, d: 0.25),
      Note(p: 67, v: 80, s: 1, d: 0.25),
    ]);
    expect(NoteEdit.stepChord(const [], {60: 100}, at: 3.9, grid: g, length: 4).single.d, closeTo(0.1, 1e-9));
    expect([NoteEdit.stepNext(1, grid: g, length: 4), NoteEdit.stepNext(3.75, grid: g, length: 4)], [1.25, 0.0]);
    expect([NoteEdit.stepPrev(1, grid: g, length: 4), NoteEdit.stepPrev(0, grid: g, length: 4)], [0.75, 3.75]);
    expect(NoteEdit.clearCell(a, at: 1, grid: g), isEmpty, reason: 'nốt ở 1 và 1.1 đều nằm trong ô 1..1.25');
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
<region> key=41 sample=tom_floor_lo.wav volume=-2.8 region_label=Floor Tom L
<region> key=46 sample=hat_open.wav region_label=Open Hat group=2 off_by=1
<region> lokey=50 hikey=52 sample=tom.wav
''';
    expect(SfzPads.parse(sfz), {
      36: 'Kick',
      38: 'Snare top',
      42: 'Hat closed',
      49: 'Crash 1',
      41: 'Floor Tom L',
      46: 'Open Hat',
    });
  });
}
