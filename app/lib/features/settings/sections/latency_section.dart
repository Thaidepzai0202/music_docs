import 'dart:async';

import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../app/theme.dart';
import '../../../engine/engine_providers.dart';
import '../../../engine/job_tracker.dart';
import '../../../services/latency_service.dart';
import '../../../services/service_providers.dart';
import '../app_settings.dart';
import '../settings_screen.dart';
import '../../../l10n/l10n.dart';

/// Settings → Độ trễ (P4-12): đo round-trip bằng loopback (`latency.calibrate`, cần micro) và bù tay
/// (`latency.setOffset`, lưu trong settings.json, gửi lại mỗi lần mở app).
class LatencySection extends ConsumerStatefulWidget {
  const LatencySection({super.key});

  /// Dải bù tay (ms).
  static const maxOffsetMs = 20.0;

  @override
  ConsumerState<LatencySection> createState() => _LatencySectionState();
}

class _LatencySectionState extends ConsumerState<LatencySection> {
  late final LatencyService _latency = ref.read(latencyServiceProvider);
  late final JobTracker _jobs = ref.read(jobTrackerProvider);
  StreamSubscription<JobProgress>? _progressSub;
  int? _jobId;
  double _progress = 0;
  String? _error;

  /// Kết quả lần đo trong phiên này.
  CalibrationResult? _last;
  late double _offsetMs = _toMs(ref.read(settingsProvider).latencyOffsetSamples);

  double get _sampleRate => _latency.sampleRate;

  double _toMs(int samples) => samples / _sampleRate * 1000;
  int _toSamples(double ms) => (ms / 1000 * _sampleRate).round();

  @override
  void dispose() {
    _progressSub?.cancel();
    final id = _jobId;
    if (id != null) _jobs.cancel(id); // rời màn giữa chừng → huỷ đo
    super.dispose();
  }

  Future<void> _calibrate() async {
    setState(() => _error = null);
    // Đo cần audio chạy + micro: chưa hỏi quyền thì hỏi lần đầu (07 §4.0).
    final audio = ref.read(engineAudioProvider);
    final ok = await audio.ensureMic();
    if (!mounted) return;
    if (!ok) {
      setState(
        () => _error = audio.micPermission == MicPermission.granted
            ? S.latencyKhongBatDuocAudio
            : S.latencyCanQuyenMicroDeDo,
      );
      return;
    }
    final int id;
    try {
      id = _latency.calibrate();
    } on EngineCallException catch (e) {
      setState(() => _error = S.latencyDoLoi(S.errorText(e.code)));
      return;
    }
    _progressSub = _jobs.progress.where((p) => p.jobId == id).listen((p) => setState(() => _progress = p.progress));
    setState(() {
      _jobId = id;
      _progress = 0;
    });
    try {
      final r = LatencyService.parse(await _jobs.awaitJob(id));
      // Engine đã áp dụng offset = measured − reported ngay khi đo xong; app chỉ lưu (05 §3).
      if (r != null) {
        ref
            .read(settingsProvider.notifier)
            .applyCalibration(measuredSamples: r.measuredSamples, offsetSamples: r.offsetSamples);
      }
      if (mounted) {
        setState(() {
          _jobId = null;
          _last = r;
          if (r != null) _offsetMs = _toMs(r.offsetSamples);
        });
      }
    } on EngineJobException catch (e) {
      if (!mounted) return;
      setState(() {
        _jobId = null;
        if (e.code != 'JOB_CANCELLED') {
          final reason = e.code == 'AUDIO_DEVICE' && LatencyService.failReasons.contains(e.message);
          _error = S.latencyDoLoi(reason ? S.latencyFailReason(e.message) : S.errorText(e.code));
        }
      });
    } finally {
      unawaited(_progressSub?.cancel());
      _progressSub = null;
    }
  }

  void _commitOffset(double ms) {
    final err = ref.read(settingsProvider.notifier).setLatencyOffset(_toSamples(ms));
    if (err != null) {
      ScaffoldMessenger.maybeOf(
        context,
      )?.showSnackBar(SnackBar(content: Text(S.latencyDaLuuEngineChuaAp(S.errorText(err)))));
    }
  }

