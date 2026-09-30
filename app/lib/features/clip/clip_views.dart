import 'dart:async';
import 'dart:math' as math;

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app/theme.dart';
import '../../engine/engine_providers.dart';
import '../../engine/engine_state.dart';
import '../../engine/engine_state_ticker.dart';
import '../../model/project.dart';
import '../session/live_clip_notes.dart';
import '../session/project_controller.dart';
import '../session/session_layout.dart';
import '../../engine/performance_actions.dart';
import '../session/session_ui.dart';
import 'clip_input.dart';
import 'note_edit.dart';
import 'pad_names.dart';
import 'piano_roll.dart';
import 'waveform.dart';
import '../../l10n/l10n.dart';

/// Lịch sử sửa nốt theo clip (07 §4.1b: undo/redo 50 bước, chỉ trong phiên).
final noteHistoriesProvider = Provider<NoteHistories>((ref) => NoteHistories());

/// Tab Clip · clip MIDI (P2-19 + P2-29 + P2-31): piano roll chế độ ✏️ Vẽ / ⬚ Chọn, lưới, zoom, độ dài clip,
/// ±quãng tám, undo/redo; Chọn: chọn tất cả, nhân bản nhóm, xoá nốt đã chọn, quantize, clear. Mỗi thao tác xong →
/// `clip.setMidi` + model (một lần). Lựa chọn giữ qua thao tác nhóm (dời / đổi độ dài / velocity / nhân bản).
class MidiClipView extends ConsumerStatefulWidget {
  const MidiClipView({super.key, required this.track, required this.clip});

  final int track;
  final MidiClip clip;

  @override
  ConsumerState<MidiClipView> createState() => _MidiClipViewState();
}

class _MidiClipViewState extends ConsumerState<MidiClipView> {
  Set<int> _selected = const {};

  /// Danh sách nốt vừa commit: model trả về đúng danh sách này thì giữ lựa chọn, khác (undo, quantize…) thì bỏ.
  List<Note>? _expect;

  /// Lưới dùng chung với ⇥ Step (sessionUi.grid).
  double get _grid => ref.read(sessionUiProvider).grid;
  int _zoom = 1;
  final _vertical = ScrollController();

  static const quantizeGrids = {'1/16': 0.25, '1/8': 0.5, '1/4': 1.0};
  static const zooms = [1, 2, 4];
  static const lengthsInBars = [1, 2, 4, 8];

  MidiClip get c => widget.clip;
  NoteHistory get _history => ref.read(noteHistoriesProvider).of(c.id);
  ProjectController get _ctl => ref.read(projectControllerProvider.notifier);

  @override
  void dispose() {
    _vertical.dispose();
    super.dispose();
  }

  @override
  void didUpdateWidget(MidiClipView old) {
    super.didUpdateWidget(old);
    // So theo giá trị: getter `notes` của freezed trả wrapper mới mỗi lần gọi.
    if (old.clip.id != widget.clip.id ||
        (!listEquals(old.clip.notes, widget.clip.notes) && !listEquals(widget.clip.notes, _expect))) {
      _selected = const {};
    }
  }

  /// Một thao tác sửa nốt: ghi lịch sử (trạng thái TRƯỚC) rồi gửi engine + model; [selection] là lựa chọn sau đó.
  void _commit(List<Note> notes, {double? length, Set<int> selection = const {}}) {
    _history.push(c.notes, c.lengthBeats);
    _expect = notes;
    _ctl.setMidiNotes(widget.track, c.slot, notes, lengthBeats: length);
    setState(() => _selected = selection);
  }

  void _undo() {
    final r = _history.undo(c.notes, c.lengthBeats);
    if (r != null) _ctl.setMidiNotes(widget.track, c.slot, r.notes, lengthBeats: r.length);
    setState(() => _selected = const {});
  }

  void _redo() {
    final r = _history.redo(c.notes, c.lengthBeats);
    if (r != null) _ctl.setMidiNotes(widget.track, c.slot, r.notes, lengthBeats: r.length);
    setState(() => _selected = const {});
  }

