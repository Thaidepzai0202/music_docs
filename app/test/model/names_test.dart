// 05 §1: engine từ chối JSON có NUL (U+0000). Mọi tên người dùng nhập bỏ ký tự điều khiển C0 + DEL; chỉ toàn ký tự
// bị lọc = tên rỗng → giữ tên cũ. Project đọc từ file cũng được làm sạch trước khi đi xuống engine.
import 'dart:convert';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/data/project_repository.dart';
import 'package:music_looper/features/session/project_controller.dart';
import 'package:music_looper/model/ids.dart';
import 'package:music_looper/model/names.dart';
import 'package:music_looper/model/project_codec.dart';
import 'package:music_looper/ui_kit/name_dialog.dart';

import '../session_harness.dart';

void main() {
  test('cleanName: bỏ NUL, tab, xuống dòng, mọi C0 + DEL rồi trim; giữ dấu tiếng Việt + emoji', () {
    expect(cleanName('Bass\u0000 2'), 'Bass 2');
    expect(cleanName(' \tTrống\n🥁\r\u001F\u007F '), 'Trống🥁');
    expect(cleanName('\u0000\u0001\u001B'), '');
    expect(cleanName('Bài hát mới 🥁'), 'Bài hát mới 🥁');
  });

  test('đọc project.json có tên chứa ký tự điều khiển → tên sạch (project rỗng → "Project")', () {
    final codec = ProjectCodec();
    final json = jsonDecode(codec.encode(newProject('x'))) as Map<String, dynamic>;
    json['name'] = '\u0000\u0007';
    (json['tracks'] as List)[0]['name'] = 'Tr\u0000ống';
    (json['scenes'] as List)[1]['name'] = 'Điệp\nkhúc';
    final p = codec.decode(jsonEncode(json)).project;
    expect(p.name, 'Project');
    expect(p.trackAt(0)!.name, 'Trống');
    expect(p.scenes[1].name, 'Điệpkhúc');
    expect(identical(cleanNames(p), p), isTrue, reason: 'đã sạch thì không tạo object mới');
  });

  test('repository: tạo / đổi tên với ký tự điều khiển → thư mục + tên sạch; toàn ký tự bị lọc → giữ tên cũ', () async {
    final tmp = Directory.systemTemp.createTempSync('names_');
    addTearDown(() => tmp.deleteSync(recursive: true));
    final repo = ProjectRepository(projectsDir: Directory('${tmp.path}/Projects'));
    final dir = await repo.create(newProject('Jam\u0000 1'));
    expect(dir, '${tmp.path}/Projects/Jam 1.loopproj');
    expect((await repo.load(dir)).project.name, 'Jam 1');
    expect(await repo.rename(dir, '\u0000\t'), dir);
    expect((await repo.load(dir)).project.name, 'Jam 1');
    final moved = await repo.rename(dir, 'Đêm\u0000 nhạc');
    expect(moved, '${tmp.path}/Projects/Đêm nhạc.loopproj');
  });

  testWidgets('ô nhập tên: chặn ký tự điều khiển khi gõ/dán; chỉ toàn ký tự bị lọc → null (giữ tên cũ)', (
    tester,
  ) async {
    String? result = 'chưa';
    await tester.pumpWidget(
      MaterialApp(
        home: Builder(
          builder: (context) => TextButton(
            onPressed: () async => result = await askName(context, title: 'Tên', initial: 'Cũ'),
            child: const Text('mở'),
          ),
        ),
      ),
    );
    Future<String?> submit(String text) async {
      await tester.tap(find.text('mở'));
      await tester.pumpAndSettle();
      await tester.enterText(find.byKey(const Key('nameDialog.field')), text);
      await tester.pump();
      final shown = tester.widget<TextField>(find.byKey(const Key('nameDialog.field'))).controller!.text;
      expect(shown, isNot(contains('\u0000')));
      await tester.tap(find.byKey(const Key('nameDialog.ok')));
      await tester.pumpAndSettle();
      return result;
    }

    expect(await submit('Lead\u0000 🎸\t'), 'Lead 🎸');
    expect(await submit('\u0000\u0001'), isNull);
  });

  testWidgets('đổi tên track / scene / clip: engine không nhận NUL; tên toàn ký tự điều khiển → giữ tên cũ', (
    tester,
  ) async {
    final h = SessionHarness(tester);
    await h.pump();
    final ctl = h.c.read(projectControllerProvider.notifier);
    final before = h.fake.calls.length;

    ctl.renameTrack(0, 'Trống\u0000 chính');
    expect(h.session.project.trackAt(0)!.name, 'Trống chính');
    expect(h.fake.calls.sublist(before).single.request['name'], 'Trống chính');
    ctl.renameTrack(0, '\u0000\n');
    expect(h.session.project.trackAt(0)!.name, 'Trống chính');
    expect(h.fake.calls.length, before + 1, reason: 'tên rỗng sau khi lọc → không gọi engine');

    ctl.renameScene(0, 'Verse\u0007');
    expect(h.session.project.scenes[0].name, 'Verse');
    ctl.renameScene(0, '\u0000');
    expect(h.session.project.scenes[0].name, 'Verse');

    final clipName = h.session.project.trackAt(0)!.clipAt(0)!.name;
    ctl.renameClip(0, 0, '\u0000\u0000');
    expect(h.session.project.trackAt(0)!.clipAt(0)!.name, clipName);
    ctl.renameClip(0, 0, 'Beat\u0000 A2');
    expect(h.session.project.trackAt(0)!.clipAt(0)!.name, 'Beat A2');

    // Engine (Fake giống engine thật) từ chối chuỗi chứa NUL nếu có chỗ nào lọt.
    expect(h.fake.call({'op': 'track.configure', 'track': 1, 'kind': 'audio', 'name': 'a\u0000b'})['ok'], isFalse);
    await h.unmount();
  });
}
