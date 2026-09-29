import 'dart:async';

import 'package:flutter/widgets.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/l10n/l10n.dart';

/// Mặc định cho mọi test: tiếng Việt (07 §8) — cả bản dịch hiện hành lẫn ngôn ngữ "iOS" giả lập (app thật chọn
/// ngôn ngữ theo đó). Test cần tiếng Anh tự đặt `locale: Locale('en')` / [useLocale].
Future<void> testExecutable(FutureOr<void> Function() testMain) async {
  setUp(() {
    L10n.current = lookupAppLocalizations(const Locale('vi'));
    TestWidgetsFlutterBinding.ensureInitialized().platformDispatcher.localesTestValue = const [Locale('vi')];
  });
  await testMain();
}
