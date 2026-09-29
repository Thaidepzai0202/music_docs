import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/misc.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/engine/engine_bootstrap.dart';
import 'package:music_looper/engine/engine_providers.dart';

/// Engine giả đã `create`, tự dispose sau test.
FakeEngineClient createFakeEngine({bool autoCompleteJobs = true}) {
  final fake = FakeEngineClient(autoCompleteJobs: autoCompleteJobs);
  fake.create(const EngineConfig(dataDir: '/docs', libraryDir: '/bundle/Library'));
  addTearDown(fake.dispose);
  return fake;
}

List<Override> engineOverrides(FakeEngineClient fake) => [
  engineBootstrapProvider.overrideWithValue(
    EngineBootstrap(engine: fake, isFake: true, createResult: LeError.LE_OK, dataDir: '/docs'),
  ),
];

/// Giả lập kênh Swift của plugin. [permission]: 'granted' | 'denied' | 'undetermined'.
/// Trả danh sách method đã gọi (theo thứ tự).
List<String> mockEnginePlatform({
  String permission = 'granted',
  bool grantOnRequest = true,
  void Function(String method)? onCall,
}) {
  final calls = <String>[];
  TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.setMockMethodCallHandler(EnginePlatform.channel, (
    MethodCall call,
  ) async {
    calls.add(call.method);
    onCall?.call(call.method);
    return switch (call.method) {
      'micPermission' => permission,
      'requestMicPermission' => grantOnRequest,
      'openAppSettings' => true,
      'bundleResourcePath' => '/bundle',
      _ => null,
    };
  });
  addTearDown(
    () => TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.setMockMethodCallHandler(
      EnginePlatform.channel,
      null,
    ),
  );
  return calls;
}

/// Màn hình iPad 8: 1080×810 pt @2x, landscape.
void useIpad8Screen(WidgetTester tester) {
  tester.view.physicalSize = const Size(2160, 1620);
  tester.view.devicePixelRatio = 2;
  addTearDown(tester.view.reset);
}
