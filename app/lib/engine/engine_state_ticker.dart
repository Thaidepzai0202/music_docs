import 'dart:ffi';
import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/scheduler.dart';
import 'package:flutter/widgets.dart';

import 'engine_state.dart';

/// Một `Ticker` duy nhất cho toàn app (07 §5): mỗi frame đọc `le_read_state` vào [state].
///
/// - `notifyListeners()` chỉ khi engine đã publish bản mới → `CustomPainter(repaint: ticker)`
///   vẽ lại, KHÔNG rebuild widget, không `setState`, không Riverpod cho dữ liệu 60Hz (07 §6).
/// - 64 `ValueNotifier<ClipState>`: chỉ báo khi ô đó ĐỔI trạng thái → chỉ đúng 1 ClipCell rebuild.
/// - Chỉ chạy khi có màn cần (Session / Spike) — xem [EngineTickerScope].
class EngineStateTicker extends ChangeNotifier {
  EngineStateTicker(this._engine) {
    _ticker = Ticker(_onTick, debugLabel: 'EngineStateTicker');
  }

  final EngineApi _engine;
  late final Ticker _ticker;
  final EngineState state = EngineState();
  final List<ValueNotifier<ClipState>> _clips = List.generate(
    LE_MAX_TRACKS * LE_MAX_SCENES,
    (_) => ValueNotifier(ClipState.empty),
  );
  bool _hasData = false;
  int _users = 0;

  ValueListenable<ClipState> clip(int track, int scene) => _clips[track * LE_MAX_SCENES + scene];

  bool get isActive => _ticker.isActive;

  /// Ticker có đang bị tạm dừng vì màn bị che (TickerMode = false) không.
  bool get isMuted => _ticker.muted;

  /// Màn cần state gọi khi hiện ra. Đếm tham chiếu: nhiều scope lồng nhau vẫn chỉ 1 Ticker.
  void acquire() {
    if (_users++ == 0) {
      poll(); // có số liệu ngay frame đầu
      _ticker.start();
    }
  }

  void release() {
    assert(_users > 0);
    if (--_users == 0) _ticker.stop();
  }

  set muted(bool value) => _ticker.muted = value;

  void _onTick(Duration _) => poll();

  /// Đọc state 1 lần. Công khai để test gọi không cần Ticker.
  @visibleForTesting
  void poll() {
    final s = _engine.readState();
    if (_hasData && s.publishCounter == state.publishCounter) return; // engine chưa publish bản mới
    _hasData = true;
    state.copyFrom(s);
    for (var t = 0; t < LE_MAX_TRACKS; t++) {
      final row = s.clipState[t];
      for (var c = 0; c < LE_MAX_SCENES; c++) {
        // ValueNotifier tự bỏ qua nếu giá trị không đổi.
        _clips[t * LE_MAX_SCENES + c].value = ClipState.fromLe(row[c]);
      }
    }
    notifyListeners();
  }

  @override
  void dispose() {
    _ticker.dispose();
    for (final c in _clips) {
      c.dispose();
    }
    super.dispose();
  }
}

/// Bọc màn cần state 60Hz. Hiện → Ticker chạy; bị route khác che (TickerMode tắt) → tạm dừng;
/// rời màn → dừng hẳn.
class EngineTickerScope extends StatefulWidget {
  const EngineTickerScope({super.key, required this.ticker, required this.child});

  final EngineStateTicker ticker;
  final Widget child;

  @override
  State<EngineTickerScope> createState() => _EngineTickerScopeState();
}

class _EngineTickerScopeState extends State<EngineTickerScope> {
  @override
  void initState() {
    super.initState();
    widget.ticker.acquire();
  }

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    widget.ticker.muted = !TickerMode.valuesOf(context).enabled;
  }

  @override
  void didUpdateWidget(EngineTickerScope old) {
    super.didUpdateWidget(old);
    if (old.ticker != widget.ticker) {
      old.ticker.release();
      widget.ticker.acquire();
    }
  }

  @override
  void dispose() {
    widget.ticker.release();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => widget.child;
}
