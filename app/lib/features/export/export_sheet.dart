import 'dart:async';

import 'package:clock/clock.dart';
import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app/theme.dart';
import '../../engine/engine_providers.dart';
import '../../engine/job_tracker.dart';
import '../../model/project.dart';
import '../session/project_controller.dart';
import '../session/session_layout.dart';
import '../../services/export_service.dart';
import '../../services/service_providers.dart';
import 'jam_recorder.dart';
import '../../l10n/l10n.dart';

/// Mở sheet Export (07 §4.4). [initialTab]: 0 = Ghi buổi jam, 1 = Export scene.
Future<void> showExportSheet(BuildContext context, {int initialTab = 0}) => showModalBottomSheet<void>(
  context: context,
  isScrollControlled: true,
  constraints: const BoxConstraints(maxWidth: 760),
  builder: (_) => ExportSheet(initialTab: initialTab),
);

/// Vùng của widget trên màn hình — iPad cần điểm neo cho popover share sheet.
Rect? shareOrigin(BuildContext context) {
  final box = context.findRenderObject();
  if (box is! RenderBox || !box.hasSize) return null;
  return box.localToGlobal(Offset.zero) & box.size;
}

/// Ghi jam bị rớt khung (droppedFrames > 0) → SnackBar "Mất X ms: bộ nhớ ghi không kịp" (không chặn).
void warnDroppedFrames(BuildContext context, WidgetRef ref, ExportResult r) {
  if (r.droppedFrames <= 0) return;
  final ms = r.droppedMs(ref.read(engineStateTickerProvider).state.sampleRate);
  ScaffoldMessenger.maybeOf(context)?.showSnackBar(SnackBar(content: Text(S.jamDroppedMs(ms))));
}

/// Chia sẻ [result]; lỗi share (hiếm) → SnackBar, không ném ra ngoài.
Future<void> shareExport(BuildContext context, WidgetRef ref, ExportResult result) async {
  final messenger = ScaffoldMessenger.maybeOf(context);
  try {
    await ref.read(shareServiceProvider).shareFiles(result.allFiles, origin: shareOrigin(context));
  } catch (e) {
    messenger?.showSnackBar(SnackBar(content: Text(S.exportKhongMoDuocShareSheet(e))));
  }
}

/// Sheet Export (P3-20): tab "Ghi buổi jam" (REC master) và tab "Export scene" (job có tiến độ).
/// Xong thì mở share sheet; file nằm ở `Documents/Exports/`.
class ExportSheet extends StatelessWidget {
  const ExportSheet({super.key, this.initialTab = 0});

  final int initialTab;

  @override
  Widget build(BuildContext context) {
    return DefaultTabController(
      length: 2,
      initialIndex: initialTab,
      child: SafeArea(
        child: Padding(
          padding: const EdgeInsets.fromLTRB(20, 8, 20, 16),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              Row(
                children: [
                  Text(S.exportTitle, style: const TextStyle(fontSize: 18, fontWeight: FontWeight.w600)),
                  const Spacer(),
                  IconButton(
                    key: const Key('export.close'),
                    icon: const Icon(Icons.close),
                    onPressed: () => Navigator.pop(context),
                  ),
                ],
              ),
              TabBar(
                tabs: [
                  Tab(key: Key('export.tab.jam'), text: S.exportGhiBuoiJam),
                  Tab(key: const Key('export.tab.scene'), text: S.exportSceneTab),
                ],
              ),
              const SizedBox(height: 320, child: TabBarView(children: [_JamTab(), _SceneTab()])),
            ],
          ),
        ),
      ),
    );
  }
}

// ───────────────────────── Ghi buổi jam ─────────────────────────

