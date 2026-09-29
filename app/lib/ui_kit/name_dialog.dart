import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../l10n/l10n.dart';
import '../model/names.dart';

/// Hỏi tên (project, clip…). Trả null nếu huỷ hoặc để trống. Ký tự điều khiển (NUL…) bị chặn khi gõ/dán và bị lọc
/// lần nữa lúc xác nhận; chỉ toàn ký tự bị lọc = để trống → nơi gọi giữ tên cũ.
Future<String?> askName(BuildContext context, {required String title, required String initial}) => showDialog<String>(
  context: context,
  builder: (_) => NameDialog(title: title, initial: initial),
);

/// Giữ TextEditingController trong State riêng để nó chỉ bị huỷ sau khi dialog đóng hẳn
/// (animation đóng vẫn còn dùng TextField).
class NameDialog extends StatefulWidget {
  const NameDialog({super.key, required this.title, required this.initial});

  final String title;
  final String initial;

  @override
  State<NameDialog> createState() => _NameDialogState();
}

class _NameDialogState extends State<NameDialog> {
  late final _c = TextEditingController(text: widget.initial);

  @override
  void dispose() {
    _c.dispose();
    super.dispose();
  }

  void _submit() {
    final name = cleanName(_c.text);
    Navigator.pop(context, name.isEmpty ? null : name);
  }

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: Text(widget.title),
      content: TextField(
        key: const Key('nameDialog.field'),
        controller: _c,
        autofocus: true,
        inputFormatters: [FilteringTextInputFormatter.deny(controlChars)],
        onSubmitted: (_) => _submit(),
      ),
      actions: [
        TextButton(onPressed: () => Navigator.pop(context), child: Text(S.exportHuy)),
        FilledButton(key: const Key('nameDialog.ok'), onPressed: _submit, child: Text(S.commonOk)),
      ],
    );
  }
}
