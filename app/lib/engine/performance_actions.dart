import 'dart:developer';

import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../features/settings/app_settings.dart';
import 'engine_providers.dart';

/// Đường nóng (07 §5): launch / record / stop / scene gọi `le_send` NGAY trong pointer-down,
/// không đợi rebuild. UI cập nhật theo state đọc ở frame sau (07 §6.6).
///
/// Mỗi lệnh bọc `Timeline.timeSync` → đo pointer-down → le_send trong DevTools / Instruments (P2-07).
class PerformanceActions {
  PerformanceActions(this._engine, {this.haptics = true});

  final EngineApi _engine;
  bool haptics;

  /// Số bar mặc định khi thu vào ô trống (Settings). 0 = Tự do (chạm lần hai để chốt).
  int defaultRecordBars = 4;

  /// Track nhận nốt (05 §2 SELECT_TRACK): nốt từ MIDI controller cũng vào track này.
  bool selectTrack(int track) => _engine.send(LeCommandType.LE_CMD_SELECT_TRACK, track: track);

  bool launchClip(int track, int slot) =>
      _send('launch', LeCommandType.LE_CMD_CLIP_LAUNCH, track: track, slot: slot, haptic: true);

  bool recordClip(int track, int slot) =>
      _send('record', LeCommandType.LE_CMD_CLIP_RECORD, track: track, slot: slot, i0: defaultRecordBars, haptic: true);

  bool stopTrack(int track) => _send('stopTrack', LeCommandType.LE_CMD_CLIP_STOP, track: track);

  /// Chốt bản thu tự do / huỷ thu đang chờ (chạm lần hai trên ô, 07 §3.1b).
  bool recordStop(int track) => _send('recordStop', LeCommandType.LE_CMD_RECORD_STOP, track: track, haptic: true);

  /// Nút LOOP / footswitch (05 §2 `LE_CMD_LOOP_BUTTON`): engine tự xoay vòng thu → chốt → overdub → phát.
  /// [slot] = ô đang chọn, −1 = để engine tự chọn.
  bool loopButton(int track, int slot) =>
      _send('loop', LeCommandType.LE_CMD_LOOP_BUTTON, track: track, slot: slot, haptic: true);

  /// Bật / tắt overdub clip đang phát của track (05 §2 `LE_CMD_OVERDUB_TOGGLE`; clip MIDI: ghi chồng nốt, 07 §4.1d).
  bool overdubToggle(int track) => _send('overdub', LeCommandType.LE_CMD_OVERDUB_TOGGLE, track: track, haptic: true);

  bool launchScene(int scene) => _send('scene', LeCommandType.LE_CMD_SCENE_LAUNCH, slot: scene, haptic: true);

  bool stopAll() => _send('stopAll', LeCommandType.LE_CMD_STOP_ALL);

  /// Nốt từ pad/bàn phím (07 §3.2). Không haptic: chơi nhanh nhiều nốt.
  bool noteOn(int track, int note, double velocity) =>
      _sendNote(LeCommandType.LE_CMD_NOTE_ON, track, note, velocity.clamp(0.0, 1.0));

  bool noteOff(int track, int note) => _sendNote(LeCommandType.LE_CMD_NOTE_OFF, track, note, 0);

  bool _sendNote(int type, int track, int note, double v) =>
      Timeline.timeSync('LoopCore.note', () => _engine.send(type, track: track, i0: note.clamp(0, 127), f0: v));

  bool transportPlay() => _send('play', LeCommandType.LE_CMD_TRANSPORT_PLAY);

  bool transportStop() => _send('stop', LeCommandType.LE_CMD_TRANSPORT_STOP);

  bool _send(String label, int type, {int track = -1, int slot = -1, int i0 = 0, bool haptic = false}) {
    final ok = Timeline.timeSync('LoopCore.$label', () => _engine.send(type, track: track, slot: slot, i0: i0));
    if (haptic && haptics) HapticFeedback.selectionClick(); // sau lệnh, không chặn đường nóng
    return ok;
  }
}

final performanceActionsProvider = Provider<PerformanceActions>((ref) {
  final a = PerformanceActions(ref.watch(engineProvider));
  void apply(AppSettings s) => a
    ..defaultRecordBars = s.recordBars
    ..haptics = s.haptics;
  apply(ref.read(settingsProvider));
  ref.listen(settingsProvider, (_, s) => apply(s));
  return a;
});
