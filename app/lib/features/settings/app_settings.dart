import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'package:engine_ffi/engine_ffi.dart';

import '../../engine/engine_providers.dart';
import '../../model/project.dart';
import '../../services/service_providers.dart';

/// Quantize nốt khi thu MIDI (P2-18). Giá trị là độ dài lưới tính bằng beat.
enum RecordQuantize {
  off(0),
  sixteenth(0.25),
  eighth(0.5);

  const RecordQuantize(this.gridBeats);
  final double gridBeats;
}

/// Buffer audio cho phép chọn trong Settings (P4-13): 128 (~2.7 ms @48k) hoặc 256 (tiết kiệm CPU).
const bufferSizeChoices = [128, 256];

/// Cài đặt của app (`<Documents>/settings.json`, P4-13). Khác project: áp dụng cho mọi project.
@immutable
final class AppSettings {
  const AppSettings({
    this.recordBars = 4,
    this.midiRecordQuantize = RecordQuantize.off,
    this.haptics = true,
    this.onboardingDone = false,
    this.bufferSize = 128,
    this.defaultMonitor = MonitorMode.off,
    this.latencyOffsetSamples = 0,
    this.latencyMeasuredSamples,
  });

  /// Số bar khi chạm ô trống trên track đang arm. 0 = Tự do: chạm lần hai để chốt (`CLIP_RECORD i0 = 0` rồi
  /// `RECORD_STOP`, 07 §3.1b). Thiết lập chung của app, không lưu trong project (06 §2).
  final int recordBars;
  final RecordQuantize midiRecordQuantize;
  final bool haptics;

  /// Đã xem onboarding lần đầu (P4-14).
  final bool onboardingDone;

  /// Buffer audio (frame) — áp dụng lúc mở app và khi đổi trong Settings.
  final int bufferSize;

  /// Monitor của track audio mới tạo (06 §2 `track.monitor`).
  final MonitorMode defaultMonitor;

  /// Bù độ trễ chỉnh tay (`latency.setOffset`, sample) — gửi lại mỗi lần mở app.
  final int latencyOffsetSamples;

  /// Round-trip đo gần nhất (`latency.calibrate` measuredSamples), chỉ để hiển thị. null = chưa đo.
  final int? latencyMeasuredSamples;

  static const _monitorJson = {MonitorMode.off: 'off', MonitorMode.auto: 'auto', MonitorMode.always: 'on'};

  factory AppSettings.fromJson(Map<String, dynamic> j) => AppSettings(
    recordBars: (j['recordBars'] as num?)?.toInt().clamp(0, 16) ?? 4,
    midiRecordQuantize: RecordQuantize.values.asNameMap()[j['midiRecordQuantize']] ?? RecordQuantize.off,
    haptics: j['haptics'] as bool? ?? true,
    onboardingDone: j['onboardingDone'] as bool? ?? false,
    bufferSize: bufferSizeChoices.contains(j['bufferSize']) ? j['bufferSize'] as int : 128,
    defaultMonitor: {for (final e in _monitorJson.entries) e.value: e.key}[j['defaultMonitor']] ?? MonitorMode.off,
    latencyOffsetSamples: (j['latencyOffsetSamples'] as num?)?.toInt() ?? 0,
    latencyMeasuredSamples: (j['latencyMeasuredSamples'] as num?)?.toInt(),
  );

  Map<String, dynamic> toJson() => {
    'version': 1,
    'recordBars': recordBars,
    'midiRecordQuantize': midiRecordQuantize.name,
    'haptics': haptics,
    'onboardingDone': onboardingDone,
    'bufferSize': bufferSize,
    'defaultMonitor': _monitorJson[defaultMonitor],
    'latencyOffsetSamples': latencyOffsetSamples,
    if (latencyMeasuredSamples != null) 'latencyMeasuredSamples': latencyMeasuredSamples,
  };

  @override
  bool operator ==(Object other) =>
      other is AppSettings &&
      other.recordBars == recordBars &&
      other.midiRecordQuantize == midiRecordQuantize &&
      other.haptics == haptics &&
      other.onboardingDone == onboardingDone &&
      other.bufferSize == bufferSize &&
      other.defaultMonitor == defaultMonitor &&
      other.latencyOffsetSamples == latencyOffsetSamples &&
      other.latencyMeasuredSamples == latencyMeasuredSamples;

  @override
  int get hashCode => Object.hash(
    recordBars,
    midiRecordQuantize,
    haptics,
    onboardingDone,
    bufferSize,
    defaultMonitor,
    latencyOffsetSamples,
    latencyMeasuredSamples,
  );

