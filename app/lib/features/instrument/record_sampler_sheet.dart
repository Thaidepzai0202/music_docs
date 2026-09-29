import 'dart:async';
import 'dart:io';
import 'dart:math' as math;

import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app/theme.dart';
import '../../engine/engine_providers.dart';
import '../../engine/engine_state_ticker.dart';
import '../../engine/job_tracker.dart';
import '../../model/ids.dart';
import '../../services/capture_service.dart';
import '../../services/service_providers.dart';
import '../../model/project.dart';
import '../../ui_kit/keyboard_view.dart';
import '../../ui_kit/meter.dart';
import '../session/project_controller.dart';
import '../../l10n/l10n.dart';

enum RecordStep { ready, recording, analyzing, review, creating }

/// Mở sheet thu → sampler cho [track]. Trả true nếu đã tạo nhạc cụ.
Future<bool> showRecordSamplerSheet(BuildContext context, int track) async =>
    await showModalBottomSheet<bool>(
      context: context,
      isScrollControlled: true,
      enableDrag: false,
      builder: (_) => RecordSamplerSheet(track: track),
    ) ??
    false;

/// Sheet Record-to-Sampler (P3-03, 07 §4.2): thu (tự dừng sau 4 giây) → phân tích (trim + nốt gốc) →
/// chỉnh trim, xác nhận nốt ("A3 +12 cent"), chọn Natural/Classic → tạo nhạc cụ (có tiến độ).
/// Huỷ ở bất kỳ bước nào (kể cả chạm ra ngoài): dừng thu, huỷ job, xoá `instruments/<id>/` — không rò file.
class RecordSamplerSheet extends ConsumerStatefulWidget {
  const RecordSamplerSheet({super.key, required this.track, this.maxSeconds = 4});

  final int track;
  final double maxSeconds;

  static const lowConfidence = 0.6;

  @override
  ConsumerState<RecordSamplerSheet> createState() => _RecordSamplerSheetState();
}

class _RecordSamplerSheetState extends ConsumerState<RecordSamplerSheet> {
  late final CaptureService _capture = ref.read(captureServiceProvider);
  late final JobTracker _jobs = ref.read(jobTrackerProvider);
  late final EngineStateTicker _ticker = ref.read(engineStateTickerProvider);
  late final String _projectDir = ref.read(projectControllerProvider)!.dir;
  final String _id = newId('i');
  final _elapsed = Stopwatch();
  StreamSubscription<RecordingFinished>? _events;
  StreamSubscription<JobProgress>? _progressSub;

  RecordStep _step = RecordStep.ready;
  String? _error;
  int? _jobId;
  double _progress = 0;
  bool _finished = false; // tạo xong → không dọn file
  bool _cancelled = false; // đã huỷ → bỏ qua mọi kết quả job về sau (job có thể xong trong lúc sheet đang đóng)

  // Kết quả phân tích.
  List<double> _peaks = const [];
  int _totalSamples = 1;
  int _trimStart = 0;
  int _trimEnd = 1;
  int _rootNote = 60;
  int _cents = 0;
  double _confidence = 0;
  bool _notePicked = false;
  InstrumentMode _mode = InstrumentMode.natural;

  String get _dir => '$_projectDir/instruments/$_id';
  String get _sourcePath => '$_dir/source.caf';
  String get _sourceRel => 'instruments/$_id/source.caf';
  bool get _mustPickNote => _confidence < RecordSamplerSheet.lowConfidence;

  double get _sr {
    final sr = _ticker.state.sampleRate;
    return sr > 0 ? sr : 48000;
  }

  @override
  void initState() {
    super.initState();
    _events = _capture.finished.listen((_) {
      if (_step == RecordStep.recording) _captured(); // tự dừng ở maxSeconds
    });
  }

  @override
  void dispose() {
    _events?.cancel();
    _progressSub?.cancel();
    if (!_finished && !_cancelled) {
      _cancelled = true;
      _cleanup(); // đóng sheet giữa chừng bằng cách chạm ra ngoài
    }
    super.dispose();
  }

  // ───────── Luồng ─────────

