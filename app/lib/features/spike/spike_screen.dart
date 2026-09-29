import 'dart:async';
import 'dart:convert';
import 'dart:math' as math;

import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app/router.dart';
import '../settings/app_settings.dart';
import '../../app/theme.dart';
import '../../engine/engine_audio.dart';
import '../../engine/engine_bootstrap.dart';
import '../../engine/engine_providers.dart';
import '../../engine/engine_state_ticker.dart';
import '../../engine/job_tracker.dart';
import 'live_panel.dart';
import 'spike_results.dart';
import '../../l10n/l10n.dart';

/// P0-05: màn spike duy nhất, không cần đẹp. Mọi control gửi lệnh TRƯỚC rồi mới cập nhật UI (07 §6.6).
///
/// Repaint/rebuild:
/// - Bảng live 60Hz: [LivePanel] (RepaintBoundary + CustomPainter(repaint: ticker)), không rebuild.
/// - Vùng control: [_SpikeControls] giữ state riêng → kéo slider chỉ rebuild + repaint vùng này.
/// - Header / kết quả / log: RepaintBoundary riêng, rebuild qua Listenable khi có sự kiện.
class SpikeScreen extends ConsumerStatefulWidget {
  const SpikeScreen({super.key});

  @override
  ConsumerState<SpikeScreen> createState() => _SpikeScreenState();
}

class _SpikeScreenState extends ConsumerState<SpikeScreen> {
  late final EngineBootstrap _boot = ref.read(engineBootstrapProvider);
  late final EngineApi _engine = ref.read(engineProvider);
  late final EngineAudio _audio = ref.read(engineAudioProvider);
  late final EngineStateTicker _ticker = ref.read(engineStateTickerProvider);

  final _info = ValueNotifier<Map<String, dynamic>>(const {});
  final _result = ValueNotifier<String>('');
  final _eventLog = ValueNotifier<List<String>>(const []);
  StreamSubscription<EngineEvent>? _eventSub;

  @override
  void initState() {
    super.initState();
    _audio.keepRunningInBackground = true; // spike: về Home vẫn phát (P0-10)
    _refreshInfo();
    _eventSub = _engine.events.listen(_onEvent);
  }

  @override
  void dispose() {
    _audio.keepRunningInBackground = false;
    _eventSub?.cancel();
    _info.dispose();
    _result.dispose();
    _eventLog.dispose();
    super.dispose();
  }

  void _refreshInfo() {
    final res = _engine.call({'op': 'engine.info'});
    _info.value = res['ok'] == true ? (res['result'] as Map).cast<String, dynamic>() : {S.spikeLoi: res['error']};
  }

  Future<void> _toggleAudio() async {
    if (_audio.isRunning) {
      _audio.stop();
      return;
    }
    // Màn đo P0 cần mic (loopback, passthrough) → hỏi quyền TRƯỚC khi bật audio (P0-06); từ chối → chỉ phát.
    final r = await _audio.ensureMic() ? const AudioStartResult(AudioStartStatus.started) : await _audio.start();
    if (!mounted) return;
    switch (r.status) {
      case AudioStartStatus.started:
        _refreshInfo();
      case AudioStartStatus.outputOnly:
        _refreshInfo(); // audio vẫn chạy (chỉ phát), đo loopback thì cần mic
        await _showMicDeniedDialog();
      case AudioStartStatus.engineError:
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(SnackBar(content: Text(S.spikeLeAudioStartLoi(leErrorName(r.errorCode)))));
    }
  }

  void _onEvent(EngineEvent e) {
    final line = switch (e) {
      RouteChanged(:final wiredOrInterface, :final bluetooth) => S.spikeRouteDoiTaiNgheDay(
        wiredOrInterface ? S.spikeCo : S.spikeKhong,
        bluetooth ? S.spikeCoLatencyCao : S.spikeKhong,
      ),
      AudioInterrupted(:final began) => began ? S.spikeAudioBiNgatSiriCuoc : S.spikeHetNgatAudio,
      XrunOccurred(:final totalXruns) => S.spikeXrunTong(totalXruns),
      EngineErrorEvent(:final errorCode) => S.spikeLoiEngine(leErrorName(errorCode)),
      MemoryWarning(:final megabytes) => S.spikeCanhBaoBoNhoMb(megabytes.toStringAsFixed(0)),
      RecordingFinished(:final frames) => S.spikeThuXong(frames, _seconds(frames)),
      _ => null,
    };
    if (line == null) return;
    final now = DateTime.now();
    final ts = '${_two(now.hour)}:${_two(now.minute)}:${_two(now.second)}';
    final prev = _eventLog.value;
    // Xrun liên tiếp: thay dòng cuối thay vì thêm (tránh log tràn).
    final replaceLast = e is XrunOccurred && prev.isNotEmpty && prev.last.contains('Xrun');
    final next = [...(replaceLast ? prev.sublist(0, prev.length - 1) : prev), '$ts  $line'];
    _eventLog.value = next.length > 30 ? next.sublist(next.length - 30) : next;
  }

