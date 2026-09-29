import 'dart:async';

import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app/router.dart';
import '../../app/theme.dart';
import '../../data/data_providers.dart';
import '../../engine/engine_providers.dart';
import '../projects/demo_project.dart';
import '../settings/app_settings.dart';
import '../../l10n/l10n.dart';

enum OnboardingStep { mic, demo }

/// Onboarding lần đầu (P4-14, 07 §1): vì sao cần micro → xin quyền → tạo project demo → vào Session.
///
/// Xong (hoặc "Bỏ qua") thì ghi `onboardingDone` vào settings.json và thay bằng màn Projects; chọn demo thì
/// Projects mở luôn project demo (route argument = thư mục project).
class OnboardingScreen extends ConsumerStatefulWidget {
  const OnboardingScreen({super.key});

  @override
  ConsumerState<OnboardingScreen> createState() => _OnboardingScreenState();
}

class _OnboardingScreenState extends ConsumerState<OnboardingScreen> {
  OnboardingStep _step = OnboardingStep.mic;
  MicPermission? _mic;
  bool _busy = false;
  String? _error;

  Future<void> _askMic() async {
    setState(() => _busy = true);
    final platform = ref.read(enginePlatformProvider);
    var p = MicPermission.denied;
    try {
      p = await platform.micPermission();
      if (p == MicPermission.undetermined) {
        p = await platform.requestMicPermission() ? MicPermission.granted : MicPermission.denied;
      }
      // Bật audio ngay: có quyền → đủ input; không có → chế độ chỉ phát (07 §4.0), demo vẫn nghe được.
      await ref.read(engineAudioProvider).start();
    } catch (_) {
      // kênh native lỗi (hiếm) → coi như chưa có quyền, vẫn cho đi tiếp
    }
    if (!mounted) return;
    setState(() {
      _busy = false;
      _mic = p;
      if (p == MicPermission.granted) _step = OnboardingStep.demo;
    });
  }

  Future<void> _openDemo() async {
    setState(() {
      _busy = true;
      _error = null;
    });
    try {
      final dir = await createDemoProject(ref.read(projectRepositoryProvider));
      _finish(openDir: dir);
    } catch (e) {
      if (mounted) {
        setState(() {
          _busy = false;
          _error = S.onboardingKhongTaoDuocProjectDemo(e);
        });
      }
    }
  }