  Future<void> _start() async {
    // Cần mic + audio chạy: chưa hỏi quyền thì hỏi lần đầu (07 §4.0).
    final ok = await ref.read(engineAudioProvider).ensureMic();
    if (!mounted) return;
    if (!ok) {
      setState(() => _error = S.samplerCanQuyenMicroDeThu);
      return;
    }
    try {
      Directory(_dir).createSync(recursive: true);
      _capture.start(_sourcePath, widget.maxSeconds);
    } on EngineCallException catch (e) {
      setState(() => _error = S.errorText(e.code));
      return;
    }
    _elapsed
      ..reset()
      ..start();
    setState(() {
      _error = null;
      _step = RecordStep.recording;
    });
  }

  void _stop() {
    if (_step == RecordStep.recording) _captured();
  }

  /// Thu xong (bấm dừng hoặc tự dừng) → capture.stop (idempotent) → capture.analyze.
  Future<void> _captured() async {
    _elapsed.stop();
    final ({String? file, double seconds}) res;
    try {
      res = _capture.stop();
      _jobId = _capture.analyze(res.file ?? _sourcePath);
    } on EngineCallException catch (e) {
      setState(() {
        _error = S.errorText(e.code);
        _step = RecordStep.ready;
      });
      return;
    }
    _totalSamples = math.max(1, (res.seconds * _sr).round());
    setState(() => _step = RecordStep.analyzing);
    try {
      final a = await _jobs.awaitJob(_jobId!);
      if (!mounted || _cancelled) return;
      if (a['silent'] == true) {
        // Mẫu im lặng: engine sẽ từ chối tạo nhạc cụ (INVALID_ARG) → thu lại ngay, không qua bước xem.
        _deleteFiles();
        setState(() {
          _error = S.samplerKhongNgheThayTieng;
          _step = RecordStep.ready;
          _jobId = null;
        });
        return;
      }
      final peaks = [for (final v in (a['peaks'] as List? ?? const [])) (v as num).toDouble()];
      final root = (a['rootNote'] as num? ?? -1).toInt(); // −1 = không dò được cao độ (05 §3)
      setState(() {
        _peaks = peaks;
        _trimStart = (a['trimStartSample'] as num? ?? 0).toInt().clamp(0, _totalSamples);
        _trimEnd = (a['trimEndSample'] as num? ?? _totalSamples).toInt().clamp(_trimStart + 1, _totalSamples);
        _rootNote = root >= 0 ? root : 60; // không dò được → bắt đầu chọn từ C4
        _cents = root >= 0 ? (a['cents'] as num? ?? 0).toInt() : 0;
        _confidence = root >= 0 ? (a['confidence'] as num? ?? 0).toDouble() : 0; // 0 → bắt buộc chọn nốt
        _step = RecordStep.review;
        _jobId = null;
      });
    } on EngineJobException catch (e) {
      if (mounted) {
        setState(() {
          _error = S.samplerPhanTichLoi(S.errorText(e.code));
          _step = RecordStep.ready;
        });
      }
    }
  }

  Future<void> _create() async {
    final sendRoot = _notePicked || _mustPickNote;
    try {
      _jobId = _capture.createInstrument(
        instrumentId: _id,
        file: _sourcePath,
        trimStartSample: _trimStart,
        trimEndSample: _trimEnd,
        rootNote: sendRoot ? _rootNote : null,
        mode: _mode.name,
      );
    } on EngineCallException catch (e) {
      setState(() => _error = S.samplerTaoNhacCuLoi(S.errorText(e.code)));
      return;
    }
    final id = _jobId!;
    _progressSub = _jobs.progress.where((p) => p.jobId == id).listen((p) => setState(() => _progress = p.progress));
    setState(() {
      _progress = 0;
      _step = RecordStep.creating;
    });
    try {
      final r = await _jobs.awaitJob(id);
      if (_cancelled) return; // người dùng đã huỷ: không gắn nhạc cụ
      final n = ref.read(projectControllerProvider)?.project.userInstruments.length ?? 0;
      final instrument = UserInstrument(
        id: _id,
        name: S.samplerTiengThu(n + 1),
        source: _sourceRel,
        rootNote: (r['rootNote'] as num? ?? _rootNote).toInt(),
        cents: (r['cents'] as num? ?? _cents).toDouble(),
        confidence: (r['confidence'] as num? ?? _confidence).toDouble(),
        mode: _mode,
      );
      ref.read(projectControllerProvider.notifier).attachUserInstrument(widget.track, instrument);
      _finished = true;
      if (mounted) Navigator.pop(context, true);
    } on EngineJobException catch (e) {
      if (!mounted || _cancelled) return;
      setState(() {
        _jobId = null;
        _step = RecordStep.review;
        if (e.code == 'PITCH_NOT_DETECTED') {
          _confidence = 0; // bắt buộc chọn nốt
          _error = S.samplerKhongDoDuocCaoDo;
        } else if (e.code != 'JOB_CANCELLED') {
          _error = S.samplerTaoNhacCuLoi(S.errorText(e.code));
        }
      });
    } finally {
      unawaited(_progressSub?.cancel());
    }
  }