  static String _two(int v) => v.toString().padLeft(2, '0');

  String _seconds(int frames) {
    final sr = _ticker.state.sampleRate > 0 ? _ticker.state.sampleRate : 48000;
    return (frames / sr).toStringAsFixed(2);
  }

  Future<void> _showMicDeniedDialog() => showDialog<void>(
    context: context,
    builder: (ctx) => AlertDialog(
      title: Text(S.spikeCanQuyenMicro),
      content: Text(S.spikeAppCanMicroDeThu),
      actions: [
        TextButton(onPressed: () => Navigator.pop(ctx), child: Text(S.spikeDeSau)),
        FilledButton(
          key: const Key('spike.openSettings'),
          onPressed: () {
            Navigator.pop(ctx);
            _audio.openAppSettings();
          },
          child: Text(S.spikeMoCaiDat),
        ),
      ],
    ),
  );

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: SafeArea(
        child: EngineTickerScope(
          ticker: _ticker,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              RepaintBoundary(child: _buildHeader()),
              const Divider(height: 1),
              Expanded(
                child: Row(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    Expanded(
                      flex: 5,
                      child: RepaintBoundary(
                        child: _SpikeControls(
                          engine: _engine,
                          jobs: ref.read(jobTrackerProvider),
                          saveDir: '${_boot.dataDir}/spike',
                          initialBufferSize: (_info.value['bufferSize'] as int?) ?? 128,
                          result: _result,
                          onBufferSizeChanged: _refreshInfo,
                        ),
                      ),
                    ),
                    const VerticalDivider(width: 1),
                    Expanded(
                      flex: 4,
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.stretch,
                        children: [
                          LivePanel(ticker: _ticker),
                          const Divider(height: 1),
                          Expanded(child: RepaintBoundary(child: _buildOutput())),
                        ],
                      ),
                    ),
                  ],
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildHeader() {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      child: ListenableBuilder(
        listenable: Listenable.merge([_audio, _info]),
        builder: (context, _) {
          final info = _info.value;
          return Row(
            children: [
              Text(S.spikeLoopcoreSpike, style: Theme.of(context).textTheme.titleMedium),
              const SizedBox(width: 16),
              Text(S.spikeApiversion(_engine.apiVersion), style: AppText.numeric),
              if (!_boot.ok) ...[
                const SizedBox(width: 16),
                Text(
                  S.spikeLeCreateLoi(leErrorName(_boot.createResult)),
                  style: const TextStyle(color: AppColors.record),
                ),
              ],
              const SizedBox(width: 16),
              Flexible(
                child: Text(
                  _boot.isFake
                      ? S.spikeFakeengineChuaCoLoopcoreXcframework
                      : S.spikeDeviceInKenh(info['device'] ?? '?', info['inputChannels'] ?? '?'),
                  overflow: TextOverflow.ellipsis,
                  style: TextStyle(color: _boot.isFake ? AppColors.queued : AppColors.textSecondary),
                ),
              ),
              const Spacer(),
              TextButton(
                key: const Key('spike.devProjects'),
                onPressed: () => Navigator.pushNamed(
                  context,
                  AppRoutes.projectsEntry(onboardingDone: ref.read(settingsProvider).onboardingDone),
                ),
                child: Text(S.spikeProjectsDev),
              ),
              const SizedBox(width: 8),
              FilledButton.icon(
                key: const Key('spike.audio'),
                style: FilledButton.styleFrom(backgroundColor: _audio.isRunning ? AppColors.record : AppColors.play),
                onPressed: _toggleAudio,
                icon: Icon(_audio.isRunning ? Icons.stop : Icons.play_arrow),
                label: Text(_audio.isRunning ? S.spikeStopAudio : S.spikeStartAudio),
              ),
            ],
          );
        },
      ),
    );
  }

  Widget _buildOutput() {
    final small = Theme.of(context).textTheme.bodySmall;
    return Padding(
      padding: const EdgeInsets.all(12),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Text(S.spikeKetQua, style: small),
          Expanded(
            flex: 3,
            child: SingleChildScrollView(
              child: ValueListenableBuilder<String>(
                valueListenable: _result,
                builder: (context, text, _) => SelectableText(
                  text.isEmpty ? '—' : text,
                  key: const Key('spike.result'),
                  style: const TextStyle(fontFamily: 'Menlo', fontSize: 12, color: AppColors.textPrimary),
                ),
              ),
            ),
          ),
          const Divider(),
          Text(S.spikeEventRouteNgatXrun, style: small),
          Expanded(
            flex: 2,
            child: ValueListenableBuilder<List<String>>(
              valueListenable: _eventLog,
              builder: (context, lines, _) => ListView.builder(
                key: const Key('spike.events'),
                reverse: true,
                itemCount: lines.length,
                itemBuilder: (context, i) => Text(lines[lines.length - 1 - i], style: small),
              ),
            ),
          ),
        ],
      ),
    );
  }
}