  AppSettings copyWith({
    int? recordBars,
    RecordQuantize? midiRecordQuantize,
    bool? haptics,
    bool? onboardingDone,
    int? bufferSize,
    MonitorMode? defaultMonitor,
    int? latencyOffsetSamples,
    int? latencyMeasuredSamples,
  }) => AppSettings(
    recordBars: recordBars ?? this.recordBars,
    midiRecordQuantize: midiRecordQuantize ?? this.midiRecordQuantize,
    haptics: haptics ?? this.haptics,
    onboardingDone: onboardingDone ?? this.onboardingDone,
    bufferSize: bufferSize ?? this.bufferSize,
    defaultMonitor: defaultMonitor ?? this.defaultMonitor,
    latencyOffsetSamples: latencyOffsetSamples ?? this.latencyOffsetSamples,
    latencyMeasuredSamples: latencyMeasuredSamples ?? this.latencyMeasuredSamples,
  );
}

/// Đọc/ghi `<Documents>/settings.json` (bản tạm cho P4-13). Ghi qua file tạm rồi đổi tên cho an toàn.
class SettingsRepository {
  SettingsRepository(this.file);

  final File file;

  Future<AppSettings> load() async {
    try {
      return AppSettings.fromJson(jsonDecode(await file.readAsString()) as Map<String, dynamic>);
    } catch (_) {
      return const AppSettings(); // chưa có file hoặc file hỏng → mặc định
    }
  }

  Future<void> save(AppSettings s) async {
    final tmp = File('${file.path}.tmp');
    await tmp.writeAsString(jsonEncode(s.toJson()), flush: true);
    await tmp.rename(file.path);
  }
}

/// Settings đọc lúc mở app — override trong `main()`.
final initialSettingsProvider = Provider<AppSettings>((ref) => const AppSettings());

final settingsRepositoryProvider = Provider<SettingsRepository>(
  (ref) => SettingsRepository(File('${ref.watch(engineBootstrapProvider).dataDir}/settings.json')),
);

class SettingsController extends Notifier<AppSettings> {
  @override
  AppSettings build() => ref.read(initialSettingsProvider);

  void setRecordBars(int bars) => _set(state.copyWith(recordBars: bars.clamp(0, 16)));

  /// Quantize khi thu MIDI làm ở ENGINE (05 §3 `midi.setRecordQuantize`, P1-30).
  void setMidiRecordQuantize(RecordQuantize q) {
    try {
      ref.read(midiServiceProvider).setRecordQuantize(q.gridBeats);
    } on EngineCallException {
      // engine từ chối: vẫn lưu, lần mở app sau gửi lại
    }
    _set(state.copyWith(midiRecordQuantize: q));
  }

  void setHaptics(bool on) => _set(state.copyWith(haptics: on));

  /// Đổi buffer audio (engine làm ngay). Trả mã lỗi engine, null = ok.
  String? setBufferSize(int frames) {
    if (!bufferSizeChoices.contains(frames)) return 'INVALID_ARG';
    try {
      ref.read(audioDeviceServiceProvider).setBufferSize(frames);
    } on EngineCallException catch (e) {
      return e.code;
    }
    _set(state.copyWith(bufferSize: frames));
    return null;
  }

  void setDefaultMonitor(MonitorMode m) => _set(state.copyWith(defaultMonitor: m));

  /// Bù độ trễ chỉnh tay (P4-12). Luôn lưu (mở app lần sau gửi lại); trả mã lỗi engine nếu có.
  String? setLatencyOffset(int samples) {
    String? err;
    try {
      ref.read(latencyServiceProvider).setOffset(samples);
    } on EngineCallException catch (e) {
      err = e.code;
    }
    _set(state.copyWith(latencyOffsetSamples: samples));
    return err;
  }

  /// Kết quả `latency.calibrate`: engine đã áp dụng offset ngay nhưng không lưu → app lưu để gửi lại lúc mở app.
  void applyCalibration({required int measuredSamples, required int offsetSamples}) =>
      _set(state.copyWith(latencyMeasuredSamples: measuredSamples, latencyOffsetSamples: offsetSamples));

  void setOnboardingDone() => _set(state.copyWith(onboardingDone: true));

  void _set(AppSettings next) {
    if (next == state) return;
    state = next;
    unawaited(ref.read(settingsRepositoryProvider).save(next).catchError((Object _) {}));
  }
}

final settingsProvider = NotifierProvider<SettingsController, AppSettings>(SettingsController.new);
