import 'package:flutter/widgets.dart';

import 'session_ui.dart';

/// Kích thước màn Session (07 §2), thiết kế cho iPad 8: 1080×810 pt.
abstract final class SessionLayout {
  static const topBarHeight = 56.0;

  /// Header cao đủ cho ô phải: LOOP 64 pt + hàng [■ Dừng track][↶ Hoàn tác] 44 pt (07 §3.1b). Hàng grid vẫn ≥ 44 pt.
  static const headerHeight = 112.0;

  /// Cột phải: nút scene, và ô trên cùng chứa LOOP + 2 nút 44 pt đặt cạnh nhau.
  static const sceneColumnWidth = 96.0;
  static const loopButtonSize = 64.0;
  static const panelHeight = 260.0;

  /// ⤢ tab Clip: panel chiếm ~70% chiều cao màn hình.
  static const panelExpandedFraction = 0.7;
  static const panelCollapsedHeight = 44.0;

  /// Chuẩn chạm tối thiểu của Apple.
  static const minCellHeight = 44.0;
  static const tracks = 8;
  static const scenes = 8;
}

/// "#RRGGBB" → Color (màu track lưu trong project.json).
Color parseHexColor(String hex, {Color fallback = const Color(0xFF9AA0A6)}) {
  final h = hex.startsWith('#') ? hex.substring(1) : hex;
  final v = int.tryParse(h, radix: 16);
  if (v == null || h.length != 6) return fallback;
  return Color(0xFF000000 | v);
}

/// Grid của màn Session (chỉ có một Session tại một thời điểm) — dùng để đổi vị trí ngón tay → ô.
final sessionGridKey = GlobalKey(debugLabel: 'sessionGrid');

/// Ô dưới điểm [global] trên màn hình, null nếu ngoài lưới 8×8 (kể cả cột scene).
CellRef? cellAtGlobal(Offset global) {
  final box = sessionGridKey.currentContext?.findRenderObject() as RenderBox?;
  if (box == null || !box.hasSize) return null;
  final p = box.globalToLocal(global);
  final w = box.size.width - SessionLayout.sceneColumnWidth;
  if (p.dx < 0 || p.dy < 0 || p.dx >= w || p.dy >= box.size.height) return null;
  return CellRef(
    (p.dx / (w / SessionLayout.tracks)).floor(),
    (p.dy / (box.size.height / SessionLayout.scenes)).floor(),
  );
}
