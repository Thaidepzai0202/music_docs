import '../../data/library_repository.dart';
import '../../l10n/l10n.dart';
import '../../model/project.dart';

/// Tên track tự động (07 §4.1c): track không chia loại cố định; gán kit / nhạc cụ thì track mang tên nhạc cụ, trừ khi
/// người dùng đã đặt tên riêng. Không lưu cờ trong project — suy ra từ tên hiện tại.
abstract final class TrackNaming {
  /// Tên đặt sẵn của project demo cũ (trước 30/09) — coi như tên tự động.
  static const _legacyDemoTokens = ['drums', 'bass', 'keys', 'lead'];

  /// [t] chưa có tên riêng: tên là "Track N" (mọi ngôn ngữ), tên đặt sẵn của demo cũ, hoặc tên của nhạc cụ đang gán
  /// (mọi ngôn ngữ).
  static bool isAuto(Track t, {LibraryManifest? library, List<UserInstrument> users = const []}) {
    final n = t.name;
    for (final locale in L10n.supportedLocales) {
      final l = lookupAppLocalizations(locale);
      if (n == l.sessionTrackName(t.index + 1)) return true;
      if (_legacyDemoTokens.any((token) => n == l.demoName(token))) return true;
    }
    return instrumentNames(t.instrument, library: library, users: users).contains(n);
  }

  /// Mọi tên (mọi ngôn ngữ) của nhạc cụ [ref]: kit / nhạc cụ trong thư viện, hoặc nhạc cụ tự thu của project.
  static Set<String> instrumentNames(
    InstrumentRef? ref, {
    LibraryManifest? library,
    List<UserInstrument> users = const [],
  }) => switch (ref) {
    SfzInstrumentRef(:final path) => {...?library?.instrumentAt(path)?.names.values},
    UserInstrumentRef(:final id) => {
      for (final u in users)
        if (u.id == id) u.name,
    },
    null => const {},
  };
}
