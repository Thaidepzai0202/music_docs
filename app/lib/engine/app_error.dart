import 'package:flutter/foundation.dart';

/// Lỗi hiện cho người dùng theo MÃ (07 §8): `code` là tên `LeError` (vd `DISK_FULL`) hoặc mã của app (vd
/// `CELL_EMPTY`); UI dịch bằng `S.errorText(code)`. `op` chỉ để log/test — không hiện, cũng như `message` của engine.
@immutable
final class AppError {
  const AppError(this.code, {this.op = ''});

  final String code;
  final String op;

  @override
  bool operator ==(Object other) => other is AppError && other.code == code && other.op == op;

  @override
  int get hashCode => Object.hash(code, op);

  @override
  String toString() => op.isEmpty ? code : '$op: $code';
}
