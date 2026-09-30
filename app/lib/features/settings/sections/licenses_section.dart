import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../../../app/app_info.dart';
import '../../../app/theme.dart';
import '../../../data/license_credits.dart';
import '../settings_screen.dart';
import '../../../l10n/l10n.dart';

/// Một thư viện bên thứ ba build vào app (engine/third_party/VERSIONS.md).
@immutable
final class ThirdPartyLicense {
  const ThirdPartyLicense(this.name, this.version, this.license, this.asset);

  final String name;
  final String version;
  final String license;

  /// File trong `assets/licenses/third_party/` (app/tool/sync_licenses.sh).
  final String asset;
}

/// Thư viện C++ vào app. Catch2 chỉ dùng cho test engine → không liệt kê. (Getter: nhãn theo ngôn ngữ hiện hành.)
List<ThirdPartyLicense> get thirdPartyLicenses => [
  ThirdPartyLicense('JUCE', '9.0.3', S.licensesJuce9DungTheoGoi, 'assets/licenses/third_party/juce.txt'),
  ThirdPartyLicense('Signalsmith Stretch', '1.4.0', 'MIT', 'assets/licenses/third_party/signalsmith-stretch.txt'),
  ThirdPartyLicense('Signalsmith Linear', '0.6.4', 'MIT', 'assets/licenses/third_party/signalsmith-linear.txt'),
  ThirdPartyLicense('SPSCQueue (Erik Rigtorp)', '1.1+', 'MIT', 'assets/licenses/third_party/spscqueue.txt'),
  ThirdPartyLicense(
    'Ableton Link (LinkKit)',
    '—',
    S.licensesChuaTichHopP406,
    'assets/licenses/third_party/linkkit.txt',
  ),
];

/// Settings → Giấy phép (P4-22): ghi công (P2-35) + nội dung âm thanh (content/LICENSES) + thư viện bên thứ ba + gói
/// Flutter/Dart.
/// File có trong asset thì mở được; chưa có (LinkKit chưa vendored) thì hiện mờ.
class LicensesSection extends StatefulWidget {
  const LicensesSection({super.key, this.bundle});

  /// Test thay được (mặc định `rootBundle`).
  final AssetBundle? bundle;

  @override
  State<LicensesSection> createState() => _LicensesSectionState();
}

class _LicensesSectionState extends State<LicensesSection> {
  AssetBundle get _bundle => widget.bundle ?? rootBundle;
  late final Future<List<String>> _assets = _listAssets();

  /// Ghi công theo file giấy phép nội dung (chỉ file có mục "## Ghi công").
  late final Future<Map<String, String>> _credits = _loadCredits();

  Future<List<String>> _listAssets() async {
    final m = await AssetManifest.loadFromAssetBundle(_bundle);
    return m.listAssets().where((a) => a.startsWith('assets/licenses/')).toList()..sort();
  }

  Future<Map<String, String>> _loadCredits() async {
    final out = <String, String>{};
    for (final a in (await _assets).where((a) => a.startsWith('assets/licenses/content/'))) {
      final credit = licenseCredit(await _bundle.loadString(a));
      if (credit != null) out[a] = credit;
    }
    return out;
  }

  void _open(String title, String asset) => Navigator.push(
    context,
    MaterialPageRoute<void>(
      builder: (_) => LicenseTextPage(title: title, asset: asset, bundle: _bundle),
    ),
  );

  @override
  Widget build(BuildContext context) {
    return FutureBuilder<List<String>>(
      future: _assets,
      builder: (context, snap) {
        final assets = snap.data ?? const <String>[];
        final content = assets.where((a) => a.startsWith('assets/licenses/content/')).toList();
        return SettingsPage(
          children: [
            // Ghi công hiện thẳng câu chữ (không phải mở file): Salamander CC BY 3.0 bắt buộc, VSCO-2 CC0 được đề nghị.
            FutureBuilder<Map<String, String>>(
              future: _credits,
              builder: (context, credits) => (credits.data ?? const {}).isEmpty
                  ? const SizedBox.shrink()
                  : SettingsGroup(
                      title: S.licensesGhiCong,
                      children: [
                        for (final e in credits.data!.entries)
                          ListTile(
                            key: Key('licenses.credit.${e.key.split('/').last}'),
                            contentPadding: EdgeInsets.zero,
                            title: SelectableText(e.value, style: const TextStyle(fontSize: 13, height: 1.35)),
                            subtitle: Text(e.key.split('/').last),
                            trailing: const Icon(Icons.chevron_right),
                            onTap: () => _open(e.key.split('/').last, e.key),
                          ),
                      ],
                    ),
            ),
            SettingsGroup(
              title: S.licensesNoiDungAmThanh,
              children: [
                if (snap.hasData && content.isEmpty)
                  Padding(
                    padding: EdgeInsets.symmetric(vertical: 12),
                    child: Text(S.licensesChuaCoFileGiayPhep, style: TextStyle(color: AppColors.textSecondary)),
                  ),
                for (final a in content)
                  ListTile(
                    key: Key('licenses.content.${a.split('/').last}'),
                    contentPadding: EdgeInsets.zero,
                    title: Text(a.split('/').last),
                    trailing: const Icon(Icons.chevron_right),
                    onTap: () => _open(a.split('/').last, a),
                  ),
              ],
            ),
            SettingsGroup(
              title: S.licensesThuVienBenThuBa,
              children: [
                for (final l in thirdPartyLicenses)
                  ListTile(
                    key: Key('licenses.lib.${l.name}'),
                    contentPadding: EdgeInsets.zero,
                    enabled: assets.contains(l.asset),
                    title: Text(l.version == '—' ? l.name : '${l.name} ${l.version}'),
                    subtitle: Text(l.license),
                    trailing: const Icon(Icons.chevron_right),
                    onTap: () => _open(l.name, l.asset),
                  ),
              ],
            ),
            SettingsGroup(
              title: S.licensesGoiFlutterDart,
              children: [
                ListTile(
                  key: const Key('licenses.flutter'),
                  contentPadding: EdgeInsets.zero,
                  title: Text(S.licensesGiayPhepCacGoiFlutter),
                  trailing: const Icon(Icons.chevron_right),
                  onTap: () => showLicensePage(
                    context: context,
                    applicationName: AppInfo.name,
                    applicationVersion: AppInfo.version,
                  ),
                ),
              ],
            ),
          ],
        );
      },
    );
  }
}

/// Toàn văn một file giấy phép (chọn/copy được).
class LicenseTextPage extends StatelessWidget {
  const LicenseTextPage({super.key, required this.title, required this.asset, required this.bundle});

  final String title;
  final String asset;
  final AssetBundle bundle;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: AppColors.background,
      appBar: AppBar(backgroundColor: AppColors.surface, title: Text(title)),
      body: FutureBuilder<String>(
        future: bundle.loadString(asset),
        builder: (context, snap) => snap.hasData
            ? SingleChildScrollView(
                key: const Key('licenses.text'),
                padding: const EdgeInsets.all(24),
                child: SelectableText(snap.data!, style: const TextStyle(fontSize: 13, height: 1.4)),
              )
            : Center(child: snap.hasError ? Text(S.licensesKhongDocDuoc(asset)) : const CircularProgressIndicator()),
      ),
    );
  }
}
