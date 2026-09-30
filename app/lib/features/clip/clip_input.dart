import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app/theme.dart';
import '../../data/library_repository.dart';
import '../../engine/engine_providers.dart';
import '../../engine/engine_state.dart';
import '../../engine/performance_actions.dart';
import '../../l10n/l10n.dart';
import '../../model/project.dart';
import '../../ui_kit/keyboard_view.dart';
import '../instrument/note_player.dart';
import '../instrument/note_surface.dart';
import '../session/project_controller.dart';
import '../session/session_layout.dart';
import '../session/session_ui.dart';
import '../settings/app_settings.dart';
import 'clip_views.dart';
import 'note_edit.dart';
import 'pad_names.dart';

/// Con trỏ ⇥ Step trên piano roll (beat). ValueNotifier: con trỏ tiến chỉ vẽ lại lớp của piano roll, không rebuild.
final stepCursorProvider = Provider<ValueNotifier<double>>((ref) {
  final n = ValueNotifier<double>(0);
  ref.onDispose(n.dispose);
  return n;
});

/// Bàn phím (track nhạc cụ) hoặc pad 4×4 (track kit) dưới piano roll ở tab Clip (07 §4.1d). Chơi thẳng NOTE_ON/OFF
/// trên track; dùng lại [NoteSurface] (1 Listener, 0 rebuild khi chơi).
/// - ● Ghi: clip chạy lặp (tự launch nếu đang dừng; ô trống thì tạo clip MIDI rỗng), nốt đánh được engine ghi chồng
///   mỗi vòng (overdub MIDI, quantize theo `midi.setRecordQuantize`). Bấm lại → dừng ghi, clip vẫn phát. Engine báo
///   RECORDING_FINISHED → controller đọc lại nốt; cả lượt ghi là một bước ↶.
/// - ⇥ Step: phím bấm → nốt dài 1 ô lưới tại con trỏ, con trỏ tiến 1 ô; nhiều phím giữ cùng lúc = hợp âm (ghi khi
///   nhấc ngón cuối). Nghỉ = tiến không ghi; ⌫ = lùi và xoá. Làm phía app (`clip.setMidi`), có ↶.
class ClipKeyboardSection extends ConsumerStatefulWidget {
  const ClipKeyboardSection({super.key, required this.track, required this.slot});

  final int track;
  final int slot;

  static const headerHeight = 40.0;

  /// Chiều cao mặt phím: pad 4×4 cần ô vuông đủ lớn cho ngón tay.
  static double surfaceHeight({required bool kit}) => kit ? 180 : 130;

  @override
  ConsumerState<ClipKeyboardSection> createState() => _ClipKeyboardSectionState();
}

class _ClipKeyboardSectionState extends ConsumerState<ClipKeyboardSection> {
  static const baseKeyboardNote = 48; // C3, như tab Instrument

  // Lấy 1 lần: dispose() còn phải gửi NOTE_OFF cho nốt đang giữ.
  late final PerformanceActions _actions = ref.read(performanceActionsProvider);
  late final NotePlayer _player = NotePlayer(noteOn: _noteOn, noteOff: _noteOff);
  late final ValueNotifier<double> _cursor = ref.read(stepCursorProvider);
  late ValueListenable<ClipState> _cell;
  int _octave = 0;

  /// Đã launch clip, chờ ô sang Playing để bật overdub.
  bool _pending = false;

  /// Nốt / độ dài trước lượt ghi Live: lần đầu engine trả nốt mới thì ghi vào ↶ (một bước cho cả lượt).
  ({List<Note> notes, double length})? _liveBefore;

  // Step: phím đang giữ và hợp âm đang gom.
  final _held = <int>{};
  final _chord = <int, int>{};
  double _chordAt = 0;

  int get _track => widget.track;
  int get _slot => widget.slot;
  ProjectController get _ctl => ref.read(projectControllerProvider.notifier);
  SessionUi get _ui => ref.read(sessionUiProvider);

  Clip? get _clip => ref.read(projectControllerProvider)?.project.trackAt(_track)?.clipAt(_slot);

  @override
  void initState() {
    super.initState();
    _cell = ref.read(engineStateTickerProvider).clip(_track, _slot);
    _cell.addListener(_onCell);
    _cursor.value = 0;
  }

  @override
  void didUpdateWidget(ClipKeyboardSection old) {
    super.didUpdateWidget(old);
    if (old.track != _track || old.slot != _slot) {
      _player.releaseAll();
      _cell.removeListener(_onCell);
      _cell = ref.read(engineStateTickerProvider).clip(_track, _slot);
      _cell.addListener(_onCell);
      _pending = false;
      _liveBefore = null;
      _held.clear();
      _chord.clear();
      _cursor.value = 0;
    }
  }

