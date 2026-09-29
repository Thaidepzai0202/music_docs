import 'dart:async';

import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'engine_providers.dart';
import '../l10n/l10n.dart';

enum BannerLevel { info, warning, error }

@immutable
final class BannerMessage {
  const BannerMessage({required this.id, required this.text, required this.level, this.key});
  final int id;
  final String text;
  final BannerLevel level;

  /// Thông báo cùng [key] thay thế nhau (ví dụ "bluetooth"), và tự gỡ khi hết điều kiện.
  final String? key;
}

/// Gom lỗi và cảnh báo engine thành banner không chặn thao tác (P2-13, 07 §4.3):
/// - LE_EVT_ERROR → lỗi (đóng tay).
/// - LE_EVT_AUDIO_INTERRUPTED → "Audio bị ngắt" cho tới khi hết ngắt.
/// - LE_EVT_ROUTE_CHANGED Bluetooth → cảnh báo độ trễ cho tới khi đổi route (P2-25).
/// Không dùng timer tự ẩn: banner ở lại tới khi người dùng đóng hoặc hết điều kiện.
class EngineErrorBus extends ChangeNotifier {
  EngineErrorBus(Stream<EngineEvent> events) {
    _sub = events.listen(_onEvent);
  }

  late final StreamSubscription<EngineEvent> _sub;
  final _items = <BannerMessage>[];
  int _nextId = 1;
  static const maxItems = 5;

  List<BannerMessage> get messages => List.unmodifiable(_items);

  void post(String text, {BannerLevel level = BannerLevel.error, String? key}) {
    if (key != null) _items.removeWhere((m) => m.key == key);
    _items.add(BannerMessage(id: _nextId++, text: text, level: level, key: key));
    if (_items.length > maxItems) _items.removeAt(0);
    notifyListeners();
  }

  void dismiss(int id) {
    _items.removeWhere((m) => m.id == id);
    notifyListeners();
  }

  void clearKey(String key) {
    final before = _items.length;
    _items.removeWhere((m) => m.key == key);
    if (_items.length != before) notifyListeners();
  }

  void _onEvent(EngineEvent e) {
    switch (e) {
      case EngineErrorEvent(errorCode: LeError.LE_ERR_OVERDUB_UNSUPPORTED, :final track, :final slot):
        // Clip audio Re-Pitch khác tempo / chưa sẵn sàng: ô vẫn phát, chỉ không overdub được.
        post(S.bannerKhongOverdubDuoc(track + 1, slot + 1), level: BannerLevel.warning, key: 'overdub');
      case EngineErrorEvent(:final errorCode):
        post(S.bannerLoiEngine(S.errorText(leErrorName(errorCode))));
      case AudioInterrupted(:final began):
        began ? post(S.bannerAudioBiNgatCuocGoi, level: BannerLevel.warning, key: 'interrupt') : clearKey('interrupt');
      case RouteChanged(:final bluetooth):
        bluetooth
            ? post(S.bannerDangDungTaiNgheBluetooth, level: BannerLevel.warning, key: 'bluetooth')
            : clearKey('bluetooth');
      case MemoryWarning(:final megabytes, :final critical):
        // Engine đã tự giải phóng cache (LE_EVT_MEMORY_WARNING): chỉ báo nhẹ, không chặn thao tác. Critical → vàng.
        post(
          S.bannerDaGiaiPhongBoNho(megabytes.toStringAsFixed(0)),
          level: critical ? BannerLevel.warning : BannerLevel.info,
          key: 'memory',
        );
      default:
        break;
    }
  }

  @override
  void dispose() {
    _sub.cancel();
    super.dispose();
  }
}

final engineErrorBusProvider = Provider<EngineErrorBus>((ref) {
  final bus = EngineErrorBus(ref.watch(engineProvider).events);
  ref.onDispose(bus.dispose);
  return bus;
});