class _JamTab extends ConsumerWidget {
  const _JamTab();

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final jam = ref.watch(jamRecorderProvider);
    final last = jam.last;
    return Padding(
      padding: const EdgeInsets.only(top: 16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(S.exportGhiToanBoDauRa, style: TextStyle(color: AppColors.textSecondary)),
          const SizedBox(height: 20),
          Row(
            children: [
              Builder(
                builder: (btnContext) => _BigRecButton(
                  recording: jam.recording,
                  onTap: () {
                    final rec = ref.read(jamRecorderProvider.notifier);
                    if (!jam.recording) {
                      final err = rec.start();
                      if (err != null) {
                        ScaffoldMessenger.maybeOf(
                          context,
                        )?.showSnackBar(SnackBar(content: Text(S.exportKhongGhiDuoc(S.errorText(err)))));
                      }
                      return;
                    }
                    final r = rec.stop();
                    if (r == null) return;
                    warnDroppedFrames(context, ref, r); // thông báo không chặn: rớt khung khi ghi
                    unawaited(shareExport(btnContext, ref, r)); // xong → share sheet
                  },
                ),
              ),
              const SizedBox(width: 20),
              Expanded(
                child: jam.recording
                    ? Row(
                        children: [
                          Text(S.exportDangGhi, style: TextStyle(color: AppColors.record)),
                          ElapsedText(key: const Key('export.jam.elapsed'), since: jam.startedAt!),
                        ],
                      )
                    : Text(S.exportChamDeBatDauGhi, style: TextStyle(color: AppColors.textSecondary)),
              ),
            ],
          ),
          const Spacer(),
          if (last != null) _ResultRow(key: const Key('export.jam.result'), result: last),
        ],
      ),
    );
  }
}

class _BigRecButton extends StatelessWidget {
  const _BigRecButton({required this.recording, required this.onTap});

  final bool recording;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    return Semantics(
      button: true,
      label: recording ? S.exportDungGhiJam : S.exportGhiBuoiJam,
      child: GestureDetector(
        key: const Key('export.jam.rec'),
        onTap: onTap,
        child: Container(
          width: 72,
          height: 72,
          alignment: Alignment.center,
          decoration: BoxDecoration(
            shape: BoxShape.circle,
            border: Border.all(color: AppColors.record, width: 3),
          ),
          child: recording
              ? Container(
                  width: 26,
                  height: 26,
                  decoration: BoxDecoration(color: AppColors.record, borderRadius: BorderRadius.circular(4)),
                )
              : const DecoratedBox(
                  decoration: BoxDecoration(color: AppColors.record, shape: BoxShape.circle),
                  child: SizedBox(width: 52, height: 52),
                ),
        ),
      ),
    );
  }
}

/// "m:ss" từ [since], cập nhật mỗi giây. Ô kích thước cố định + boundary riêng: đổi chữ không relayout/vẽ lại
/// vùng xung quanh (transport bar).
class ElapsedText extends StatefulWidget {
  const ElapsedText({super.key, required this.since, this.style, this.width = 56, this.height = 20});

  final DateTime since;
  final TextStyle? style;
  final double width;
  final double height;

  @override
  State<ElapsedText> createState() => _ElapsedTextState();
}

class _ElapsedTextState extends State<ElapsedText> {
  late final Timer _timer = Timer.periodic(const Duration(seconds: 1), (_) => setState(() {}));

  @override
  void initState() {
    super.initState();
    _timer;
  }

  @override
  void dispose() {
    _timer.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final s = clock.now().difference(widget.since).inMilliseconds / 1000;
    return SizedBox(
      width: widget.width,
      height: widget.height,
      child: RepaintBoundary(
        child: Text(
          formatSeconds(s < 0 ? 0 : s),
          maxLines: 1,
          overflow: TextOverflow.clip,
          style: widget.style ?? AppText.numeric,
        ),
      ),
    );
  }
}

class _ResultRow extends ConsumerWidget {
  const _ResultRow({super.key, required this.result});

