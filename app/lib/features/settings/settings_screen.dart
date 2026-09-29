import 'package:flutter/material.dart';

import '../../app/theme.dart';
import 'sections/about_section.dart';
import 'sections/audio_section.dart';
import 'sections/latency_section.dart';
import 'sections/licenses_section.dart';
import 'sections/link_section.dart';
import 'sections/midi_section.dart';
import '../../l10n/l10n.dart';

/// Mục của màn Settings (07 §1: Audio · Latency · MIDI · Link · Giấy phép · Giới thiệu).
enum SettingsSection {
  audio(Icons.graphic_eq),
  latency(Icons.timer_outlined),
  midi(Icons.piano),
  link(Icons.link),
  licenses(Icons.description_outlined),
  about(Icons.info_outline);

  const SettingsSection(this.icon);
  final IconData icon;

  String get label => S.settingsSection(name);
}

/// Màn Settings đầy đủ (P4-13, P4-12): danh mục bên trái, nội dung bên phải (iPad landscape).
/// Cài đặt app lưu ở `settings.json`; MIDI learn và Link là của project đang mở (06 §2).
class SettingsScreen extends StatefulWidget {
  const SettingsScreen({super.key, this.initial = SettingsSection.audio});

  final SettingsSection initial;

  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  late SettingsSection _section = widget.initial;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: AppColors.background,
      appBar: AppBar(
        backgroundColor: AppColors.surface,
        leading: IconButton(
          key: const Key('settings.back'),
          icon: const Icon(Icons.chevron_left),
          onPressed: () => Navigator.maybePop(context),
        ),
        title: Text(S.projectsCaiDat),
      ),
      body: Row(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          SizedBox(
            width: 220,
            child: Material(
              color: AppColors.surface, // ListTile vẽ nền chọn/ripple lên Material gần nhất
              child: ListView(
                padding: const EdgeInsets.symmetric(vertical: 8),
                children: [
                  for (final s in SettingsSection.values)
                    ListTile(
                      key: Key('settings.nav.${s.name}'),
                      leading: Icon(s.icon, size: 20),
                      title: Text(s.label),
                      selected: s == _section,
                      selectedTileColor: AppColors.background,
                      onTap: () => setState(() => _section = s),
                    ),
                ],
              ),
            ),
          ),
          Expanded(
            child: Align(
              alignment: Alignment.topLeft,
              child: ConstrainedBox(
                constraints: const BoxConstraints(maxWidth: 720),
                child: KeyedSubtree(
                  key: ValueKey(_section),
                  child: switch (_section) {
                    SettingsSection.audio => const AudioSection(),
                    SettingsSection.latency => const LatencySection(),
                    SettingsSection.midi => const MidiSection(),
                    SettingsSection.link => const LinkSection(),
                    SettingsSection.licenses => const LicensesSection(),
                    SettingsSection.about => const AboutSection(),
                  },
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }
}

/// Khung cuộn chung cho mỗi mục.
class SettingsPage extends StatelessWidget {
  const SettingsPage({super.key, required this.children});

  final List<Widget> children;

  @override
  Widget build(BuildContext context) =>
      ListView(padding: const EdgeInsets.fromLTRB(24, 16, 24, 32), children: children);
}

/// Nhóm cài đặt có tiêu đề.
class SettingsGroup extends StatelessWidget {
  const SettingsGroup({super.key, required this.title, required this.children, this.note});

  final String title;
  final String? note;
  final List<Widget> children;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 20),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Text(
            title.toUpperCase(),
            style: const TextStyle(fontSize: 12, letterSpacing: 0.8, color: AppColors.textSecondary),
          ),
          const SizedBox(height: 6),
          Material(
            color: AppColors.surface,
            shape: RoundedRectangleBorder(
              borderRadius: BorderRadius.circular(8),
              side: const BorderSide(color: AppColors.border),
            ),
            child: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
              child: Column(crossAxisAlignment: CrossAxisAlignment.stretch, children: children),
            ),
          ),
          if (note != null) ...[
            const SizedBox(height: 6),
            Text(note!, style: const TextStyle(fontSize: 12, color: AppColors.textSecondary)),
          ],
        ],
      ),
    );
  }
}

/// Một dòng: nhãn (+ mô tả) bên trái, điều khiển bên phải; điều khiển rộng thì xuống dòng dưới.
class SettingsRow extends StatelessWidget {
  const SettingsRow({super.key, required this.label, this.subtitle, required this.child, this.below = false});

  final String label;
  final String? subtitle;
  final Widget child;

  /// Điều khiển nằm dưới nhãn (SegmentedButton nhiều lựa chọn, slider).
  final bool below;

  @override
  Widget build(BuildContext context) {
    final text = Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      mainAxisSize: MainAxisSize.min,
      children: [
        Text(label),
        if (subtitle != null)
          Padding(
            padding: const EdgeInsets.only(top: 2),
            child: Text(subtitle!, style: const TextStyle(fontSize: 12, color: AppColors.textSecondary)),
          ),
      ],
    );
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 10),
      child: below
          ? Column(crossAxisAlignment: CrossAxisAlignment.start, children: [text, const SizedBox(height: 8), child])
          : Row(
              children: [
                Expanded(child: text),
                const SizedBox(width: 16),
                child,
              ],
            ),
    );
  }
}

/// Nội dung khi mục cần project đang mở mà chưa có.
class NeedsProject extends StatelessWidget {
  const NeedsProject({super.key, required this.message});

  /// Câu đầy đủ (không ghép mảnh — dịch đúng ngữ pháp từng ngôn ngữ).
  final String message;

  @override
  Widget build(BuildContext context) => Padding(
    padding: const EdgeInsets.symmetric(vertical: 12),
    child: Text(
      message,
      key: const Key('settings.needsProject'),
      style: const TextStyle(color: AppColors.textSecondary),
    ),
  );
}
