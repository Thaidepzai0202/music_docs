import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../engine/engine_providers.dart';
import 'audio_device_service.dart';
import 'capture_service.dart';
import 'export_service.dart';
import 'latency_service.dart';
import 'link_service.dart';
import 'midi_service.dart';
import 'peaks_service.dart';
import 'share_service.dart';

/// Tầng gọi engine (07 §5): widget → `ProjectController` (thay đổi project) hoặc service theo tính năng → engine.
final midiServiceProvider = Provider<MidiService>(
  (ref) => MidiService(ref.watch(engineProvider), ref.watch(enginePlatformProvider)),
);
final captureServiceProvider = Provider<CaptureService>((ref) => CaptureService(ref.watch(engineProvider)));
final exportServiceProvider = Provider<ExportService>((ref) => ExportService(ref.watch(engineProvider)));
final peaksServiceProvider = Provider<PeaksService>((ref) => PeaksService(ref.watch(engineProvider)));
final latencyServiceProvider = Provider<LatencyService>((ref) => LatencyService(ref.watch(engineProvider)));
final audioDeviceServiceProvider = Provider<AudioDeviceService>((ref) => AudioDeviceService(ref.watch(engineProvider)));
final linkServiceProvider = Provider<LinkService>((ref) => LinkService(ref.watch(engineProvider)));
final shareServiceProvider = Provider<ShareService>((ref) => const SharePlusService());