  void _retake() {
    _deleteFiles();
    setState(() {
      _step = RecordStep.ready;
      _notePicked = false;
      _error = null;
    });
  }

  /// Huỷ NGAY (không đợi sheet đóng xong): dừng thu, huỷ job, xoá file.
  void _cancel() {
    if (_cancelled) return;
    _cancelled = true;
    _cleanup();
    Navigator.pop(context, false);
  }

  /// Dừng thu / huỷ job đang chạy, rồi xoá thư mục nhạc cụ.
  void _cleanup() {
    if (_step == RecordStep.recording) _capture.abort();
    final job = _jobId;
    if (job != null) _jobs.cancel(job);
    _deleteFiles();
  }

  void _deleteFiles() {
    try {
      final d = Directory(_dir);
      if (d.existsSync()) d.deleteSync(recursive: true);
    } on FileSystemException {
      // đã xoá hoặc không có quyền — không chặn UI
    }
  }

  void _pickNote(int delta) => setState(() {
    _rootNote = (_rootNote + delta).clamp(24, 96);
    _cents = 0;
    _notePicked = true;
    _error = null;
  });

  // ───────── UI ─────────

  @override
  Widget build(BuildContext context) {
    final small = Theme.of(context).textTheme.bodySmall;
    return SafeArea(
      child: SizedBox(
        height: 490,
        child: Padding(
          padding: const EdgeInsets.fromLTRB(24, 12, 24, 16),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              Row(
                children: [
                  Text(S.samplerThuAmNhacCu, style: TextStyle(fontSize: 18, fontWeight: FontWeight.w600)),
                  const Spacer(),
                  TextButton(key: const Key('rec.cancel'), onPressed: _cancel, child: Text(S.exportHuy)),
                ],
              ),
              if (_error case final e?)
                Padding(
                  padding: const EdgeInsets.only(bottom: 6),
                  child: Text(
                    e,
                    key: const Key('rec.error'),
                    maxLines: 2,
                    overflow: TextOverflow.ellipsis,
                    style: const TextStyle(color: AppColors.record),
                  ),
                ),
              Expanded(child: _body(small)),
            ],
          ),
        ),
      ),
    );
  }

  Widget _body(TextStyle? small) => switch (_step) {
    RecordStep.ready || RecordStep.recording => _recordView(small),
    RecordStep.analyzing => Column(
      mainAxisAlignment: MainAxisAlignment.center,
      children: [LinearProgressIndicator(), SizedBox(height: 12), Text(S.samplerDangCatKhoangLangVa)],
    ),
    RecordStep.review => _reviewView(small),
    RecordStep.creating => Column(
      mainAxisAlignment: MainAxisAlignment.center,
      children: [
        LinearProgressIndicator(key: const Key('rec.progress'), value: _progress),
        const SizedBox(height: 12),
        Text(S.samplerDangTaoNhacCu13((_progress * 100).round())),
      ],
    ),
  };

  Widget _recordView(TextStyle? small) {
    final recording = _step == RecordStep.recording;
    return Column(
      children: [
        const SizedBox(height: 8),
        GestureDetector(
          key: Key(recording ? 'rec.stop' : 'rec.start'),
          onTap: recording ? _stop : _start,
          child: Container(
            width: 120,
            height: 120,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              color: recording ? AppColors.surface : AppColors.record,
              border: Border.all(color: AppColors.record, width: 4),
            ),
            alignment: Alignment.center,
            child: Icon(recording ? Icons.stop : Icons.fiber_manual_record, size: 56, color: Colors.white),
          ),
        ),
        const SizedBox(height: 12),
        SizedBox(
          height: 24,
          width: 260,
          child: RepaintBoundary(
            child: CustomPaint(
              painter: _ElapsedPainter(_ticker, _elapsed, widget.maxSeconds, recording, painterFont(context)),
            ),
          ),
        ),
        const SizedBox(height: 8),
        SizedBox(
          height: 14,
          width: 360,
          child: LevelMeter(
            key: const Key('rec.meter'),
            repaint: _ticker,
            channels: 1,
            axis: Axis.horizontal,
            level: (_) => _ticker.state.inputPeak,
          ),
        ),
        const SizedBox(height: 8),
        Text(
          recording
              ? S.samplerHatHoacThoiMotNot(widget.maxSeconds.toStringAsFixed(0))
              : S.samplerChamDeThuMotNot(widget.maxSeconds.toStringAsFixed(0)),
          style: small,
        ),
      ],
    );
  }

  Widget _reviewView(TextStyle? small) {
    final noteLabel = '${noteName(_rootNote)} ${_cents >= 0 ? '+' : ''}$_cents cent';
    final canCreate = !_mustPickNote || _notePicked;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        SizedBox(
          height: 130,
          child: LayoutBuilder(
            builder: (context, c) {
              final w = c.maxWidth;
              double xOf(int s) => s / _totalSamples * w;
              int sOf(double x) => (x / w * _totalSamples).round().clamp(0, _totalSamples);
              return Stack(
                children: [
                  Positioned.fill(
                    child: RepaintBoundary(
                      child: CustomPaint(painter: _PeaksPainter(_peaks, xOf(_trimStart), xOf(_trimEnd))),
                    ),
                  ),
                  _trimHandle(const Key('rec.trimStart'), xOf(_trimStart), (dx) {
                    setState(() => _trimStart = math.min(sOf(xOf(_trimStart) + dx), _trimEnd - (_sr * 0.05).round()));
                  }),
                  _trimHandle(const Key('rec.trimEnd'), xOf(_trimEnd), (dx) {
                    setState(() => _trimEnd = math.max(sOf(xOf(_trimEnd) + dx), _trimStart + (_sr * 0.05).round()));
                  }),
                ],
              );
            },
          ),
        ),
        const SizedBox(height: 4),
        Text(S.samplerTrim((_trimStart / _sr).toStringAsFixed(2), (_trimEnd / _sr).toStringAsFixed(2)), style: small),
        const SizedBox(height: 12),
        Row(
          children: [
            Text(S.samplerNotGoc),
            IconButton(key: const Key('rec.noteDown'), icon: const Icon(Icons.remove), onPressed: () => _pickNote(-1)),
            Text(
              noteLabel,
              key: const Key('rec.note'),
              style: AppText.numeric.copyWith(fontSize: 18, fontWeight: FontWeight.w600),
            ),
            IconButton(key: const Key('rec.noteUp'), icon: const Icon(Icons.add), onPressed: () => _pickNote(1)),
            const SizedBox(width: 12),
            Flexible(
              child: Text(
                S.samplerDoTinCay((_confidence * 100).round()),
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: small,
              ),
            ),
          ],
        ),
        if (_mustPickNote && !_notePicked)
          Text(
            S.samplerKhongChacVeCaoDo,
            key: Key('rec.lowConfidence'),
            maxLines: 2,
            overflow: TextOverflow.ellipsis,
            style: TextStyle(color: AppColors.queued),
          ),
        const Spacer(),
        // Sheet bị giới hạn ngang (~640 pt): chế độ và nút hành động tách 2 hàng để không tràn.
        SegmentedButton<InstrumentMode>(
          key: const Key('rec.mode'),
          segments: [
            ButtonSegment(value: InstrumentMode.natural, label: Text(S.instrumentMode('natural'))),
            ButtonSegment(value: InstrumentMode.classic, label: Text(S.instrumentMode('classic'))),
          ],
          selected: {_mode},
          onSelectionChanged: (s) => setState(() => _mode = s.first),
        ),
        const SizedBox(height: 8),
        Row(
          children: [
            const Spacer(),
            TextButton(key: const Key('rec.retake'), onPressed: _retake, child: Text(S.samplerThuLai)),
            const SizedBox(width: 8),
            FilledButton(
              key: const Key('rec.create'),
              onPressed: canCreate ? _create : null,
              child: Text(S.samplerTaoNhacCu),
            ),
          ],
        ),
      ],
    );
  }

  Widget _trimHandle(Key key, double x, ValueChanged<double> onDrag) => Positioned(
    left: x - 14,
    top: 0,
    bottom: 0,
    width: 28,
    child: GestureDetector(
      key: key,
      behavior: HitTestBehavior.opaque,
      onHorizontalDragUpdate: (d) => onDrag(d.delta.dx),
      child: const Center(
        child: SizedBox(
          width: 4,
          height: double.infinity,
          child: ColoredBox(color: AppColors.queued),
        ),
      ),
    ),
  );
}