  /// Chọn nốt (chạm, khung, thước, chọn tất cả). Chọn ở chế độ Vẽ (kéo thước) → chuyển sang chế độ Chọn.
  void _select(Set<int> s) {
    setState(() => _selected = s);
    if (s.isNotEmpty && ref.read(sessionUiProvider).pianoRollMode == PianoRollMode.draw) {
      ref.read(sessionUiProvider.notifier).setPianoRollMode(PianoRollMode.select);
    }
  }

  /// Nhân bản nhóm đã chọn, đặt ngay sau nó (07 §4.1b). Hết chỗ → hỏi tăng độ dài clip (bar nhỏ nhất đủ chứa).
  Future<void> _duplicate(int beatsPerBar) async {
    final before = c.notes;
    final r = NoteEdit.duplicate(before, _selected, grid: _grid);
    double? length;
    if (r.end > c.lengthBeats + 1e-9) {
      final need = (r.end / beatsPerBar - 1e-9).ceil();
      final bars = lengthsInBars.where((b) => b >= need).firstOrNull;
      if (bars == null) {
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(S.clipKhongDuChoToiDa(lengthsInBars.last))));
        return;
      }
      final ok = await showDialog<bool>(
        context: context,
        builder: (ctx) => AlertDialog(
          title: Text(S.clipKhongDuCho),
          content: Text(S.clipTangDoDaiHoi(bars)),
          actions: [
            TextButton(
              key: const Key('midi.extend.cancel'),
              onPressed: () => Navigator.pop(ctx, false),
              child: Text(S.commonCancel),
            ),
            FilledButton(
              key: const Key('midi.extend.ok'),
              onPressed: () => Navigator.pop(ctx, true),
              child: Text(S.clipTangDoDai),
            ),
          ],
        ),
      );
      if (ok != true || !mounted || !listEquals(c.notes, before)) return;
      length = bars * beatsPerBar.toDouble();
    }
    _commit(r.notes, length: length, selection: r.copies);
  }

  /// Nốt vừa thêm được nghe thử: NOTE_ON rồi NOTE_OFF sau 150 ms trên track (07 §4.1b).
  void _audition(int pitch) {
    final actions = ref.read(performanceActionsProvider);
    actions.noteOn(widget.track, pitch, NoteEdit.defaultVelocity / 127);
    Timer(const Duration(milliseconds: 150), () => actions.noteOff(widget.track, pitch));
  }

  void _octave(int dir) {
    if (!_vertical.hasClients) return;
    final target = (_vertical.offset + dir * 12 * PianoRollEditor.pianoRowH).clamp(
      0.0,
      _vertical.position.maxScrollExtent,
    );
    _vertical.animateTo(target, duration: const Duration(milliseconds: 180), curve: Curves.easeOut);
  }

  @override
  Widget build(BuildContext context) {
    final color = ref.watch(
      projectControllerProvider.select((s) => parseHexColor(s?.project.trackAt(widget.track)?.color ?? '#7BD88F')),
    );
    final instrument = ref.watch(projectControllerProvider.select((s) => s?.project.trackAt(widget.track)?.instrument));
    final beatsPerBar = ref.watch(
      projectControllerProvider.select((s) => s?.project.transport.timeSignature.first ?? 4),
    );
    final kitPath = switch (instrument) {
      SfzInstrumentRef(:final path) when path.startsWith('kits/') => path,
      _ => null,
    };
    final pads = kitPath == null ? null : ref.watch(padNamesProvider(kitPath)).value;
    final kit = pads != null && pads.isNotEmpty;
    final mode = ref.watch(sessionUiProvider.select((u) => u.pianoRollMode));
    ref.watch(sessionUiProvider.select((u) => u.grid));
    final keyboard = ref.watch(sessionUiProvider.select((u) => u.clipKeyboardVisible));
    final step = ref.watch(sessionUiProvider.select((u) => u.stepMode && u.clipKeyboardVisible));
    // Sang chế độ Vẽ thì bỏ lựa chọn (chạm nốt ở chế độ Vẽ là xoá, không phải chọn).
    ref.listen(sessionUiProvider.select((u) => u.pianoRollMode), (_, m) {
      if (m == PianoRollMode.draw && _selected.isNotEmpty) setState(() => _selected = const {});
    });
    final draw = mode == PianoRollMode.draw;
    final selected = {
      for (final i in _selected)
        if (i < c.notes.length) i,
    };
    final bars = (c.lengthBeats / beatsPerBar).round();
    final small = Theme.of(context).textTheme.bodySmall;
    Widget iconBtn(String key, IconData icon, String tip, VoidCallback? onPressed) => IconButton(
      key: Key(key),
      tooltip: tip,
      visualDensity: VisualDensity.compact,
      icon: Icon(icon, size: 20),
      onPressed: onPressed,
    );
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Row(
          children: [
            Flexible(
              child: Text.rich(
                TextSpan(
                  children: [
                    TextSpan(
                      text: c.name,
                      style: const TextStyle(fontWeight: FontWeight.w600),
                    ),
                    TextSpan(text: '  ${S.clipNotBeat(c.notes.length, _beats(c.lengthBeats))}', style: small),
                  ],
                ),
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
              ),
            ),
            const SizedBox(width: 8),
            SegmentedButton<PianoRollMode>(
              key: const Key('midi.mode'),
              showSelectedIcon: false,
              style: const ButtonStyle(visualDensity: VisualDensity.compact),
              // Chỉ icon (tooltip = tên chế độ) để thanh công cụ vừa một hàng ở cả en / vi.
              segments: [
                ButtonSegment(
                  value: PianoRollMode.draw,
                  tooltip: S.clipModeVe,
                  icon: const Icon(Icons.edit, size: 18, key: Key('midi.mode.draw')),
                ),
                ButtonSegment(
                  value: PianoRollMode.select,
                  tooltip: S.clipModeChon,
                  icon: const Icon(Icons.highlight_alt, size: 18, key: Key('midi.mode.select')),
                ),
              ],
              selected: {mode},
              onSelectionChanged: (v) => ref.read(sessionUiProvider.notifier).setPianoRollMode(v.first),
            ),
            PopupMenuButton<double>(
              key: const Key('midi.grid'),
              tooltip: S.clipLuoi,
              onSelected: (g) => ref.read(sessionUiProvider.notifier).setGrid(g),
              itemBuilder: (_) => [
                for (final e in NoteEdit.grids.entries)
                  CheckedPopupMenuItem(
                    key: Key('midi.grid.${e.key}'),
                    value: e.value,
                    checked: e.value == _grid,
                    child: Text(e.key),
                  ),
              ],
              child: _ToolChip(
                icon: Icons.grid_4x4,
                label: NoteEdit.grids.entries.firstWhere((e) => e.value == _grid).key,
              ),
            ),
            PopupMenuButton<int>(
              key: const Key('midi.zoom'),
              tooltip: S.clipZoom,
              onSelected: (z) => setState(() => _zoom = z),
              itemBuilder: (_) => [
                for (final z in zooms)
                  CheckedPopupMenuItem(key: Key('midi.zoom.$z'), value: z, checked: z == _zoom, child: Text('$z×')),
              ],
              child: _ToolChip(icon: Icons.zoom_in, label: '$_zoom×'),
            ),
            PopupMenuButton<int>(
              key: const Key('midi.length'),
              tooltip: S.clipDoDaiClip,
              onSelected: (b) {
                final beats = b * beatsPerBar.toDouble();
                if (beats != c.lengthBeats) _commit(NoteEdit.fitLength(c.notes, beats), length: beats);
              },
              itemBuilder: (_) => [
                for (final b in lengthsInBars)
                  CheckedPopupMenuItem(
                    key: Key('midi.length.$b'),
                    value: b,
                    checked: b == bars,
                    child: Text(S.audioBars(b)),
                  ),
              ],
              child: _ToolChip(icon: Icons.straighten, label: S.audioBars(bars)),
            ),
            // 07 §4.1d: bàn phím / pad dưới piano roll (bật thì mở ⤢).
            IconButton(
              key: const Key('midi.keyboard'),
              tooltip: S.instrumentBanPhim,
              visualDensity: VisualDensity.compact,
              isSelected: keyboard,
              icon: const Icon(Icons.piano_outlined, size: 20),
              selectedIcon: const Icon(Icons.piano, size: 20),
              onPressed: () => ref.read(sessionUiProvider.notifier).setClipKeyboard(!keyboard),
            ),
            if (!kit) ...[
              iconBtn('midi.octDown', Icons.keyboard_double_arrow_down, S.clipQuangTamXuong, () => _octave(1)),
              iconBtn('midi.octUp', Icons.keyboard_double_arrow_up, S.clipQuangTamLen, () => _octave(-1)),
            ],
            iconBtn('midi.undo', Icons.undo, S.clipHoanTacSua, _history.canUndo ? _undo : null),
            iconBtn('midi.redo', Icons.redo, S.clipLamLai, _history.canRedo ? _redo : null),
            if (!draw) ...[
              iconBtn(
                'midi.selectAll',
                Icons.select_all,
                S.clipChonTatCa,
                c.notes.isEmpty ? null : () => _select({for (var i = 0; i < c.notes.length; i++) i}),
              ),
              iconBtn(
                'midi.duplicate',
                Icons.library_add_outlined,
                S.clipNhanBan,
                selected.isEmpty ? null : () => _duplicate(beatsPerBar),
              ),
              PopupMenuButton<double>(
                key: const Key('midi.quantize'),
                tooltip: S.clipQuantize,
                onSelected: (g) {
                  _history.push(c.notes, c.lengthBeats);
                  _ctl.quantizeClip(widget.track, c.slot, g);
                  setState(() {});
                },
                itemBuilder: (_) => [
                  for (final e in quantizeGrids.entries)
                    PopupMenuItem(
                      key: Key('midi.quantize.${e.key}'),
                      value: e.value,
                      child: Text(S.clipQuantizeGrid(e.key)),
                    ),
                ],
                child: const Padding(padding: EdgeInsets.all(8), child: Icon(Icons.grid_on, size: 20)),
              ),
              IconButton(
                key: const Key('midi.deleteNotes'),
                tooltip: S.clipXoaNotDaChon(selected.length),
                visualDensity: VisualDensity.compact,
                onPressed: selected.isEmpty
                    ? null
                    : () => _commit([
                        for (var i = 0; i < c.notes.length; i++)
                          if (!selected.contains(i)) c.notes[i],
                      ]),
                icon: Badge(
                  isLabelVisible: selected.isNotEmpty,
                  label: Text('${selected.length}'),
                  child: const Icon(Icons.delete_outline, size: 20),
                ),
              ),
              iconBtn('midi.clear', Icons.clear_all, S.clipClear, c.notes.isEmpty ? null : () => _commit(const [])),
            ],
          ],
        ),
        const SizedBox(height: 4),
        Expanded(
          child: PianoRollEditor(
            clip: c,
            rows: pianoRollRows(pads: kit ? pads : null, notes: c.notes),
            kit: kit,
            mode: mode,
            grid: _grid,
            stepCursor: step ? ref.read(stepCursorProvider) : null,
            liveNotes: ref.read(liveClipNotesProvider).of(widget.track, c.slot),
            onStepCursor: step ? (b) => ref.read(stepCursorProvider).value = b : null,
            zoom: _zoom,
            selected: selected,
            color: color,
            track: widget.track,
            beatsPerBar: beatsPerBar,
            ticker: ref.watch(engineStateTickerProvider),
            verticalController: _vertical,
            onToggle: (i) => setState(() {
              final s = {..._selected};
              if (!s.remove(i)) s.add(i);
              _selected = s;
            }),
            onSelect: _select,
            onCommit: (notes, selection) => _commit(notes, selection: selection),
            onAudition: _audition,
          ),
        ),
      ],
    );
  }
}