  String _describe(int samples) => S.latencySamplesMs(samples, _toMs(samples).toStringAsFixed(1));

  @override
  Widget build(BuildContext context) {
    final s = ref.watch(settingsProvider);
    final current = _latency.roundTripSamples;
    final canRecord = ref.watch(canRecordAudioProvider);
    final running = _jobId != null;
    final offsetSamples = _toSamples(_offsetMs);

    return SettingsPage(
      children: [
        SettingsGroup(
          title: S.latencyDoTreVongThuPhat,
          children: [
            SettingsRow(
              label: S.latencyEngineDangDung,
              child: Text(
                current > 0 ? _describe(current) : S.latencyChuaCo,
                key: const Key('latency.current'),
                style: AppText.numeric,
              ),
            ),
            if (s.latencyMeasuredSamples case final m?) ...[
              const Divider(height: 1),
              SettingsRow(
                label: S.latencyLanDoGanNhat,
                subtitle: switch (_last) {
                  final r? => S.latencyDeviceBaoBuSampleLech(
                    r.reportedSamples,
                    r.offsetSamples,
                    r.spreadSamples,
                    (r.confidence * 100).round(),
                  ),
                  null => null,
                },
                child: Text(_describe(m), key: const Key('latency.measured'), style: AppText.numeric),
              ),
            ],
          ],
        ),
        SettingsGroup(
          title: S.latencyDoTuDong,
          note: S.latencyAppPhatTiengClickRa,
          children: [
            Padding(
              padding: const EdgeInsets.symmetric(vertical: 12),
              child: Row(
                children: [
                  FilledButton.icon(
                    key: const Key('latency.calibrate'),
                    onPressed: running || !canRecord ? null : _calibrate,
                    icon: const Icon(Icons.hearing, size: 18),
                    label: Text(S.latencyDoDoTre),
                  ),
                  const SizedBox(width: 16),
                  if (running) ...[
                    Expanded(
                      child: LinearProgressIndicator(key: const Key('latency.progress'), value: _progress),
                    ),
                    TextButton(
                      key: const Key('latency.cancel'),
                      onPressed: () => _jobs.cancel(_jobId!),
                      child: Text(S.exportHuy),
                    ),
                  ] else if (!canRecord)
                    Flexible(
                      child: Text(S.latencyCanQuyenMicroDeDo, style: TextStyle(color: AppColors.queued)),
                    ),
                ],
              ),
            ),
            if (_error != null)
              Padding(
                padding: const EdgeInsets.only(bottom: 12),
                child: Text(
                  _error!,
                  key: const Key('latency.error'),
                  style: const TextStyle(color: AppColors.record),
                ),
              ),
          ],
        ),
        SettingsGroup(
          title: S.latencyChinhTay,
          note: S.latencyBanThuNgheBiTre,
          children: [
            SettingsRow(
              label: S.latencyBuThem,
              subtitle: S.latencyOffsetValue(
                '${_offsetMs >= 0 ? '+' : ''}${_offsetMs.toStringAsFixed(1)}',
                offsetSamples,
              ),
              below: true,
              child: Row(
                children: [
                  Expanded(
                    child: Slider(
                      key: const Key('latency.offset'),
                      min: -LatencySection.maxOffsetMs,
                      max: LatencySection.maxOffsetMs,
                      divisions: (LatencySection.maxOffsetMs * 4).round(), // bước 0.5 ms
                      value: _offsetMs.clamp(-LatencySection.maxOffsetMs, LatencySection.maxOffsetMs),
                      onChanged: (v) => setState(() => _offsetMs = v),
                      onChangeEnd: _commitOffset,
                    ),
                  ),
                  TextButton(
                    key: const Key('latency.offset.reset'),
                    onPressed: _offsetMs == 0
                        ? null
                        : () {
                            setState(() => _offsetMs = 0);
                            _commitOffset(0);
                          },
                    child: Text(S.latencyVe0),
                  ),
                ],
              ),
            ),
          ],
        ),
      ],
    );
  }
}