/// Các control spike. State cục bộ → setState chỉ rebuild vùng này.
class _SpikeControls extends StatefulWidget {
  const _SpikeControls({
    required this.engine,
    required this.jobs,
    required this.saveDir,
    required this.initialBufferSize,
    required this.result,
    required this.onBufferSizeChanged,
  });

  final EngineApi engine;
  final JobTracker jobs;
  final String saveDir;
  final int initialBufferSize;
  final ValueNotifier<String> result;
  final VoidCallback onBufferSizeChanged;

  @override
  State<_SpikeControls> createState() => _SpikeControlsState();
}

class _SpikeControlsState extends State<_SpikeControls> {
  static const minHz = 50.0;
  static const maxHz = 2000.0;
  static const recordMs = 4000;

  EngineApi get _engine => widget.engine;

  late int _bufferSize = widget.initialBufferSize;
  double _freqT = math.log(440 / minHz) / math.log(maxHz / minHz); // 440 Hz trên thang log
  double _gain = 0.3;
  bool _sineOn = false;
  int _voices = 0;
  bool _recording = false;
  bool _loopOn = false;
  bool _passthrough = false;
  bool _formant = true;
  String _sessionMode = 'default';
  bool _busyLatency = false;
  bool _busyStretch = false;
  bool _stretchFineBlock = false; // blockMs 200 / intervalMs 50: lệch cao độ ít hơn, chậm hơn
  bool _stretchCheaper = false;
  Timer? _recordTimer;

  static double _hzOf(double t) => (minHz * math.pow(maxHz / minHz, t)).toDouble();

  @override
  void dispose() {
    _recordTimer?.cancel();
    super.dispose();
  }

  // ───────── Hành động: gửi lệnh TRƯỚC, cập nhật UI SAU ─────────