/// Tab Clip · clip audio (P2-20/21): waveform (cache Picture theo zoom), vùng loop kéo được,
/// gain, warp. Playhead ở lớp riêng.
class AudioClipView extends ConsumerStatefulWidget {
  const AudioClipView({super.key, required this.track, required this.clip});

  final int track;
  final AudioClip clip;

  @override
  ConsumerState<AudioClipView> createState() => _AudioClipViewState();
}

class _AudioClipViewState extends ConsumerState<AudioClipView> {
  static const zooms = [1, 2, 4];
  int _zoom = 0;

  // Vị trí handle đang kéo (sample); null = theo model.
  int? _dragStart;
  int? _dragEnd;
  double _gainDraft = double.nan;

  AudioClip get c => widget.clip;

  double get _sr {
    final sr = ref.read(engineStateTickerProvider).state.sampleRate;
    return sr > 0 ? sr : 48000;
  }

  int get _loopLenSamples => (c.lengthBeats * 60 / c.originalBpm * _sr).round();

  @override
  Widget build(BuildContext context) {
    final ctl = ref.read(projectControllerProvider.notifier);
    final ticker = ref.watch(engineStateTickerProvider);
    final cache = ref.watch(waveformCacheProvider);
    // Overdub xong → revision tăng → waveform lấy peaks mới (04 §5.4).
    final revision = ref.watch(projectControllerProvider.select((s) => s?.revisionOf(c.id) ?? 0));
    final drums = c.tags.contains('drums'); // 06 §2 clips[].tags
    final small = Theme.of(context).textTheme.bodySmall;
    final gain = _gainDraft.isNaN ? c.gainDb : _gainDraft;
    return Row(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              Row(
                children: [
                  Flexible(
                    child: Text(
                      c.name,
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: const TextStyle(fontWeight: FontWeight.w600),
                    ),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    flex: 3,
                    child: Text(
                      S.clipLoopTuSBeatGoc(
                        (c.loop.startSample / _sr).toStringAsFixed(2),
                        _beats(c.lengthBeats),
                        c.originalBpm.toStringAsFixed(1),
                      ),
                      key: const Key('audio.loopLabel'),
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: small,
                    ),
                  ),
                  SegmentedButton<int>(
                    key: const Key('audio.zoom'),
                    segments: [
                      for (var i = 0; i < zooms.length; i++) ButtonSegment(value: i, label: Text('${zooms[i]}×')),
                    ],
                    selected: {_zoom},
                    onSelectionChanged: (s) => setState(() => _zoom = s.first),
                  ),
                  UndoOverdubButton(track: widget.track, slot: c.slot, clip: c, compact: true),
                ],
              ),
              const SizedBox(height: 4),
              Expanded(
                child: LayoutBuilder(
                  builder: (context, box) {
                    final size = Size(box.maxWidth * zooms[_zoom], box.maxHeight);
                    final est = ((c.loop.startSample + _loopLenSamples) * 1.5).round();
                    final pic = cache.get(
                      clipId: c.id,
                      zoom: _zoom,
                      revision: revision,
                      size: size,
                      estimatedSamples: math.max(est, 48000),
                    );
                    final total = pic?.totalSamples ?? math.max(est, 1);
                    double xOf(int sample) => sample / total * size.width;
                    int sampleOf(double x) => (x / size.width * total).round().clamp(0, total);
                    final start = _dragStart ?? c.loop.startSample;
                    final end = _dragEnd ?? (c.loop.startSample + _loopLenSamples);
                    return SingleChildScrollView(
                      scrollDirection: Axis.horizontal,
                      child: SizedBox.fromSize(
                        size: size,
                        child: Stack(
                          children: [
                            RepaintBoundary(
                              child: CustomPaint(
                                key: const Key('audio.waveform'),
                                size: size,
                                painter: WaveformPicturePainter(pic),
                              ),
                            ),
                            Positioned.fill(
                              child: IgnorePointer(
                                child: CustomPaint(painter: _LoopShadePainter(xOf(start), xOf(end))),
                              ),
                            ),
                            Positioned.fill(
                              child: RepaintBoundary(
                                child: IgnorePointer(
                                  child: CustomPaint(
                                    painter: _LoopPlayheadPainter(ticker, widget.track, c.slot, xOf(start), xOf(end)),
                                  ),
                                ),
                              ),
                            ),
                            _handle(
                              key: const Key('audio.loopStart'),
                              x: xOf(start),
                              onDrag: (dx) => setState(() {
                                final s = sampleOf(xOf(start) + dx);
                                _dragStart = math.min(s, end - (_sr * 60 / c.originalBpm * 0.25).round());
                              }),
                              onEnd: () => _commit(ctl),
                            ),
                            _handle(
                              key: const Key('audio.loopEnd'),
                              x: xOf(end),
                              onDrag: (dx) => setState(() => _dragEnd = math.max(sampleOf(xOf(end) + dx), start + 1)),
                              onEnd: () => _commit(ctl),
                            ),
                          ],
                        ),
                      ),
                    );
                  },
                ),
              ),
            ],
          ),
        ),
        const SizedBox(width: 12),
        SizedBox(
          width: 190 + SessionLayout.sceneColumnWidth - 40,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(S.clipGain(gain.toStringAsFixed(1)), style: AppText.numeric),
              Slider(
                key: const Key('audio.gain'),
                min: -24,
                max: 12,
                value: gain.clamp(-24.0, 12.0),
                onChanged: (v) => setState(() => _gainDraft = v),
                onChangeEnd: (v) {
                  ctl.setClipAudioParams(widget.track, c.slot, gainDb: v);
                  setState(() => _gainDraft = double.nan);
                },
              ),
              // 06 §4: loop trống nghe tự nhiên hơn khi Re-Pitch → gợi ý thay nhãn (không thêm chiều cao cột).
              if (drums && c.warp == WarpMode.stretch)
                Text(
                  S.clipDrumsHint,
                  key: const Key('audio.drumsHint'),
                  maxLines: 2,
                  overflow: TextOverflow.ellipsis,
                  style: small?.copyWith(color: AppColors.queued),
                )
              else
                Text(S.clipWarp, style: small),
              const SizedBox(height: 4),
              SegmentedButton<WarpMode>(
                key: const Key('audio.warp'),
                segments: [
                  ButtonSegment(value: WarpMode.stretch, label: Text(S.warpMode('stretch'))),
                  ButtonSegment(value: WarpMode.repitch, label: Text(S.warpMode('repitch'))),
                ],
                selected: {c.warp},
                onSelectionChanged: (s) => ctl.setClipAudioParams(widget.track, c.slot, warp: s.first),
              ),
            ],
          ),
        ),
      ],
    );
  }

  Widget _handle({
    required Key key,
    required double x,
    required ValueChanged<double> onDrag,
    required VoidCallback onEnd,
  }) {
    return Positioned(
      left: x - 14,
      top: 0,
      bottom: 0,
      width: 28,
      child: GestureDetector(
        key: key,
        behavior: HitTestBehavior.opaque,
        onHorizontalDragUpdate: (d) => onDrag(d.delta.dx),
        onHorizontalDragEnd: (_) => onEnd(),
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

  /// Nhả handle → `clip.setLoopRegion`. Độ dài làm tròn về 1/4 beat.
  void _commit(ProjectController ctl) {
    final start = _dragStart ?? c.loop.startSample;
    final end = _dragEnd ?? (c.loop.startSample + _loopLenSamples);
    final beats = ((end - start) / _sr * c.originalBpm / 60 * 4).round() / 4;
    ctl.setLoopRegion(widget.track, c.slot, startSample: start, lengthBeats: beats);
    setState(() => _dragStart = _dragEnd = null);
  }
}

String _beats(double b) => b.toStringAsFixed(b % 1 == 0 ? 0 : 2);

class _ToolChip extends StatelessWidget {
  const _ToolChip({required this.icon, required this.label});
  final IconData icon;
  final String label;

  @override
  Widget build(BuildContext context) => Padding(
    padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 6),
    child: Row(children: [Icon(icon, size: 18), const SizedBox(width: 4), Text(label)]),
  );
}

class _LoopShadePainter extends CustomPainter {
  _LoopShadePainter(this.x0, this.x1);
  final double x0;
  final double x1;
  static final _shade = Paint()..color = const Color(0x99000000);

  @override
  void paint(Canvas canvas, Size size) {
    canvas.drawRect(Rect.fromLTRB(0, 0, x0, size.height), _shade);
    canvas.drawRect(Rect.fromLTRB(x1, 0, size.width, size.height), _shade);
  }

  @override
  bool shouldRepaint(_LoopShadePainter old) => old.x0 != x0 || old.x1 != x1;
}

class _LoopPlayheadPainter extends CustomPainter {
  _LoopPlayheadPainter(this.ticker, this.track, this.slot, this.x0, this.x1) : super(repaint: ticker);
  final EngineStateTicker ticker;
  final int track;
  final int slot;
  final double x0;
  final double x1;
  static final _line = Paint()
    ..color = AppColors.play
    ..strokeWidth = 2;

  @override
  void paint(Canvas canvas, Size size) {
    final s = ticker.state;
    if (s.trackPlayingSlot[track] != slot) return;
    final x = x0 + s.trackClipProgress[track].clamp(0.0, 1.0) * (x1 - x0);
    canvas.drawLine(Offset(x, 0), Offset(x, size.height), _line);
  }

  @override
  bool shouldRepaint(_LoopPlayheadPainter old) =>
      old.ticker != ticker || old.x0 != x0 || old.x1 != x1 || old.track != track || old.slot != slot;
}

/// "Hoàn tác overdub" (`clip.undoOverdub`, chỉ clip AUDIO có lớp undo): chỉ bật khi `clip.info.hasUndo`. Chỉ hỏi engine khi
/// vừa hiện, khi ô vừa HẾT overdub, hoặc khi [clip] đổi (sửa nốt / thay clip) — không theo frame.
class UndoOverdubButton extends ConsumerStatefulWidget {
  const UndoOverdubButton({
    super.key,
    required this.track,
    required this.slot,
    required this.clip,
    this.compact = false,
  });

  final int track;
  final int slot;

  /// Clip đang hiện: đổi (bất biến → khác instance) thì hỏi lại.
  final Clip clip;

  /// Chỉ icon (header hẹp).
  final bool compact;

  @override
  ConsumerState<UndoOverdubButton> createState() => _UndoOverdubButtonState();
}

class _UndoOverdubButtonState extends ConsumerState<UndoOverdubButton> {
  late final ValueListenable<ClipState> _cell = ref.read(engineStateTickerProvider).clip(widget.track, widget.slot);
  late ClipState _last = _cell.value;
  bool _hasUndo = false;

  @override
  void initState() {
    super.initState();
    _refresh();
    _cell.addListener(_onCell);
  }

  @override
  void didUpdateWidget(UndoOverdubButton old) {
    super.didUpdateWidget(old);
    if (!identical(old.clip, widget.clip)) _refresh();
  }

  @override
  void dispose() {
    _cell.removeListener(_onCell);
    super.dispose();
  }

  void _onCell() {
    final now = _cell.value;
    final endedOverdub = _last == ClipState.overdubbing && now != ClipState.overdubbing;
    _last = now;
    if (now == ClipState.empty) {
      setState(() => _hasUndo = false);
    } else if (endedOverdub) {
      setState(_refresh);
    }
  }

  void _refresh() => _hasUndo = ref.read(projectControllerProvider.notifier).hasUndo(widget.track, widget.slot);

  @override
  Widget build(BuildContext context) {
    final VoidCallback? onPressed = _hasUndo
        ? () {
            ref.read(projectControllerProvider.notifier).undoOverdub(widget.track, widget.slot);
            setState(_refresh);
          }
        : null;
    if (widget.compact) {
      return IconButton(
        key: const Key('clip.undoOverdub'),
        tooltip: S.clipHoanTacOverdub,
        onPressed: onPressed,
        icon: const Icon(Icons.undo, size: 20),
      );
    }
    return TextButton.icon(
      key: const Key('clip.undoOverdub'),
      onPressed: onPressed,
      icon: const Icon(Icons.undo, size: 18),
      label: Text(S.clipHoanTacOverdub),
    );
  }
}
