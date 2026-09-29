import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app/theme.dart';
import '../../engine/engine_providers.dart';
import '../../engine/performance_actions.dart';
import '../../model/project.dart';
import '../../ui_kit/keyboard_view.dart';
import '../session/project_controller.dart';
import '../session/session_layout.dart';
import '../session/session_ui.dart';
import '../session/widgets/track_picker.dart';
import 'envelope_editor.dart';
import 'note_player.dart';
import 'record_sampler_sheet.dart';
import '../../l10n/l10n.dart';

enum PlaySurface { pads, keyboard }

/// Panel Instrument (P2-16/17): pad 4×4 hoặc bàn phím 2 quãng tám, chơi trên track đang chọn.
/// Một `Listener` cho cả mặt phím: pointer-down → NOTE_ON ngay (đường nóng), không GestureDetector.
class InstrumentPanel extends ConsumerStatefulWidget {
  const InstrumentPanel({super.key});

  @override
  ConsumerState<InstrumentPanel> createState() => _InstrumentPanelState();
}

class _InstrumentPanelState extends ConsumerState<InstrumentPanel> {
  // Lấy 1 lần: dispose() còn phải gửi NOTE_OFF cho nốt đang giữ, lúc đó không dùng ref được nữa.
  late final PerformanceActions _actions = ref.read(performanceActionsProvider);
  late final NotePlayer _player = NotePlayer(
    noteOn: (n, v) => _actions.noteOn(_track, n, v),
    noteOff: (n) => _actions.noteOff(_track, n),
  );
  PlaySurface? _surface; // null = tự chọn theo nhạc cụ của track
  int _octave = 0; // dịch quãng: bàn phím bắt đầu từ C3 (48) + 12·octave
  int _track = 0; // track của các nốt đang giữ (đổi track thì tắt hết nốt trước)

  static const baseKeyboardNote = 48;

  @override
  void initState() {
    super.initState();
    _actions; // lấy trước khi cần
  }

  @override
  void dispose() {
    _player.dispose(); // tắt mọi nốt còn giữ
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final track = ref.watch(sessionUiProvider.select((u) => u.selectedTrack));
    if (track != _track) {
      _player.releaseAll(); // không để nốt treo ở track cũ
      _track = track;
    }
    final t = ref.watch(projectControllerProvider.select((s) => s?.project.trackAt(track)));
    final isKit = t?.instrument is SfzInstrumentRef && (t!.instrument! as SfzInstrumentRef).path.startsWith('kits/');
    final surface = _surface ?? (isKit ? PlaySurface.pads : PlaySurface.keyboard);
    final color = t == null ? AppColors.tracks[track % 8] : parseHexColor(t.color);
    final userId = switch (t?.instrument) {
      UserInstrumentRef(:final id) => id,
      _ => null,
    };
    final Widget surfaceWidget = surface == PlaySurface.pads
        ? Center(
            child: AspectRatio(
              aspectRatio: 1,
              child: _PlaySurface(
                key: const Key('instrument.pads'),
                player: _player,
                noteAt: const PadLayout().noteAt,
                velocityAt: (_, _) => 0.8,
                painter: PadPainter(
                  layout: const PadLayout(),
                  held: _player.held,
                  accent: color,
                  font: painterFont(context),
                ),
              ),
            ),
          )
        : _PlaySurface(
            key: const Key('instrument.keyboard'),
            player: _player,
            noteAt: KeyboardLayout(baseNote: baseKeyboardNote + 12 * _octave).noteAt,
            velocityAt: KeyboardLayout.velocityAt,
            painter: KeyboardPainter(
              layout: KeyboardLayout(baseNote: baseKeyboardNote + 12 * _octave),
              held: _player.held,
              accent: color,
              font: painterFont(context),
            ),
          );

    return Padding(
      padding: const EdgeInsets.fromLTRB(12, 6, 12 + SessionLayout.sceneColumnWidth, 10),
      child: Column(
        children: [
          SizedBox(
            height: 36,
            child: Row(
              children: [
                // Tên track tự co lại (có "…") để hàng không tràn khi tên dài.
                Flexible(
                  child: TrackPicker(key: const Key('instrument.track'), track: track),
                ),
                const SizedBox(width: 12),
                SegmentedButton<PlaySurface>(
                  key: const Key('instrument.surface'),
                  segments: [
                    ButtonSegment(value: PlaySurface.pads, label: Text(S.instrumentPad)),
                    ButtonSegment(value: PlaySurface.keyboard, label: Text(S.instrumentBanPhim)),
                  ],
                  selected: {surface},
                  onSelectionChanged: (s) {
                    _player.releaseAll();
                    setState(() => _surface = s.first);
                  },
                ),
                const SizedBox(width: 12),
                OutlinedButton.icon(
                  key: const Key('instrument.recordNew'),
                  // Chỉ phát (thiếu quyền mic) → khoá (07 §4.0).
                  onPressed: ref.watch(canRecordAudioProvider)
                      ? () {
                          _player.releaseAll();
                          showRecordSamplerSheet(context, track);
                        }
                      : null,
                  icon: const Icon(Icons.mic, size: 18),
                  label: Text(S.instrumentThuAmMoi),
                ),
                const Spacer(),
                if (surface == PlaySurface.keyboard) ...[
                  IconButton(
                    key: const Key('instrument.octaveDown'),
                    icon: const Icon(Icons.remove),
                    onPressed: _octave > -3 ? () => _shift(-1) : null,
                  ),
                  Text(noteName(baseKeyboardNote + 12 * _octave), style: AppText.numeric),
                  IconButton(
                    key: const Key('instrument.octaveUp'),
                    icon: const Icon(Icons.add),
                    onPressed: _octave < 3 ? () => _shift(1) : null,
                  ),
                ],
              ],
            ),
          ),
          const SizedBox(height: 6),
          Expanded(
            child: Row(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                // Nhạc cụ tự thu: Natural/Classic + ADSR bên trái mặt phím (P3-07).
                if (userId != null) ...[
                  SizedBox(
                    width: 250,
                    child: UserInstrumentEditor(instrumentId: userId, color: color),
                  ),
                  const SizedBox(width: 8),
                ],
                Expanded(child: surfaceWidget),
              ],
            ),
          ),
        ],
      ),
    );
  }

  void _shift(int d) {
    _player.releaseAll();
    setState(() => _octave += d);
  }
}

/// Mặt chơi: 1 Listener cho mọi ngón, hit-test bằng hình học (không widget từng phím).
class _PlaySurface extends StatelessWidget {
  const _PlaySurface({
    super.key,
    required this.player,
    required this.noteAt,
    required this.velocityAt,
    required this.painter,
  });

  final NotePlayer player;
  final int? Function(Offset, Size) noteAt;
  final double Function(Offset, Size) velocityAt;
  final CustomPainter painter;

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(
      builder: (context, c) {
        final size = c.biggest;
        return Listener(
          behavior: HitTestBehavior.opaque,
          onPointerDown: (e) =>
              player.down(e.pointer, noteAt(e.localPosition, size), velocityAt(e.localPosition, size)),
          onPointerMove: (e) =>
              player.move(e.pointer, noteAt(e.localPosition, size), velocityAt(e.localPosition, size)),
          onPointerUp: (e) => player.up(e.pointer),
          onPointerCancel: (e) => player.up(e.pointer),
          child: RepaintBoundary(
            child: CustomPaint(size: size, painter: painter),
          ),
        );
      },
    );
  }
}