  @override
  void dispose() {
    _cell.removeListener(_onCell);
    _player.dispose(); // tắt mọi nốt còn giữ
    super.dispose();
  }

  void _noteOn(int note, double velocity) {
    _actions.noteOn(_track, note, velocity);
    if (!_ui.stepMode) return;
    if (_held.isEmpty) _chordAt = _cursor.value;
    _held.add(note);
    _chord[note] = (velocity * 127).round().clamp(1, 127);
  }

  void _noteOff(int note) {
    _actions.noteOff(_track, note);
    if (!_held.remove(note) || _held.isNotEmpty || _chord.isEmpty) return;
    final chord = Map.of(_chord);
    _chord.clear();
    final clip = _ensureMidiClip();
    if (clip == null) return;
    final grid = _ui.grid;
    final at = _chordAt.clamp(0.0, clip.lengthBeats - grid).toDouble();
    _commit(clip, NoteEdit.stepChord(clip.notes, chord, at: at, grid: grid, length: clip.lengthBeats));
    _cursor.value = NoteEdit.stepNext(at, grid: grid, length: clip.lengthBeats);
  }

  void _rest() {
    final c = _clip;
    final len = c is MidiClip ? c.lengthBeats : (_defaultBars() * _beatsPerBar()).toDouble();
    _cursor.value = NoteEdit.stepNext(_cursor.value, grid: _ui.grid, length: len);
  }

  void _back() {
    final clip = _clip;
    if (clip is! MidiClip) return;
    final grid = _ui.grid;
    final at = NoteEdit.stepPrev(_cursor.value, grid: grid, length: clip.lengthBeats);
    final next = NoteEdit.clearCell(clip.notes, at: at, grid: grid);
    if (next.length != clip.notes.length) _commit(clip, next);
    _cursor.value = at;
  }

  void _commit(MidiClip clip, List<Note> notes) {
    ref.read(noteHistoriesProvider).of(clip.id).push(clip.notes, clip.lengthBeats);
    _ctl.setMidiNotes(_track, _slot, notes);
  }

  int _beatsPerBar() => ref.read(projectControllerProvider)?.project.transport.timeSignature.first ?? 4;

  /// Độ dài clip MIDI rỗng tạo khi ghi vào ô trống: theo Settings "Độ dài thu"; Tự do → 2 bar.
  int _defaultBars() {
    final bars = ref.read(settingsProvider).recordBars;
    return bars > 0 ? bars : 2;
  }

  /// Clip MIDI của ô; ô trống thì tạo clip rỗng (07 §4.1d).
  MidiClip? _ensureMidiClip() {
    final c = _clip;
    if (c is MidiClip) return c;
    if (c != null || !_ctl.createMidiClip(_track, _slot, _defaultBars())) return null;
    return _clip as MidiClip?;
  }

  void _toggleLive() {
    final state = _cell.value;
    if (state == ClipState.overdubbing || _pending) {
      if (state == ClipState.overdubbing) _actions.overdubToggle(_track);
      setState(() => _pending = false);
      return;
    }
    final clip = _ensureMidiClip();
    if (clip == null) return;
    _liveBefore = (notes: clip.notes, length: clip.lengthBeats);
    if (state == ClipState.playing) {
      _actions.overdubToggle(_track);
    } else {
      _actions.launchClip(_track, _slot);
      setState(() => _pending = true);
    }
  }

  void _onCell() {
    if (_pending && _cell.value == ClipState.playing) {
      _pending = false;
      _actions.overdubToggle(_track);
      setState(() {});
    }
  }

