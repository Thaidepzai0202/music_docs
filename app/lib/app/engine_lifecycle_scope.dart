import 'package:flutter/widgets.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../engine/engine_providers.dart';

/// Vòng đời engine theo `AppLifecycleState` (P2-03):
/// - `paused` (xuống nền): transport không chạy và không thu → `le_audio_stop`; đang phát/thu → giữ.
/// - `resumed`: bật lại nếu chính lớp này đã tắt.
/// Autosave khi xuống nền (06 §5) sẽ móc vào đây ở P2-25.
class EngineLifecycleScope extends ConsumerStatefulWidget {
  const EngineLifecycleScope({super.key, required this.child});

  final Widget child;

  @override
  ConsumerState<EngineLifecycleScope> createState() => _EngineLifecycleScopeState();
}

class _EngineLifecycleScopeState extends ConsumerState<EngineLifecycleScope> {
  late final AppLifecycleListener _listener = AppLifecycleListener(onStateChange: _onStateChange);

  @override
  void initState() {
    super.initState();
    _listener; // khởi tạo listener ngay
  }

  void _onStateChange(AppLifecycleState s) {
    final audio = ref.read(engineAudioProvider);
    switch (s) {
      case AppLifecycleState.paused:
        // Đọc thẳng từ engine: Ticker có thể đang tắt nếu không ở màn Session.
        final st = ref.read(engineProvider).readState();
        audio.onBackground(engineBusy: st.playing != 0 || st.anyRecording != 0);
      case AppLifecycleState.resumed:
        audio.onForeground();
      default:
        break;
    }
  }

  @override
  void dispose() {
    _listener.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => widget.child;
}
