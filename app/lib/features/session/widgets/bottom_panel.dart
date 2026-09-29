import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../app/theme.dart';
import '../../../model/project.dart';
import '../../browser/browser_panel.dart';
import '../../clip/clip_views.dart';
import '../../fx/fx_panel.dart';
import '../../instrument/instrument_panel.dart';
import '../../mixer/mixer_panel.dart';
import '../project_controller.dart';
import '../session_layout.dart';
import '../session_ui.dart';
import '../../../l10n/l10n.dart';

extension on PanelTab {
  String get label => S.panelTab(name);
}

/// Panel ngữ cảnh dưới grid (07 §2): cao 260 pt, thu gọn còn 44 pt.
/// Tab lấy từ [sessionUiProvider] (chọn clip ở Edit tự mở tab Clip). Chỉ panel rebuild khi đổi
/// tab/thu gọn; grid giãn ra bằng layout (P2-06).
class BottomPanel extends ConsumerWidget {
  const BottomPanel({super.key, required this.collapsed});

  final ValueNotifier<bool> collapsed;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final tab = ref.watch(sessionUiProvider.select((u) => u.tab));
    final expanded = ref.watch(sessionUiProvider.select((u) => u.panelExpanded));
    final ui = ref.read(sessionUiProvider.notifier);
    return ValueListenableBuilder<bool>(
      valueListenable: collapsed,
      builder: (context, isCollapsed, _) => SizedBox(
        key: const Key('session.panel'),
        height: isCollapsed
            ? SessionLayout.panelCollapsedHeight
            : expanded
            ? MediaQuery.sizeOf(context).height * SessionLayout.panelExpandedFraction
            : SessionLayout.panelHeight,
        child: ColoredBox(
          color: AppColors.surface,
          child: Column(
            children: [
              SizedBox(
                height: SessionLayout.panelCollapsedHeight,
                child: Row(
                  children: [
                    for (final t in PanelTab.values)
                      _TabButton(
                        key: Key('panel.tab.${t.name}'),
                        label: t.label,
                        selected: t == tab && !isCollapsed,
                        onTap: () {
                          ref.read(sessionUiProvider.notifier).showTab(t);
                          collapsed.value = false;
                        },
                      ),
                    const Spacer(),
                    // ⤢ chỉ ở tab Clip: panel cao ~70% màn hình để vẽ nốt / sửa clip.
                    if (tab == PanelTab.clip && !isCollapsed)
                      IconButton(
                        key: const Key('session.panel.expand'),
                        tooltip: expanded ? S.sessionThuNhoPanel : S.sessionMoRongPanel,
                        icon: Icon(expanded ? Icons.close_fullscreen : Icons.open_in_full),
                        onPressed: () => ui.setPanelExpanded(!expanded),
                      ),
                    IconButton(
                      key: const Key('session.panel.toggle'),
                      tooltip: isCollapsed ? S.sessionMoPanel : S.sessionThuGon,
                      icon: Icon(isCollapsed ? Icons.expand_less : Icons.expand_more),
                      onPressed: () {
                        if (!isCollapsed) ui.setPanelExpanded(false); // thu gọn → bỏ luôn trạng thái mở rộng
                        collapsed.value = !isCollapsed;
                      },
                    ),
                  ],
                ),
              ),
              if (!isCollapsed)
                Expanded(
                  child: switch (tab) {
                    PanelTab.mixer => const MixerPanel(),
                    PanelTab.instrument => const InstrumentPanel(),
                    PanelTab.clip => const ClipPanel(),
                    PanelTab.fx => const FxPanel(),
                    PanelTab.browser => const BrowserPanel(),
                  },
                ),
            ],
          ),
        ),
      ),
    );
  }
}

class _TabButton extends StatelessWidget {
  const _TabButton({super.key, required this.label, required this.selected, required this.onTap});

  final String label;
  final bool selected;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTap: onTap,
      behavior: HitTestBehavior.opaque,
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 16),
        alignment: Alignment.center,
        decoration: BoxDecoration(
          border: Border(bottom: BorderSide(color: selected ? AppColors.play : Colors.transparent, width: 2)),
        ),
        child: Text(label, style: TextStyle(color: selected ? AppColors.textPrimary : AppColors.textSecondary)),
      ),
    );
  }
}

/// Tab Clip: clip đang chọn ở Edit — MIDI (piano roll, P2-19) hoặc audio (waveform + loop, P2-20/21).
class ClipPanel extends ConsumerWidget {
  const ClipPanel({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final sel = ref.watch(sessionUiProvider.select((u) => u.selected));
    final clip = sel == null
        ? null
        : ref.watch(projectControllerProvider.select((s) => s?.project.trackAt(sel.track)?.clipAt(sel.slot)));
    final small = Theme.of(context).textTheme.bodySmall;
    if (sel == null) {
      return Center(child: Text(S.sessionBatEditRoiChamMot, style: small));
    }
    if (clip == null) {
      return Center(child: Text(S.sessionOTrong(sel.track + 1, sel.slot + 1), style: small));
    }
    return Padding(
      key: const Key('clipPanel'),
      padding: const EdgeInsets.fromLTRB(12, 8, 12, 8),
      child: switch (clip) {
        MidiClip() => MidiClipView(key: ValueKey(clip.id), track: sel.track, clip: clip),
        AudioClip() => AudioClipView(key: ValueKey(clip.id), track: sel.track, clip: clip),
      },
    );
  }
}
