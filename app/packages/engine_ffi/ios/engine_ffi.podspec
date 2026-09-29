#
# engine_ffi — link LoopCore.xcframework (static) vào app iOS (09 §5).
#
# LoopCore.xcframework do agent Engine build bằng scripts/build_engine_xcframework.sh,
# output vào ios/Frameworks/. Chưa có file đó thì pod vẫn build được (chỉ phần Swift),
# và app tự dùng FakeEngineClient (EngineClient.tryOpen() trả null).
# Sau khi XCFramework xuất hiện: chạy lại `pod install` (hoặc `flutter clean && flutter build ios`).
#
has_engine = File.exist?(File.join(__dir__, 'Frameworks', 'LoopCore.xcframework'))

Pod::Spec.new do |s|
  s.name             = 'engine_ffi'
  s.version          = '0.1.0'
  s.summary          = 'Cầu nối Flutter ↔ LoopCore (dart:ffi + quyền mic).'
  s.description      = 'Bindings ffigen từ engine_api.h, EngineClient và plugin Swift xin quyền mic.'
  s.homepage         = 'https://example.invalid/loopcore'
  s.license          = { :file => '../LICENSE' }
  s.author           = { 'LoopCore' => 'dev@loopcore.invalid' }
  s.source           = { :path => '.' }
  s.source_files     = 'Classes/**/*'
  s.dependency 'Flutter'
  s.platform         = :ios, '17.0'
  s.swift_version    = '5.0'

  # Framework hệ thống JUCE + engine cần (danh sách chốt ở P0-03; QuartzCore do juce_audio_formats).
  s.frameworks = 'AVFoundation', 'AudioToolbox', 'CoreAudio', 'CoreMIDI', 'Accelerate',
                 'QuartzCore', 'UIKit', 'Foundation'
  s.libraries  = 'c++'

  s.pod_target_xcconfig = {
    'DEFINES_MODULE' => 'YES',
    'EXCLUDED_ARCHS[sdk=iphonesimulator*]' => 'i386',
    # Giữ symbol global (le_*) để DynamicLibrary.process() tìm thấy lúc chạy.
    'STRIP_STYLE' => 'non-global',
  }
  # Nếu pod bị link tĩnh vào Runner (use_frameworks! :linkage => :static) thì le_* nằm trong
  # binary app → app cũng không được strip symbol global.
  s.user_target_xcconfig = { 'STRIP_STYLE' => 'non-global' }

  if has_engine
    s.vendored_frameworks = 'Frameworks/LoopCore.xcframework'
  else
    # LoopCoreKeepAlive.c tham chiếu le_* → không có thư viện thì sẽ lỗi link.
    s.exclude_files = 'Classes/LoopCoreKeepAlive.c'
  end
end
