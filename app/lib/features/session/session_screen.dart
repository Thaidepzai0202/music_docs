import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app/theme.dart';
import '../../engine/engine_providers.dart';
import '../../engine/engine_state_ticker.dart';
import 'project_controller.dart';

/// Khung màn Session. Grid, track header, transport làm từ P2-06 (cần engine P1 để kiểm thử).
/// Đã bọc [EngineTickerScope]: Ticker state 60Hz chỉ chạy khi màn này hiển thị (P2-02).
class SessionScreen extends ConsumerWidget {
  const SessionScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final session = ref.watch(projectControllerProvider);
    return EngineTickerScope(
      ticker: ref.watch(engineStateTickerProvider),
      child: Scaffold(
        appBar: AppBar(title: Text(session?.project.name ?? 'Session')),
        body: Center(
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              if (session == null) const Text('Chưa mở project'),
              if (session?.progress case final p?) Text('Đang mở project… ${p.done}/${p.total}'),
              for (final e in session?.errors ?? const <String>[])
                Text(e, style: const TextStyle(color: AppColors.record)),
            ],
          ),
        ),
      ),
    );
  }
}