  void _finish({String? openDir}) {
    ref.read(settingsProvider.notifier).setOnboardingDone();
    if (!mounted) return;
    unawaited(Navigator.pushReplacementNamed(context, AppRoutes.projects, arguments: openDir));
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: AppColors.background,
      body: SafeArea(
        child: Stack(
          children: [
            Center(
              child: ConstrainedBox(
                constraints: const BoxConstraints(maxWidth: 560),
                child: Padding(
                  padding: const EdgeInsets.all(24),
                  child: switch (_step) {
                    OnboardingStep.mic => _MicStep(
                      busy: _busy,
                      denied: _mic == MicPermission.denied,
                      onAllow: _askMic,
                      onLater: () => setState(() => _step = OnboardingStep.demo),
                      onOpenSettings: () => ref.read(engineAudioProvider).openAppSettings(),
                    ),
                    OnboardingStep.demo => _DemoStep(
                      busy: _busy,
                      micGranted: _mic == MicPermission.granted,
                      error: _error,
                      onDemo: _openDemo,
                      onEmpty: _finish,
                    ),
                  },
                ),
              ),
            ),
            Positioned(
              top: 8,
              right: 8,
              child: TextButton(
                key: const Key('onboarding.skip'),
                onPressed: _busy ? null : _finish,
                child: Text(S.onboardingBoQua),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _MicStep extends StatelessWidget {
  const _MicStep({
    required this.busy,
    required this.denied,
    required this.onAllow,
    required this.onLater,
    required this.onOpenSettings,
  });

  final bool busy;
  final bool denied;
  final VoidCallback onAllow;
  final VoidCallback onLater;
  final VoidCallback onOpenSettings;

  @override
  Widget build(BuildContext context) {
    return Column(
      key: const Key('onboarding.mic'),
      mainAxisSize: MainAxisSize.min,
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        const Icon(Icons.mic, size: 48, color: AppColors.record),
        const SizedBox(height: 16),
        Text(S.onboardingMusicLooperCanMicro, style: TextStyle(fontSize: 26, fontWeight: FontWeight.w600)),
        const SizedBox(height: 16),
        _Bullet(S.onboardingThuGiongHatGuitarThanh),
        _Bullet(S.onboardingBienMotTiengBanThu),
        _Bullet(S.onboardingDoDoTreDeBan),
        const SizedBox(height: 8),
        Text(S.onboardingAmThanhChiLuuTren, style: TextStyle(color: AppColors.textSecondary)),
        const SizedBox(height: 24),
        if (denied) ...[
          Text(S.onboardingMicroDangBiTatBan, style: TextStyle(color: AppColors.queued)),
          const SizedBox(height: 12),
        ],
        Wrap(
          spacing: 12,
          runSpacing: 8,
          children: [
            if (!denied)
              FilledButton.icon(
                key: const Key('onboarding.allowMic'),
                onPressed: busy ? null : onAllow,
                icon: const Icon(Icons.mic, size: 18),
                label: Text(S.onboardingChoPhepMicro),
              )
            else
              FilledButton.icon(
                key: const Key('onboarding.openSettings'),
                onPressed: onOpenSettings,
                icon: const Icon(Icons.settings, size: 18),
                label: Text(S.onboardingMoCaiDat),
              ),
            TextButton(
              key: const Key('onboarding.micLater'),
              onPressed: busy ? null : onLater,
              child: Text(denied ? S.onboardingTiepTuc : S.onboardingDeSau),
            ),
          ],
        ),
      ],
    );
  }
}

class _DemoStep extends StatelessWidget {
  const _DemoStep({
    required this.busy,
    required this.micGranted,
    required this.error,
    required this.onDemo,
    required this.onEmpty,
  });

  final bool busy;
  final bool micGranted;
  final String? error;
  final VoidCallback onDemo;
  final VoidCallback onEmpty;

  @override
  Widget build(BuildContext context) {
    return Column(
      key: const Key('onboarding.demo'),
      mainAxisSize: MainAxisSize.min,
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        const Icon(Icons.grid_view_rounded, size: 48, color: AppColors.play),
        const SizedBox(height: 16),
        Text(S.onboardingThuNgayVoiProjectDemo, style: TextStyle(fontSize: 26, fontWeight: FontWeight.w600)),
        const SizedBox(height: 16),
        _Bullet(S.onboardingChamMotODePhat),
        _Bullet(S.onboardingChamSoSceneBenPhai),
        _Bullet(S.onboardingTabInstrumentPhiaDuoiDe),
        if (!micGranted) ...[
          const SizedBox(height: 8),
          Text(S.onboardingChuaCoQuyenMicroThu, style: TextStyle(color: AppColors.textSecondary)),
        ],
        const SizedBox(height: 24),
        if (busy) const LinearProgressIndicator(key: Key('onboarding.progress')),
        if (error != null) Text(error!, style: const TextStyle(color: AppColors.record)),
        const SizedBox(height: 12),
        Wrap(
          spacing: 12,
          runSpacing: 8,
          children: [
            FilledButton.icon(
              key: const Key('onboarding.openDemo'),
              onPressed: busy ? null : onDemo,
              icon: const Icon(Icons.play_arrow, size: 18),
              label: Text(S.onboardingMoProjectDemo),
            ),
            TextButton(
              key: const Key('onboarding.finish'),
              onPressed: busy ? null : onEmpty,
              child: Text(S.onboardingTuTaoProject),
            ),
          ],
        ),
      ],
    );
  }
}

class _Bullet extends StatelessWidget {
  const _Bullet(this.text);

  final String text;

  @override
  Widget build(BuildContext context) => Padding(
    padding: const EdgeInsets.only(bottom: 8),
    child: Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        const Padding(
          padding: EdgeInsets.only(top: 7, right: 10),
          child: Icon(Icons.circle, size: 6, color: AppColors.textSecondary),
        ),
        Expanded(child: Text(text, style: const TextStyle(fontSize: 16))),
      ],
    ),
  );
}
