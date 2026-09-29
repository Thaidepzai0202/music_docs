import 'dart:async';

import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../app/theme.dart';
import '../../../model/project.dart';
import '../../../services/midi_service.dart';
import '../../../services/service_providers.dart';
import '../../fx/fx_specs.dart';
import '../../session/project_controller.dart';
import '../settings_screen.dart';
import '../../../l10n/l10n.dart';

/// Settings → MIDI (P4-01/04): thiết bị vào (bật/tắt) và MIDI learn cho project đang mở.
///
/// Target learn theo 05 §3 (khớp `midi::LearnAction`): clip · scene · transport · stopAll · trackGain · trackMute ·
/// fx (track −1 = master). Sau `MIDI_LEARNED` (chỉ có kind + number) gọi `midi.learnResult` để lấy thiết bị + kênh
/// thật. Quy ước mapping: `device: ""` = mọi thiết bị, `channel: -1` = mọi kênh; channel 0 là kênh 1.
class MidiSection extends ConsumerStatefulWidget {
  const MidiSection({super.key});

  @override
  ConsumerState<MidiSection> createState() => _MidiSectionState();
}

class _MidiSectionState extends ConsumerState<MidiSection> {
  late final MidiService _midi = ref.read(midiServiceProvider);
  StreamSubscription<void>? _sub;
  List<MidiDevice> _inputs = const [];
  List<MidiDevice> _outputs = const [];
  String? _error;

  @override
  void initState() {
    super.initState();
    _load();
    _sub = _midi.devicesChanged.listen((_) => setState(_load));
  }

  @override
  void dispose() {
    _sub?.cancel();
    super.dispose();
  }

  void _load() {
    try {
      final d = _midi.listDevices();
      _inputs = d.inputs;
      _outputs = d.outputs;
      _error = null;
    } on EngineCallException catch (e) {
      _error = e.code == 'NOT_IMPLEMENTED' ? S.midiEngineChuaHoTroMidi : S.errorText(e.code);
    }
  }

  void _enable(String id, bool enabled) {
    String? err;
    try {
      _midi.enableDevice(id, enabled);
    } on EngineCallException catch (e) {
      err = e.code;
    }
    setState(_load);
    if (err != null) {
      ScaffoldMessenger.maybeOf(context)?.showSnackBar(SnackBar(content: Text(S.midiEnableFailed(S.errorText(err)))));
    }
  }

  /// Màn ghép Bluetooth MIDI của iOS; đóng xong thì làm mới danh sách (thiết bị mới ghép hiện ngay).
  Future<void> _pairBluetooth() async {
    await _midi.pairBluetooth();
    if (mounted) setState(_load);
  }

