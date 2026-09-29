// Test hợp đồng Dart ↔ C (05 §5): layout struct sinh bởi ffigen phải khớp engine_api.h.
// Engine C++ in ra LeCommand=32, LeState=248, LeConfig=40 byte (arm64). Nếu engine đổi header,
// chạy scripts/gen_bindings.sh → test này bắt lệch layout trước khi chạy trên máy.
import 'dart:ffi';
import 'dart:typed_data';

import 'package:engine_ffi/src/loopcore_bindings.g.dart';
import 'package:ffi/ffi.dart';
import 'package:flutter_test/flutter_test.dart';

ByteData _bytesOf<T extends NativeType>(Pointer<T> p, int size) =>
    ByteData.sublistView(p.cast<Uint8>().asTypedList(size));

void main() {
  test('kích thước struct khớp engine C++', () {
    expect(sizeOf<LeCommand>(), 32);
    expect(sizeOf<LeState>(), 248);
    expect(sizeOf<LeConfig>(), 40);
  });

  test('hằng số API', () {
    expect(LE_API_VERSION, 1);
    expect(LE_MAX_TRACKS, 8);
    expect(LE_MAX_SCENES, 8);
  });

  test('offset các trường LeCommand', () {
    final p = calloc<LeCommand>();
    addTearDown(() => calloc.free(p));
    p.ref
      ..type = 0x1234
      ..track = -1
      ..slot = 7
      ..i0 = 42
      ..f0 = 0.5
      ..f1 = -2.0
      ..d0 = 123.25
      ..hostTimeNs = 987654321;
    final b = _bytesOf(p, 32);
    expect(b.getUint16(0, Endian.little), 0x1234);
    expect(b.getInt8(2), -1);
    expect(b.getInt8(3), 7);
    expect(b.getInt32(4, Endian.little), 42);
    expect(b.getFloat32(8, Endian.little), 0.5);
    expect(b.getFloat32(12, Endian.little), -2.0);
    expect(b.getFloat64(16, Endian.little), 123.25);
    expect(b.getInt64(24, Endian.little), 987654321);
  });

  test('offset các trường LeState', () {
    final p = calloc<LeState>();
    addTearDown(() => calloc.free(p));
    final s = p.ref
      ..publishCounter = 7
      ..playing = 1
      ..linkPeers = 3
      ..beat = 4.5
      ..bpm = 128
      ..sampleRate = 48000
      ..bufferSize = 128
      ..beatsPerBar = 4
      ..quantize = LeQuantize.LE_Q_1_BAR
      ..latencyRoundTripSamples = 480
      ..cpuLoad = 0.25
      ..cpuPeak = 0.5
      ..xrunCount = 9
      ..activeVoices = 12
      ..inputPeak = 0.75;
    s.masterPeak[1] = 0.125;
    s.trackPeak[7][1] = 0.0625;
    s.clipState[7][7] = LeClipState.LE_CLIP_OVERDUBBING;
    s.clipState[2][5] = LeClipState.LE_CLIP_PLAYING;
    s.trackPlayingSlot[7] = -1;
    s.trackClipProgress[7] = 0.875;

    final b = _bytesOf(p, 248);
    expect(b.getUint32(0, Endian.little), 7);
    expect(b.getUint8(4), 1); // playing
    expect(b.getUint8(7), 3); // linkPeers
    expect(b.getFloat64(8, Endian.little), 4.5); // beat
    expect(b.getFloat64(16, Endian.little), 128); // bpm
    expect(b.getFloat64(24, Endian.little), 48000); // sampleRate
    expect(b.getInt32(32, Endian.little), 128); // bufferSize
    expect(b.getInt32(36, Endian.little), 4); // beatsPerBar
    expect(b.getInt32(40, Endian.little), LeQuantize.LE_Q_1_BAR);
    expect(b.getInt32(44, Endian.little), 480); // latencyRoundTripSamples
    expect(b.getFloat32(48, Endian.little), 0.25); // cpuLoad
    expect(b.getFloat32(52, Endian.little), 0.5); // cpuPeak
    expect(b.getUint32(56, Endian.little), 9); // xrunCount
    expect(b.getInt32(60, Endian.little), 12); // activeVoices
    expect(b.getFloat32(64, Endian.little), 0.75); // inputPeak
    expect(b.getFloat32(68 + 4, Endian.little), 0.125); // masterPeak[1]
    expect(b.getFloat32(76 + (7 * 2 + 1) * 4, Endian.little), 0.0625); // trackPeak[7][1]
    expect(b.getUint8(140 + 2 * 8 + 5), LeClipState.LE_CLIP_PLAYING); // clipState[track 2][scene 5]
    expect(b.getUint8(140 + 63), LeClipState.LE_CLIP_OVERDUBBING); // clipState[7][7]
    expect(b.getInt8(204 + 7), -1); // trackPlayingSlot[7]
    expect(b.getFloat32(212 + 7 * 4, Endian.little), 0.875); // trackClipProgress[7]
  });

  test('offset các trường LeConfig', () {
    final p = calloc<LeConfig>();
    addTearDown(() => calloc.free(p));
    p.ref
      ..apiVersion = 1
      ..preferredBufferSize = 128
      ..preferredSampleRate = 48000
      ..numInputChannels = 2
      ..dataDir = Pointer.fromAddress(0x1000)
      ..libraryDir = Pointer.fromAddress(0x2000);
    final b = _bytesOf(p, 40);
    expect(b.getInt32(0, Endian.little), 1);
    expect(b.getInt32(4, Endian.little), 128);
    expect(b.getFloat64(8, Endian.little), 48000);
    expect(b.getInt32(16, Endian.little), 2);
    expect(b.getInt32(20, Endian.little), 0); // _reserved
    expect(b.getUint64(24, Endian.little), 0x1000);
    expect(b.getUint64(32, Endian.little), 0x2000);
  });
}
