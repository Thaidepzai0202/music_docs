import 'package:engine_ffi/engine_ffi.dart';

import 'engine_state_ticker.dart';

/// Màn hình luôn sáng khi transport đang chạy (07 §3.4, P2-13).
///
/// Nghe [EngineStateTicker] (đã chạy sẵn ở màn Session), chỉ gọi native khi cờ `playing` ĐỔI.
/// Native: `UIApplication.isIdleTimerDisabled` trong plugin engine_ffi — không thêm package.
class KeepScreenOn {
  KeepScreenOn(this._ticker, {Future<void> Function(bool on)? setKeepOn})
    : _set = setKeepOn ?? const EnginePlatform().setKeepScreenOn {
    _ticker.addListener(_onTick);
  }

  final EngineStateTicker _ticker;
  final Future<void> Function(bool on) _set;
  bool _on = false;

  bool get isOn => _on;

  void _onTick() {
    final playing = _ticker.state.playing;
    if (playing == _on) return;
    _on = playing;
    _set(playing);
  }

  void dispose() {
    _ticker.removeListener(_onTick);
    if (_on) _set(false);
    _on = false;
  }
}