  @override
  Widget build(BuildContext context) {
    final project = ref.watch(projectControllerProvider.select((s) => s?.project));
    return SettingsPage(
      children: [
        SettingsGroup(
          title: S.midiThietBiVao,
          children: [
            if (_error != null)
              Padding(
                padding: const EdgeInsets.symmetric(vertical: 12),
                child: Text(
                  _error!,
                  key: const Key('midi.error'),
                  style: const TextStyle(color: AppColors.queued),
                ),
              )
            else if (_inputs.isEmpty)
              Padding(
                padding: EdgeInsets.symmetric(vertical: 12),
                child: Text(S.midiChuaThayThietBiMidi, style: TextStyle(color: AppColors.textSecondary)),
              ),
            for (final d in _inputs)
              SettingsRow(
                label: d.name,
                subtitle: d.id,
                child: Switch(key: Key('midi.device.${d.id}'), value: d.enabled, onChanged: (v) => _enable(d.id, v)),
              ),
            Row(
              mainAxisAlignment: MainAxisAlignment.end,
              children: [
                TextButton.icon(
                  key: const Key('midi.bluetooth'),
                  onPressed: _pairBluetooth,
                  icon: const Icon(Icons.bluetooth, size: 18),
                  label: Text(S.midiGhepThietBiBluetoothMidi),
                ),
                TextButton.icon(
                  key: const Key('midi.refresh'),
                  onPressed: () => setState(_load),
                  icon: const Icon(Icons.refresh, size: 18),
                  label: Text(S.midiLamMoi),
                ),
              ],
            ),
          ],
        ),
        if (_outputs.isNotEmpty)
          SettingsGroup(
            title: S.midiThietBiRa,
            note: S.midiDenLedPhanHoiTrang,
            children: [for (final d in _outputs) SettingsRow(label: d.name, child: const Icon(Icons.output, size: 18))],
          ),
        SettingsGroup(
          title: S.midiMidiLearnProjectDangMo,
          note: S.midiGanMotNutNumTren,
          children: [
            if (project == null)
              NeedsProject(message: S.settingsNeedsProjectMidi)
            else ...[
              if (project.midiMappings.isEmpty)
                Padding(
                  padding: EdgeInsets.symmetric(vertical: 12),
                  child: Text(S.midiChuaGanGi, style: TextStyle(color: AppColors.textSecondary)),
                ),
              for (var i = 0; i < project.midiMappings.length; i++)
                SettingsRow(
                  label: targetLabel(project, project.midiMappings[i].target),
                  subtitle: sourceLabel(project.midiMappings[i].src),
                  child: IconButton(
                    key: Key('midi.mapping.delete.$i'),
                    tooltip: S.midiBoGan,
                    icon: const Icon(Icons.delete_outline),
                    onPressed: () => ref
                        .read(projectControllerProvider.notifier)
                        .setMidiMappings([...project.midiMappings]..removeAt(i)),
                  ),
                ),
              Align(
                alignment: Alignment.centerRight,
                child: TextButton.icon(
                  key: const Key('midi.learn.add'),
                  onPressed: () => _learn(project),
                  icon: const Icon(Icons.add, size: 18),
                  label: Text(S.midiGanMoi),
                ),
              ),
            ],
          ],
        ),
      ],
    );
  }

  Future<void> _learn(Project project) async {
    final m = await showDialog<MidiMapping>(
      context: context,
      builder: (_) => _LearnDialog(project: project),
    );
    if (m == null || !mounted) return;
    // Một nút controller chỉ điều khiển một thứ: gán mới thay gán cũ cùng nguồn (thiết bị, loại, kênh, số).
    final rest = [
      for (final x in project.midiMappings)
        if (x.src != m.src) x,
    ];
    ref.read(projectControllerProvider.notifier).setMidiMappings([...rest, m]);
  }
}

String sourceLabel(MidiSource s) =>
    '${s.kind == 'cc' ? S.midiSourceCc(s.number) : S.midiSourceNote(s.number)} · '
    '${s.device.isEmpty ? S.midiAnyDevice : s.device} · '
    '${s.channel < 0 ? S.midiAnyChannel : S.midiChannel(s.channel + 1)}';

/// Loại target learn (05 §3) → nhãn trên UI.
Map<String, String> get learnKinds => {
  for (final k in const [
    'clip',
    'scene',
    'transport',
    'stopAll',
    'loopButton',
    'trackStop',
    'undoOverdub',
    'trackGain',
    'trackMute',
    'fx',
  ])
    k: S.learnKind(k),
};

Map<String, String> get transportActions => {
  for (final a in const ['play', 'stop', 'toggle']) a: S.transportAction(a),
};

/// Tham số FX gán được: master slot 0 = EQ3, slot 1 = Limiter (chỉ trần); track: theo loại FX ở slot.
List<ParamSpec> learnParams(Project p, int track, int slot) {
  if (track == -1) return slot == 0 ? fxSpecs[FxType.eq3]! : const [limiterCeilingSpec];
  final fx = p.trackAt(track)?.fx ?? const <FxSlot>[];
  return slot < fx.length ? fxSpecs[fx[slot].type]! : const [];
}