  @override
  Widget build(BuildContext context) {
    final t = ref.watch(projectControllerProvider.select((s) => s?.project.trackAt(_track)));
    final kitPath = switch (t?.instrument) {
      SfzInstrumentRef(:final path) when path.startsWith('kits/') => path,
      _ => null,
    };
    final kit = kitPath != null;
    final step = ref.watch(sessionUiProvider.select((u) => u.stepMode));
    final color = t == null ? AppColors.tracks[_track % 8] : parseHexColor(t.color);
    // Lượt ghi Live: nốt đổi lần đầu (engine trả sau RECORDING_FINISHED) → ghi trạng thái trước vào ↶.
    ref.listen(projectControllerProvider.select((s) => s?.project.trackAt(_track)?.clipAt(_slot)), (prev, next) {
      final before = _liveBefore;
      if (before == null || next is! MidiClip || prev is! MidiClip || listEquals(prev.notes, next.notes)) return;
      ref.read(noteHistoriesProvider).of(next.id).push(before.notes, before.length);
      _liveBefore = null;
    });
    final Widget surface = kit
        ? Center(
            child: AspectRatio(
              aspectRatio: 1,
              child: NoteSurface(
                key: const Key('clip.pads'),
                player: _player,
                noteAt: const PadLayout().noteAt,
                velocityAt: (_, _) => 0.8, // 06 §4: pad gõ velocity cố định
                painter: PadPainter(
                  layout: const PadLayout(),
                  held: _player.held,
                  accent: color,
                  names: ref.watch(padNamesProvider(kitPath)).value ?? const {},
                  font: painterFont(context),
                ),
              ),
            ),
          )
        : NoteSurface(
            key: const Key('clip.keyboard'),
            player: _player,
            noteAt: KeyboardLayout(baseNote: baseKeyboardNote + 12 * _octave).noteAt,
            velocityAt: KeyboardLayout.velocityAt,
            painter: KeyboardPainter(
              layout: KeyboardLayout(baseNote: baseKeyboardNote + 12 * _octave),
              held: _player.held,
              accent: color,
              range: switch (t?.instrument) {
                SfzInstrumentRef(:final path) => ref.watch(instrumentRangeProvider(path)),
                _ => null,
              },
              font: painterFont(context),
            ),
          );
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        SizedBox(
          height: ClipKeyboardSection.headerHeight,
          child: Row(
            children: [
              ValueListenableBuilder<ClipState>(
                valueListenable: _cell,
                builder: (context, state, _) => _LiveButton(
                  recording: state == ClipState.overdubbing,
                  pending: _pending,
                  onPressed: step ? null : _toggleLive,
                ),
              ),
              const SizedBox(width: 8),
              ValueListenableBuilder<ClipState>(
                valueListenable: _cell,
                builder: (context, state, _) => FilterChip(
                  key: const Key('clip.step'),
                  avatar: const Icon(Icons.keyboard_tab, size: 18),
                  label: Text(S.clipStep),
                  selected: step,
                  showCheckmark: false,
                  onSelected: state == ClipState.overdubbing || _pending
                      ? null
                      : (on) {
                          _held.clear();
                          _chord.clear();
                          _cursor.value = 0;
                          ref.read(sessionUiProvider.notifier).setStepMode(on);
                        },
                ),
              ),
              if (step) ...[
                const SizedBox(width: 4),
                TextButton.icon(
                  key: const Key('clip.stepRest'),
                  onPressed: _rest,
                  icon: const Icon(Icons.space_bar, size: 18),
                  label: Text(S.clipNghi),
                ),
                IconButton(
                  key: const Key('clip.stepBack'),
                  tooltip: S.clipLuiVaXoa,
                  onPressed: _back,
                  icon: const Icon(Icons.backspace_outlined, size: 20),
                ),
              ],
              const Spacer(),
              if (!kit) ...[
                IconButton(
                  key: const Key('clip.octDown'),
                  tooltip: S.clipQuangTamXuong,
                  onPressed: () => _shift(-1),
                  icon: const Icon(Icons.remove, size: 20),
                ),
                Text(noteName(baseKeyboardNote + 12 * _octave), style: AppText.numeric),
                IconButton(
                  key: const Key('clip.octUp'),
                  tooltip: S.clipQuangTamLen,
                  onPressed: () => _shift(1),
                  icon: const Icon(Icons.add, size: 20),
                ),
              ],
            ],
          ),
        ),
        Expanded(child: surface),
      ],
    );
  }

  void _shift(int d) {
    if ((_octave + d).abs() > 3) return;
    _player.releaseAll();
    setState(() => _octave += d);
  }
}

/// ● Ghi Live: đỏ đặc khi đang ghi chồng, viền đỏ khi đã launch và chờ clip chạy.
class _LiveButton extends StatelessWidget {
  const _LiveButton({required this.recording, required this.pending, required this.onPressed});

  final bool recording;
  final bool pending;
  final VoidCallback? onPressed;

  @override
  Widget build(BuildContext context) {
    final label = Text(
      recording
          ? S.clipDangGhi
          : pending
          ? S.clipChoClipChay
          : S.clipGhi,
    );
    const icon = Icon(Icons.fiber_manual_record, size: 18);
    return recording
        ? FilledButton.icon(
            key: const Key('clip.liveRecord'),
            style: FilledButton.styleFrom(backgroundColor: AppColors.record, foregroundColor: Colors.white),
            onPressed: onPressed,
            icon: icon,
            label: label,
          )
        : OutlinedButton.icon(
            key: const Key('clip.liveRecord'),
            style: OutlinedButton.styleFrom(
              foregroundColor: AppColors.record,
              side: BorderSide(color: pending ? AppColors.record : AppColors.border),
            ),
            onPressed: onPressed,
            icon: icon,
            label: label,
          );
  }
}
