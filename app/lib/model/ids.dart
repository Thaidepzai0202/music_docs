import 'package:uuid/uuid.dart';

import '../app/app_info.dart';
import '../app/theme.dart';
import 'migrations/project_migrator.dart';
import 'project.dart';

const _uuid = Uuid();

/// ID dạng UUID v4 có tiền tố (06 §1): `t_` track, `c_` clip, `i_` instrument, `p_` project.
String newId(String prefix) => '${prefix}_${_uuid.v4()}';

String colorHex(int trackIndex) {
  final c = AppColors.tracks[trackIndex % AppColors.tracks.length];
  return '#${(c.toARGB32() & 0xFFFFFF).toRadixString(16).padLeft(6, '0').toUpperCase()}';
}

String _trackName(int n) => 'Track $n';
String _sceneName(int n) => 'Scene $n';

/// Track audio trống ở cột [index]. [monitor] lấy từ Settings (monitor mặc định, P4-13). [name]: tên theo ngôn ngữ
/// lúc tạo (UI truyền `S.sessionTrackName`).
Track defaultTrack(int index, {MonitorMode monitor = MonitorMode.off, String Function(int n) name = _trackName}) =>
    Track(
      id: newId('t'),
      index: index,
      name: name(index + 1),
      color: colorHex(index),
      kind: TrackKind.audio,
      monitor: monitor,
    );

/// Project mới: 8 track audio trống, 8 scene, 120 BPM, quantize 1 bar. Tên track/scene theo ngôn ngữ lúc tạo
/// ([trackName] / [sceneName], UI truyền `S.sessionTrackName` / `S.sessionSceneName`).
Project newProject(
  String name, {
  DateTime? now,
  String appVersion = AppInfo.version,
  MonitorMode monitor = MonitorMode.off,
  String Function(int n) trackName = _trackName,
  String Function(int n) sceneName = _sceneName,
}) {
  final t = (now ?? DateTime.now()).toUtc();
  return Project(
    schemaVersion: kCurrentSchemaVersion,
    id: newId('p'),
    name: name,
    createdAt: t,
    modifiedAt: t,
    appVersion: appVersion,
    transport: const Transport(tempoMode: TempoMode.firstLoop), // 07 §3.1b: project mới mặc định pedal mode
    scenes: [for (var i = 0; i < 8; i++) Scene(index: i, name: sceneName(i + 1))],
    tracks: [for (var i = 0; i < 8; i++) defaultTrack(i, monitor: monitor, name: trackName)],
  );
}