String targetLabel(Project p, Map<String, dynamic> t) {
  String trackName(int i) => p.trackAt(i)?.name ?? S.sessionTrackName(i + 1);
  final track = t['track'] as int? ?? 0;
  final slot = t['slot'] as int? ?? 0;
  switch (t['kind']) {
    case 'clip':
      return S.midiClipO(trackName(track), slot + 1);
    case 'scene':
      final name =
          p.scenes.where((s) => s.index == slot).map((s) => s.name).firstOrNull ?? S.sessionSceneName(slot + 1);
      return 'Scene · $name';
    case 'transport':
      return 'Transport · ${transportActions[t['action']] ?? t['action']}';
    case 'stopAll':
      return S.midiDungTatCa;
    case 'loopButton' || 'trackStop' || 'undoOverdub':
      return S.learnKind(t['kind'] as String);
    case 'trackGain':
      return 'Gain · ${trackName(track)}';
    case 'trackMute':
      return 'Mute · ${trackName(track)}';
    case 'fx':
      final param = learnParams(p, track, slot).where((s) => s.id == t['param']).map((s) => s.label).firstOrNull;
      if (track == -1) {
        return 'FX · ${S.mixerMaster} · ${slot == 0 ? 'EQ3' : 'Limiter'} · ${param ?? S.midiParamFallback(t['param'] ?? '?')}';
      }
      final fx = p.trackAt(track)?.fx ?? const <FxSlot>[];
      final type = slot < fx.length ? ' ${fxTypeLabel(fx[slot].type)}' : '';
      return 'FX · ${trackName(track)} · ${S.midiSlot(slot + 1)}$type · ${param ?? S.midiParamFallback(t['param'] ?? '?')}';
    default:
      return '${t['kind']}';
  }
}

/// Chọn target → `midi.learnStart` → chờ `MIDI_LEARNED` → `midi.learnResult` → trả [MidiMapping].
/// Huỷ/đóng → `midi.learnCancel`.
class _LearnDialog extends ConsumerStatefulWidget {
  const _LearnDialog({required this.project});

  final Project project;

  @override
  ConsumerState<_LearnDialog> createState() => _LearnDialogState();
}

class _LearnDialogState extends ConsumerState<_LearnDialog> {
  late final MidiService _midi = ref.read(midiServiceProvider);
  StreamSubscription<MidiLearned>? _sub;
  String _kind = 'clip';
  int _track = 0; // −1 = master (chỉ với fx)
  int _slot = 0;
  int _param = 0;
  String _action = 'toggle';
  bool _waiting = false;
  String? _error;

  Project get _p => widget.project;
  List<FxSlot> get _fx => _p.trackAt(_track)?.fx ?? const [];
  List<ParamSpec> get _params => learnParams(_p, _track, _slot);

  Map<String, dynamic> get _target => switch (_kind) {
    'clip' => {'kind': 'clip', 'track': _track, 'slot': _slot},
    'scene' => {'kind': 'scene', 'slot': _slot},
    'transport' => {'kind': 'transport', 'action': _action},
    'stopAll' => {'kind': 'stopAll'},
    // Footswitch: tác động lên track đang chọn (05 §3).
    'loopButton' || 'trackStop' || 'undoOverdub' => {'kind': _kind},
    'trackGain' => {'kind': 'trackGain', 'track': _track},
    'trackMute' => {'kind': 'trackMute', 'track': _track},
    _ => {'kind': 'fx', 'track': _track, 'slot': _slot, 'param': _param},
  };

  bool get _valid => _kind != 'fx' || _params.any((s) => s.id == _param);

  @override
  void dispose() {
    _sub?.cancel();
    if (_waiting) _midi.learnCancel();
    super.dispose();
  }

  void _start() {
    try {
      _midi.learnStart(_target);
    } on EngineCallException catch (e) {
      setState(() => _error = S.midiLearnFailed(S.errorText(e.code)));
      return;
    }
    final target = _target;
    _sub = _midi.learned.listen((e) {
      _waiting = false;
      if (!mounted) return;
      // Event chỉ có kind + number → hỏi nguồn đầy đủ (engine không trả được → mọi thiết bị/kênh).
      Navigator.pop(context, MidiMapping(src: _midi.learnResult(e), target: target));
    });
    setState(() {
      _waiting = true;
      _error = null;
    });
  }

  void _setKind(String k) => setState(() {
    _kind = k;
    if (k != 'fx' && _track < 0) _track = 0;
    _slot = 0;
    _param = 0;
  });

  Widget _dropdown<T>(String key, T value, Map<T, String> items, ValueChanged<T> onChanged) => DropdownButton<T>(
    key: Key(key),
    isExpanded: true,
    value: value,
    items: [
      for (final e in items.entries)
        DropdownMenuItem(
          value: e.key,
          child: Text(e.value, maxLines: 1, overflow: TextOverflow.ellipsis),
        ),
    ],
    onChanged: (v) {
      if (v != null) onChanged(v);
    },
  );