  void _setBufferSize(int frames) {
    final res = _engine.call({'op': 'spike.setBufferSize', 'frames': frames});
    if (res['ok'] != true) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(S.spikeSpikeSetbuffersizeLoi(S.errorText('${(res['error'] as Map?)?['code']}')))),
      );
      return;
    }
    widget.onBufferSizeChanged();
    setState(() => _bufferSize = frames);
  }

  void _sendSine({required double hz, required double gain, required bool on}) =>
      _engine.send(LeCommandType.LE_CMD_SPIKE_SINE, f0: hz, f1: on ? gain : 0);

  void _onSineToggle(bool on) {
    _sendSine(hz: _hzOf(_freqT), gain: _gain, on: on);
    setState(() => _sineOn = on);
  }

  void _onFreqChanged(double t) {
    if (_sineOn) _sendSine(hz: _hzOf(t), gain: _gain, on: true);
    setState(() => _freqT = t);
  }

  void _onGainChanged(double g) {
    if (_sineOn) _sendSine(hz: _hzOf(_freqT), gain: g, on: true);
    setState(() => _gain = g);
  }

  void _onVoicesChanged(double v) {
    final n = v.round();
    if (n == _voices) return;
    _engine.send(LeCommandType.LE_CMD_SPIKE_LOAD_VOICES, i0: n);
    setState(() => _voices = n);
  }

  void _record() {
    _engine.send(LeCommandType.LE_CMD_SPIKE_RECORD, i0: recordMs);
    _recordTimer?.cancel();
    _recordTimer = Timer(const Duration(milliseconds: recordMs), () {
      if (mounted) setState(() => _recording = false);
    });
    setState(() => _recording = true);
  }

  void _onLoopToggle(bool on) {
    _engine.send(LeCommandType.LE_CMD_SPIKE_PLAY_RECORD, i0: on ? 1 : 0);
    setState(() => _loopOn = on);
  }

  Future<void> _onPassthroughToggle(bool on) async {
    if (on && !await _confirmHeadphones()) return;
    _engine.send(LeCommandType.LE_CMD_SPIKE_PASSTHROUGH, i0: on ? 1 : 0);
    if (mounted) setState(() => _passthrough = on);
  }

  /// P0-06: so sánh chất lượng thu giữa mode `default` và `measurement`.
  void _setSessionMode(String mode) {
    final res = _engine.call({'op': 'spike.setSessionMode', 'mode': mode});
    if (res['ok'] != true) {
      widget.result.value = S.spikeSpikeSetsessionmodeLoi(S.errorText('${(res['error'] as Map?)?['code']}'));
      return;
    }
    final r = (res['result'] as Map).cast<String, dynamic>();
    widget.result.value = 'spike.setSessionMode\n${const JsonEncoder.withIndent('  ').convert(r)}';
    setState(() => _sessionMode = (r['mode'] as String?) ?? mode);
  }

  /// Hiện category/mode/options của AVAudioSession (kiểm allowBluetoothHFP = false).
  void _showSessionInfo() {
    final res = _engine.call({'op': 'spike.sessionInfo'});
    widget.result.value = 'spike.sessionInfo\n${const JsonEncoder.withIndent('  ').convert(res)}';
  }

  Future<void> _runLatency() =>
      _runJob('spike.latencyLoopback', const {}, (b) => _busyLatency = b, summary: formatLatencyResult);

  Future<void> _runStretch() => _runJob(
    'spike.stretchBench',
    {
      'semitones': [for (var s = -18; s <= 18; s += 3) s],
      'formant': _formant,
      'saveDir': widget.saveDir,
      if (_stretchFineBlock) ...{'blockMs': 200, 'intervalMs': 50},
      if (_stretchCheaper) 'cheaper': true,
    },
    (b) => _busyStretch = b,
    summary: formatStretchResult,
  );

  Future<void> _runJob(
    String op,
    Map<String, dynamic> params,
    void Function(bool) setBusy, {
    required String Function(Map<String, dynamic>) summary,
  }) async {
    final int jobId;
    try {
      jobId = _engine.callJob(op, params);
    } on EngineCallException catch (e) {
      final hint = e.code == 'AUDIO_DEVICE' ? S.spikeNaudioChuaChayBamStart : '';
      widget.result.value = '$op → ${e.code}\n${e.message}$hint';
      return;
    }
    widget.result.value = S.spikeJobDangChay(op, jobId);
    setState(() => setBusy(true));
    final progress = widget.jobs.progress
        .where((p) => p.jobId == jobId)
        .listen((p) => widget.result.value = S.spikeJobDangChay2(op, jobId, (p.progress * 100).round()));
    try {
      final r = await widget.jobs.awaitJob(jobId);
      widget.result.value = '${summary(r)}\n\n$op (job $jobId)\n${const JsonEncoder.withIndent('  ').convert(r)}';
    } on EngineJobException catch (e) {
      widget.result.value = S.spikeJobThatBai(op, jobId, e.code, e.message);
    } finally {
      unawaited(progress.cancel()); // không cần đợi; broadcast stream huỷ ngay
    }
    if (mounted) setState(() => setBusy(false));
  }

  Future<bool> _confirmHeadphones() async =>
      await showDialog<bool>(
        context: context,
        builder: (ctx) => AlertDialog(
          title: Text(S.spikeCamTaiNgheTruoc),
          content: Text(S.spikePassthroughDuaMicRaLoa),
          actions: [
            TextButton(onPressed: () => Navigator.pop(ctx, false), child: Text(S.spikeHuy)),
            FilledButton(
              key: const Key('spike.passthrough.confirm'),
              onPressed: () => Navigator.pop(ctx, true),
              child: Text(S.spikeDaCamBat),
            ),
          ],
        ),
      ) ??
      false;

  // ───────── UI ─────────

  @override
  Widget build(BuildContext context) {
    final secondary = Theme.of(context).textTheme.bodySmall;
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        _section('Buffer'),
        SegmentedButton<int>(
          key: const Key('spike.buffer'),
          segments: const [
            ButtonSegment(value: 128, label: Text('128')),
            ButtonSegment(value: 256, label: Text('256')),
          ],
          selected: {_bufferSize},
          onSelectionChanged: (s) => _setBufferSize(s.first),
        ),
        _section('Sine'),
        SwitchListTile(
          key: const Key('spike.sine.on'),
          contentPadding: EdgeInsets.zero,
          title: Text(S.spikeSineHzGain(_hzOf(_freqT).round(), _gain.toStringAsFixed(2)), style: AppText.numeric),
          value: _sineOn,
          onChanged: _onSineToggle,
        ),
        Row(
          children: [
            SizedBox(width: 72, child: Text(S.spikeTanSo, style: secondary)),
            Expanded(
              child: Slider(key: const Key('spike.sine.freq'), value: _freqT, onChanged: _onFreqChanged),
            ),
          ],
        ),
        Row(
          children: [
            SizedBox(width: 72, child: Text(S.spikeGain, style: secondary)),
            Expanded(
              child: Slider(key: const Key('spike.sine.gain'), value: _gain, onChanged: _onGainChanged),
            ),
          ],
        ),
        _section(S.spikeTaiGiaLapVoice(_voices)),
        Slider(
          key: const Key('spike.voices'),
          value: _voices.toDouble(),
          max: 128,
          divisions: 128,
          label: '$_voices',
          onChanged: _onVoicesChanged,
        ),
        _section('Mic'),
        Wrap(
          spacing: 12,
          runSpacing: 8,
          crossAxisAlignment: WrapCrossAlignment.center,
          children: [
            FilledButton.icon(
              key: const Key('spike.record'),
              style: FilledButton.styleFrom(backgroundColor: AppColors.record),
              onPressed: _recording ? null : _record,
              icon: const Icon(Icons.fiber_manual_record),
              label: Text(_recording ? S.spikeDangThu4Giay : S.spikeThu4Giay),
            ),
            FilterChip(
              key: const Key('spike.loop'),
              label: Text(S.spikePhatLoop),
              selected: _loopOn,
              onSelected: _onLoopToggle,
            ),
            FilterChip(
              key: const Key('spike.passthrough'),
              label: Text(S.spikePassthroughCanTaiNghe),
              selected: _passthrough,
              onSelected: _onPassthroughToggle,
            ),
          ],
        ),
        const SizedBox(height: 12),
        Wrap(
          spacing: 12,
          runSpacing: 8,
          crossAxisAlignment: WrapCrossAlignment.center,
          children: [
            SegmentedButton<String>(
              key: const Key('spike.sessionMode'),
              segments: [
                ButtonSegment(value: 'default', label: Text(S.spikeModeDefault)),
                ButtonSegment(value: 'measurement', label: Text(S.spikeModeMeasurement)),
              ],
              selected: {_sessionMode},
              onSelectionChanged: (s) => _setSessionMode(s.first),
            ),
            OutlinedButton(
              key: const Key('spike.sessionInfo'),
              onPressed: _showSessionInfo,
              child: Text(S.spikeSessionInfo),
            ),
          ],
        ),
        _section(S.spikeDo),
        Wrap(
          spacing: 12,
          runSpacing: 8,
          crossAxisAlignment: WrapCrossAlignment.center,
          children: [
            OutlinedButton(
              key: const Key('spike.latency'),
              onPressed: _busyLatency ? null : _runLatency,
              child: Text(_busyLatency ? S.spikeDangDo : S.spikeDoLatency),
            ),
            OutlinedButton(
              key: const Key('spike.stretch'),
              onPressed: _busyStretch ? null : _runStretch,
              child: Text(_busyStretch ? S.spikeDangChay : S.spikeStretchBench),
            ),
            FilterChip(
              key: const Key('spike.formant'),
              label: Text(S.spikeFormant),
              selected: _formant,
              onSelected: (v) => setState(() => _formant = v),
            ),
            FilterChip(
              key: const Key('spike.stretch.fineBlock'),
              label: Text(S.spikeBlock20050Ms),
              selected: _stretchFineBlock,
              onSelected: (v) => setState(() => _stretchFineBlock = v),
            ),
            FilterChip(
              key: const Key('spike.stretch.cheaper'),
              label: Text(S.spikeCheaper),
              selected: _stretchCheaper,
              onSelected: (v) => setState(() => _stretchCheaper = v),
            ),
          ],
        ),
        const SizedBox(height: 6),
        Text(S.spikeDoLatencyBoTaiNghe, style: secondary),
      ],
    );
  }

  Widget _section(String title) => Padding(
    padding: const EdgeInsets.only(top: 16, bottom: 6),
    child: Text(title, style: Theme.of(context).textTheme.labelLarge),
  );
}
