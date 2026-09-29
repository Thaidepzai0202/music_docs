import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../app/theme.dart';
import '../../session/project_controller.dart';
import '../../../services/link_service.dart';
import '../../../services/service_providers.dart';
import '../settings_screen.dart';
import '../../../l10n/l10n.dart';

/// Settings → Link (P4-09 bản đơn giản): bật/tắt Ableton Link + đồng bộ start/stop của project đang mở
/// (06 §2 `link`), số thiết bị đang nối. Màn Link chuẩn của Ableton (`ABLLinkSettingsViewController`) ở P4-09.
class LinkSection extends ConsumerStatefulWidget {
  const LinkSection({super.key});

  @override
  ConsumerState<LinkSection> createState() => _LinkSectionState();
}

class _LinkSectionState extends ConsumerState<LinkSection> {
  late final LinkService _link = ref.read(linkServiceProvider);
  StreamSubscription<int>? _sub;
  late int _peers = _link.peers;

  @override
  void initState() {
    super.initState();
    _sub = _link.peersChanged.listen((n) => setState(() => _peers = n));
  }

  @override
  void dispose() {
    _sub?.cancel();
    super.dispose();
  }

  void _set({required bool enabled, required bool startStopSync}) {
    final err = ref.read(projectControllerProvider.notifier).setLink(enabled: enabled, startStopSync: startStopSync);
    if (err != null) {
      final msg = err == 'NOT_IMPLEMENTED' ? S.linkEngineChuaCoLinkP4 : S.errorText(err);
      ScaffoldMessenger.maybeOf(context)?.showSnackBar(SnackBar(content: Text(msg)));
    }
  }

  @override
  Widget build(BuildContext context) {
    final link = ref.watch(projectControllerProvider.select((s) => s?.project.link));
    return SettingsPage(
      children: [
        SettingsGroup(
          title: S.linkTitle,
          note: S.linkDongBoTempoVaNhip,
          children: [
            if (link == null)
              NeedsProject(message: S.settingsNeedsProjectLink)
            else ...[
              SettingsRow(
                label: S.linkBatLink2,
                subtitle: link.enabled ? S.linkDangNoiThietBi(_peers) : null,
                child: Switch(
                  key: const Key('link.enable'),
                  value: link.enabled,
                  onChanged: (v) => _set(enabled: v, startStopSync: link.startStopSync),
                ),
              ),
              const Divider(height: 1),
              SettingsRow(
                label: S.linkDongBoStartStop,
                subtitle: S.linkBamOMotMayThi,
                child: Switch(
                  key: const Key('link.startStop'),
                  value: link.startStopSync,
                  onChanged: link.enabled ? (v) => _set(enabled: true, startStopSync: v) : null,
                ),
              ),
            ],
          ],
        ),
        Text(S.linkCanQuyenMangCucBo, style: TextStyle(fontSize: 12, color: AppColors.textSecondary)),
      ],
    );
  }
}