  List<Widget> _fields() {
    final tracks = {
      if (_kind == 'fx') -1: S.mixerMaster,
      for (var i = 0; i < 8; i++) i: '${i + 1}. ${_p.trackAt(i)?.name ?? S.sessionTrackName(i + 1)}',
    };
    Widget trackPicker() => _dropdown<int>('midi.learn.track', _track, tracks, (v) {
      setState(() {
        _track = v;
        _slot = 0;
        _param = 0;
      });
    });
    switch (_kind) {
      case 'clip':
        return [
          trackPicker(),
          _dropdown<int>('midi.learn.slot', _slot, {for (var i = 0; i < 8; i++) i: S.midiO(i + 1)}, (v) {
            setState(() => _slot = v);
          }),
        ];
      case 'scene':
        return [
          _dropdown<int>('midi.learn.scene', _slot, {
            for (var i = 0; i < 8; i++)
              i: '${i + 1}. ${_p.scenes.where((s) => s.index == i).map((s) => s.name).firstOrNull ?? S.sessionSceneName(i + 1)}',
          }, (v) => setState(() => _slot = v)),
        ];
      case 'transport':
        return [_dropdown<String>('midi.learn.action', _action, transportActions, (v) => setState(() => _action = v))];
      case 'stopAll':
        return [
          Padding(
            padding: EdgeInsets.only(top: 8),
            child: Text(S.midiDungMoiClipTheoQuantize, style: TextStyle(color: AppColors.textSecondary)),
          ),
        ];
      case 'loopButton' || 'trackStop' || 'undoOverdub':
        return [
          Padding(
            padding: const EdgeInsets.only(top: 8),
            child: Text(S.midiFootswitchGhiChu(_kind), style: const TextStyle(color: AppColors.textSecondary)),
          ),
        ];
      case 'trackGain' || 'trackMute':
        return [trackPicker()];
      default: // fx
        final slots = _track == -1
            ? const {0: 'EQ3', 1: 'Limiter'}
            : {for (var i = 0; i < _fx.length; i++) i: S.midiFxSlotItem(i + 1, fxTypeLabel(_fx[i].type))};
        return [
          trackPicker(),
          if (slots.isEmpty)
            Padding(
              padding: EdgeInsets.only(top: 8),
              child: Text(S.midiTrackNayChuaCoFx, style: TextStyle(color: AppColors.textSecondary)),
            )
          else ...[
            _dropdown<int>('midi.learn.fxSlot', _slot, slots, (v) {
              setState(() {
                _slot = v;
                _param = 0;
              });
            }),
            _dropdown<int>(
              'midi.learn.param',
              _params.any((s) => s.id == _param) ? _param : _params.first.id,
              {for (final s in _params) s.id: s.label},
              (v) => setState(() => _param = v),
            ),
          ],
        ];
    }
  }

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: Text(S.midiGanDieuKhienMidi2),
      content: SizedBox(
        width: 460,
        child: _waiting
            ? Column(
                key: Key('midi.learn.waiting'),
                mainAxisSize: MainAxisSize.min,
                children: [
                  SizedBox(height: 8),
                  RepaintBoundary(child: CircularProgressIndicator()), // quay mỗi frame lúc chờ
                  SizedBox(height: 16),
                  Text(S.midiVanHoacBamMotNut),
                ],
              )
            : Column(
                mainAxisSize: MainAxisSize.min,
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  _dropdown<String>('midi.learn.kind', _kind, learnKinds, _setKind),
                  ..._fields(),
                  if (_error != null)
                    Padding(
                      padding: const EdgeInsets.only(top: 8),
                      child: Text(_error!, style: const TextStyle(color: AppColors.record)),
                    ),
                ],
              ),
      ),
      actions: [
        TextButton(
          key: const Key('midi.learn.cancel'),
          onPressed: () => Navigator.pop(context),
          child: Text(S.exportHuy),
        ),
        if (!_waiting)
          FilledButton(
            key: const Key('midi.learn.start'),
            onPressed: _valid ? _start : null,
            child: Text(S.midiBatDauLearn),
          ),
      ],
    );
  }
}