/// Thời gian đã thu, vẽ theo Ticker (không Timer, không setState).
class _ElapsedPainter extends CustomPainter {
  _ElapsedPainter(this.ticker, this.watch, this.max, this.active, TextStyle font)
    : _style = font.merge(AppText.numeric),
      super(repaint: ticker);
  final EngineStateTicker ticker;
  final Stopwatch watch;
  final double max;
  final bool active;
  final TextStyle _style;
  final _tp = TextPainter(textDirection: TextDirection.ltr);
  int? _key;
  static final _bar = Paint()..color = AppColors.record;
  static final _slot = Paint()..color = AppColors.border;

  @override
  void paint(Canvas canvas, Size size) {
    final secs = active ? math.min(watch.elapsedMilliseconds / 1000, max) : 0.0;
    final k = (secs * 10).floor();
    if (k != _key) {
      _key = k;
      _tp
        ..text = TextSpan(text: '${secs.toStringAsFixed(1)} / ${max.toStringAsFixed(0)} s', style: _style)
        ..layout();
    }
    canvas.drawRect(Rect.fromLTWH(0, size.height - 4, size.width, 4), _slot);
    canvas.drawRect(Rect.fromLTWH(0, size.height - 4, size.width * secs / max, 4), _bar);
    _tp.paint(canvas, Offset((size.width - _tp.width) / 2, 0));
  }