  final ExportResult result;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final extra = result.stems.isEmpty ? '' : S.exportStemCount(result.stems.length);
    return Row(
      children: [
        const Icon(Icons.check_circle, color: AppColors.play, size: 20),
        const SizedBox(width: 8),
        Expanded(
          child: Text(
            '${S.exportResult(result.fileName, formatSeconds(result.seconds))}$extra',
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
          ),
        ),
        Builder(
          builder: (btnContext) => FilledButton.icon(
            key: const Key('export.share'),
            onPressed: () => shareExport(btnContext, ref, result),
            icon: const Icon(Icons.ios_share, size: 18),
            label: Text(S.exportChiaSe),
          ),
        ),
      ],
    );
  }
}

// ───────────────────────── Export scene ─────────────────────────

class _SceneTab extends ConsumerStatefulWidget {
  const _SceneTab();

  @override
  ConsumerState<_SceneTab> createState() => _SceneTabState();
}

class _SceneTabState extends ConsumerState<_SceneTab> {
  late final ExportService _exports = ref.read(exportServiceProvider);
  late final JobTracker _jobs = ref.read(jobTrackerProvider);
  final _exportKey = GlobalKey();

  int _scene = 0;
  int _bars = 4;
  String _format = 'wav';
  bool _stems = false;
  int? _jobId;
  double _progress = 0;
  ExportResult? _result;
  String? _error;
  StreamSubscription<JobProgress>? _progressSub;

  static const barChoices = [1, 2, 4, 8, 16];

  @override
  void dispose() {
    _progressSub?.cancel();
    final id = _jobId;
    if (id != null) _jobs.cancel(id); // đóng sheet giữa chừng → huỷ export
    super.dispose();
  }

  Future<void> _export() async {
    final project = ref.read(projectControllerProvider)?.project;
    if (project == null) return;
    final dir = ref.read(exportsDirProvider);
    final sceneName = _sceneName(project.scenes, _scene);
    final path =
        '${dir.path}/${safeFileName(project.name)}_${safeFileName(sceneName)}_${fileStamp(clock.now())}.$_format';
    final int id;
    try {
      dir.createSync(recursive: true);
      id = _exports.exportScene(scene: _scene, bars: _bars, path: path, format: _format, stems: _stems);
    } on EngineCallException catch (e) {
      setState(() => _error = S.exportExportLoi(S.errorText(e.code)));
      return;
    }
    _progressSub = _jobs.progress.where((p) => p.jobId == id).listen((p) => setState(() => _progress = p.progress));
    setState(() {
      _jobId = id;
      _progress = 0;
      _result = null;
      _error = null;
    });
    try {
      final r = ExportResult.fromJson(await _jobs.awaitJob(id), fallbackPath: path);
      if (!mounted) return;
      setState(() {
        _jobId = null;
        _result = r;
      });
      if (r.clippedSamples > 0) {
        // Thông báo không chặn (eb chốt c): bản export bị clip → gợi ý giảm gain master.
        ScaffoldMessenger.maybeOf(
          context,
        )?.showSnackBar(SnackBar(content: Text(S.exportClippedSamples(r.clippedSamples))));
      }
      final ctx = _exportKey.currentContext;
      if (ctx != null && ctx.mounted) await shareExport(ctx, ref, r); // xong → share sheet
    } on EngineJobException catch (e) {
      if (!mounted) return;
      setState(() {
        _jobId = null;
        if (e.code != 'JOB_CANCELLED') _error = S.exportExportLoi(S.errorText(e.code));
      });
    } finally {
      unawaited(_progressSub?.cancel());
      _progressSub = null;
    }
  }

  void _cancel() {
    final id = _jobId;
    if (id != null) _jobs.cancel(id);
  }

  static String _sceneName(List<Scene> scenes, int i) {
    for (final s in scenes) {
      if (s.index == i) return s.name;
    }
    return S.sessionSceneName(i + 1);
  }

  @override
  Widget build(BuildContext context) {
    final project = ref.watch(projectControllerProvider.select((s) => s?.project));
    final scenes = project?.scenes ?? const <Scene>[];
    final hasClips = project?.tracks.any((t) => t.clipAt(_scene) != null) ?? false;
    final running = _jobId != null;
    return Padding(
      padding: const EdgeInsets.only(top: 12),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              _Labeled(
                label: S.exportScene,
                child: SizedBox(
                  width: 280,
                  child: DropdownButton<int>(
                    key: const Key('export.scene.pick'),
                    isExpanded: true,
                    value: _scene,
                    onChanged: running ? null : (v) => setState(() => _scene = v ?? 0),
                    items: [
                      for (var i = 0; i < SessionLayout.scenes; i++)
                        DropdownMenuItem(
                          value: i,
                          child: Text(
                            S.exportSceneItem(i + 1, _sceneName(scenes, i)),
                            maxLines: 1,
                            overflow: TextOverflow.ellipsis,
                          ),
                        ),
                    ],
                  ),
                ),
              ),
              const SizedBox(width: 24),
              _Labeled(
                label: S.exportSoBar,
                child: SegmentedButton<int>(
                  key: const Key('export.scene.bars'),
                  showSelectedIcon: false,
                  segments: [for (final b in barChoices) ButtonSegment(value: b, label: Text('$b'))],
                  selected: {_bars},
                  onSelectionChanged: running ? null : (s) => setState(() => _bars = s.first),
                ),
              ),
            ],
          ),
          const SizedBox(height: 12),
          Row(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              _Labeled(
                label: S.exportDinhDang,
                child: SegmentedButton<String>(
                  key: const Key('export.scene.format'),
                  showSelectedIcon: false,
                  segments: const [
                    ButtonSegment(value: 'wav', label: Text('WAV')),
                    ButtonSegment(value: 'm4a', label: Text('M4A')),
                  ],
                  selected: {_format},
                  onSelectionChanged: running ? null : (s) => setState(() => _format = s.first),
                ),
              ),
              const SizedBox(width: 24),
              Expanded(
                child: _Labeled(
                  label: S.exportStemsLabel,
                  child: Row(
                    children: [
                      Switch(
                        key: const Key('export.scene.stems'),
                        value: _stems,
                        onChanged: running ? null : (v) => setState(() => _stems = v),
                      ),
                      Flexible(
                        child: Text(
                          S.exportMoiTrackMotFile,
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                          style: TextStyle(color: AppColors.textSecondary),
                        ),
                      ),
                    ],
                  ),
                ),
              ),
            ],
          ),
          const SizedBox(height: 16),
          Row(
            children: [
              FilledButton.icon(
                key: _exportKey,
                onPressed: running || !hasClips ? null : _export,
                icon: const Icon(Icons.file_upload_outlined, size: 18),
                label: Text(S.exportTitle),
              ),
              const SizedBox(width: 16),
              if (running) ...[
                Expanded(
                  child: LinearProgressIndicator(key: const Key('export.scene.progress'), value: _progress),
                ),
                const SizedBox(width: 12),
                Text('${(_progress * 100).round()}%', style: AppText.numeric),
                TextButton(key: const Key('export.scene.cancel'), onPressed: _cancel, child: Text(S.exportHuy)),
              ] else if (!hasClips)
                Flexible(
                  child: Text(S.exportSceneNayChuaCoClip, style: TextStyle(color: AppColors.textSecondary)),
                ),
            ],
          ),
          if (_error != null) ...[
            const SizedBox(height: 8),
            Text(_error!, style: const TextStyle(color: AppColors.record)),
          ],
          const Spacer(),
          if (_result != null) _ResultRow(key: const Key('export.scene.result'), result: _result!),
        ],
      ),
    );
  }
}

class _Labeled extends StatelessWidget {
  const _Labeled({required this.label, required this.child});

  final String label;
  final Widget child;

  @override
  Widget build(BuildContext context) => Column(
    mainAxisSize: MainAxisSize.min,
    crossAxisAlignment: CrossAxisAlignment.start,
    children: [
      Text(label, style: const TextStyle(fontSize: 12, color: AppColors.textSecondary)),
      const SizedBox(height: 4),
      child,
    ],
  );
}