  @override
  bool shouldRepaint(_ElapsedPainter old) => old.active != active || old.watch != watch;
}

/// Waveform từ 512 cặp peaks của capture.analyze; ngoài vùng trim tô tối.
class _PeaksPainter extends CustomPainter {
  _PeaksPainter(this.peaks, this.x0, this.x1);
  final List<double> peaks;
  final double x0;
  final double x1;
  static final _wave = Paint()..color = AppColors.textSecondary;
  static final _bg = Paint()..color = AppColors.background;
  static final _shade = Paint()..color = const Color(0x99000000);

  @override
  void paint(Canvas canvas, Size size) {
    canvas.drawRect(Offset.zero & size, _bg);
    final n = peaks.length ~/ 2;
    if (n > 0) {
      final mid = size.height / 2;
      final w = size.width / n;
      for (var i = 0; i < n; i++) {
        canvas.drawRect(
          Rect.fromLTRB(i * w, mid - peaks[i * 2 + 1] * mid, (i + 1) * w, mid - peaks[i * 2] * mid),
          _wave,
        );
      }
    }
    canvas.drawRect(Rect.fromLTRB(0, 0, x0, size.height), _shade);
    canvas.drawRect(Rect.fromLTRB(x1, 0, size.width, size.height), _shade);
  }

  @override
  bool shouldRepaint(_PeaksPainter old) => old.peaks != peaks || old.x0 != x0 || old.x1 != x1;
}
