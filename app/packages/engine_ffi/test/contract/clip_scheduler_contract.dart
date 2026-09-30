// Hợp đồng HÀNH VI của ClipScheduler (04 §3), dùng chung cho FakeEngineClient và engine thật.
//
// Chỉ đi qua API công khai: le_send / le_call / le_read_state. Muốn chạy với engine thật chỉ cần
// một [ClipEngineHarness] biết cho thời gian nhạc trôi (engine thật: `sim.offline` + `sim.advance`).
//
// Ngữ nghĩa thời gian (khớp C++): lệnh le_send được xử lý ở ĐẦU block kế tiếp, nên sau mỗi lệnh
// gọi [ClipEngineHarness.settle] rồi mới đọc state.
import 'dart:ffi';
import 'dart:io';
import 'dart:math' as math;
import 'dart:typed_data';

import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter_test/flutter_test.dart';

abstract interface class ClipEngineHarness {
  EngineApi get engine;

  /// Vị trí transport CHÍNH XÁC hiện tại (beat) — LeState.beat chỉ là vị trí đầu block gần nhất.
  double get beat;

  /// Cho transport chạy thêm [beats] beat.
  Future<void> advanceBeats(double beats);

  /// Cho engine áp dụng các lệnh vừa gửi (engine thật: render 1 frame; Fake: không cần).
  Future<void> settle();

  /// Cho thời gian thực trôi (capture chạy cả khi transport dừng).
  Future<void> advanceSeconds(double seconds);

  /// Mẫu capture kế tiếp là im lặng (engine thật: `sim.offline` không có input nên luôn im lặng; Fake: bật cờ).
  void makeCaptureSilent();

  /// Đặt sẵn một mẫu CÓ TIẾNG ở [path] để phân tích / tạo nhạc cụ, thay cho capture (sim không thu được tiếng).
  /// [pitched]: sine C4 (nốt 60) hoặc noise. Engine thật: ghi WAV bằng [writeTestWav]; Fake: chỉ đăng ký.
  void addSample(String path, {required bool pitched});

  /// `LeConfig.libraryDir` của engine đang test (gốc của đường dẫn `base: "library"`).
  String get libraryDir;

  /// Ghi file văn bản (SFZ…) ở [path]. Engine thật: ghi đĩa; Fake: chỉ đăng ký là có file.
  void addTextFile(String path, String content);

  /// Lần `latency.calibrate` kế tiếp không nghe được tín hiệu (engine thật: sim không có input; Fake: bật cờ).
  void makeCalibrationSilent();

  /// Một message MIDI như từ thiết bị (engine thật: `sim.midiIn`, thiết bị "virtual").
  Future<void> midiIn(List<int> bytes);

  /// Engine có đẩy event về Dart trong lúc test không.
  bool get deliversEvents;

  Future<void> dispose();
}

/// WAV PCM 16-bit mono 48 kHz, [seconds] giây: sine C4 (261.63 Hz, nốt 60) hoặc noise trắng (seed cố định).
void writeTestWav(String path, {required bool pitched, double seconds = 1.0}) {
  const sr = 48000;
  final n = (seconds * sr).round();
  final rnd = math.Random(7);
  final data = ByteData(44 + n * 2);
  void tag(int at, String s) {
    for (var i = 0; i < 4; i++) {
      data.setUint8(at + i, s.codeUnitAt(i));
    }
  }

  tag(0, 'RIFF');
  data.setUint32(4, 36 + n * 2, Endian.little);
  tag(8, 'WAVE');
  tag(12, 'fmt ');
  data
    ..setUint32(16, 16, Endian.little)
    ..setUint16(20, 1, Endian.little) // PCM
    ..setUint16(22, 1, Endian.little) // mono
    ..setUint32(24, sr, Endian.little)
    ..setUint32(28, sr * 2, Endian.little)
    ..setUint16(32, 2, Endian.little)
    ..setUint16(34, 16, Endian.little);
  tag(36, 'data');
  data.setUint32(40, n * 2, Endian.little);
  for (var i = 0; i < n; i++) {
    final v = pitched ? 0.5 * math.sin(2 * math.pi * 261.6256 * i / sr) : rnd.nextDouble() * 0.8 - 0.4;
    data.setInt16(44 + i * 2, (v * 32767).round(), Endian.little);
  }
  File(path)
    ..parent.createSync(recursive: true)
    ..writeAsBytesSync(data.buffer.asUint8List());
}

const _q1Bar = LeQuantize.LE_Q_1_BAR;
const empty = LeClipState.LE_CLIP_EMPTY;
const stopped = LeClipState.LE_CLIP_STOPPED;
const queuedPlay = LeClipState.LE_CLIP_QUEUED_PLAY;
const playing = LeClipState.LE_CLIP_PLAYING;
const queuedStop = LeClipState.LE_CLIP_QUEUED_STOP;
const queuedRecord = LeClipState.LE_CLIP_QUEUED_RECORD;
const recording = LeClipState.LE_CLIP_RECORDING;
const overdubbing = LeClipState.LE_CLIP_OVERDUBBING;

/// [knownGaps]: tên ca → lý do tạm skip (engine chưa làm tới). Xoá dòng khi engine sửa xong.
void defineClipSchedulerContract(
  String label,
  Future<ClipEngineHarness> Function() create, {
  String? skip,
  Map<String, String> knownGaps = const {},
}) {
  void tc(String name, dynamic Function() body) => test(name, body, skip: knownGaps[name]);

  group('ClipScheduler (04 §3) — $label', skip: skip, () {
    late ClipEngineHarness h;
    EngineApi e() => h.engine;

    /// Gửi lệnh RT rồi đợi engine áp dụng. Trả giá trị le_send.
    Future<bool> cmd(int type, {int track = -1, int slot = -1, int i0 = 0, double f0 = 0, double d0 = 0}) async {
      final ok = e().send(type, track: track, slot: slot, i0: i0, f0: f0, d0: d0);
      await h.settle();
      return ok;
    }

    Future<Map<String, dynamic>> op(String name, [Map<String, dynamic> params = const {}]) async {
      final r = e().callOk(name, params);
      await h.settle();
      return r;
    }

    /// Mã lỗi của le_call (null = ok).
    String? errCode(Map<String, dynamic> request) {
      final r = e().call(request);
      return r['ok'] == true ? null : (r['error'] as Map)['code'] as String?;
    }

    setUp(() async {
      h = await create();
      await op('project.open', {'dir': '/tmp/contract.loopproj'});
      await op('transport.setTimeSignature', {'num': 4, 'den': 4});
      await cmd(LeCommandType.LE_CMD_SET_QUANTIZE, i0: _q1Bar);
      await cmd(LeCommandType.LE_CMD_SET_BPM, d0: 120);
      await cmd(LeCommandType.LE_CMD_SET_COUNT_IN, i0: 0);
    });
    tearDown(() => h.dispose());

    Future<void> clip(int t, int s, {double beats = 4}) =>
        op('clip.setMidi', {'track': t, 'slot': s, 'clipId': 'c_t${t}s$s', 'lengthBeats': beats, 'notes': <Object>[]});
    int st(int t, int s) => e().readState().clipState[t][s];
    bool transportPlaying() => e().readState().playing != 0;
    int playingSlot(int t) => e().readState().trackPlayingSlot[t];
    double progress(int t) => e().readState().trackClipProgress[t];

    /// `clip.info` → lengthBeats; null nếu ô trống (engine trả `{kind:"empty"}`).
    num? clipLength(int t, int s) {
      final r = e().call({'op': 'clip.info', 'track': t, 'slot': s});
      if (r['ok'] != true) return null;
      final info = (r['result'] as Map).cast<String, dynamic>();
      return info['kind'] == 'empty' ? null : info['lengthBeats'] as num?;
    }

    Future<void> launch(int t, int s) async =>
        expect(await cmd(LeCommandType.LE_CMD_CLIP_LAUNCH, track: t, slot: s), isTrue);

    /// Đưa transport tới đúng beat [b] (đang chạy).
    Future<void> goTo(double b) async {
      final d = b - h.beat;
      if (d > 0) await h.advanceBeats(d);
    }

    tc('ô có clip → Stopped, ô trống → Empty (clip.info báo ô trống)', () async {
      await clip(0, 0);
      expect(st(0, 0), stopped);
      expect(st(0, 1), empty);
      expect(clipLength(0, 0), 4);
      expect(clipLength(0, 1), isNull);
    });

    tc('project.open xoá sạch mọi clip, kể cả take vừa thu', () async {
      await clip(0, 0);
      await cmd(LeCommandType.LE_CMD_CLIP_RECORD, track: 1, slot: 0, i0: 1);
      await goTo(4.1);
      await cmd(LeCommandType.LE_CMD_TRANSPORT_STOP);
      await op('project.open', {'dir': '/tmp/contract2.loopproj'});
      expect([st(0, 0), st(1, 0)], [empty, empty]);
      expect(clipLength(0, 0), isNull);
      expect(clipLength(1, 0), isNull);
    });

    tc('transport dừng + launch → phát ngay từ beat 0, transport chạy', () async {
      await clip(0, 0);
      await launch(0, 0);
      expect(st(0, 0), playing);
      expect(transportPlaying(), isTrue);
      expect(h.beat, closeTo(0, 0.01));
      expect(playingSlot(0), 0);
    });

    tc('đang chạy: launch quantize 1 bar → QueuedPlay tới ranh giới rồi Playing', () async {
      await clip(0, 0);
      await clip(1, 0);
      await launch(0, 0); // khởi động transport
      await goTo(1.5);
      await launch(1, 0);
      expect(st(1, 0), queuedPlay);
      await goTo(3.9);
      expect(st(1, 0), queuedPlay);
      await goTo(4.1);
      expect(st(1, 0), playing);
      expect(playingSlot(1), 0);
    });

    tc('đứng đúng ranh giới (beat 4.0) → áp dụng ngay, không đợi bar sau', () async {
      await clip(0, 0);
      await clip(1, 0);
      await launch(0, 0);
      await goTo(4.0);
      await launch(1, 0);
      await h.advanceBeats(0.01);
      expect(st(1, 0), playing);
    });

    tc('quantize none → áp dụng ngay', () async {
      await clip(0, 0);
      await clip(1, 0);
      await launch(0, 0);
      await goTo(1.3);
      await cmd(LeCommandType.LE_CMD_SET_QUANTIZE, i0: LeQuantize.LE_Q_NONE);
      await launch(1, 0);
      expect(st(1, 0), playing);
    });

    tc('launch clip khác cùng track: A QueuedStop + B QueuedPlay, đổi cùng một ranh giới', () async {
      await clip(0, 0);
      await clip(0, 1);
      await launch(0, 0);
      await goTo(2);
      await launch(0, 1);
      expect(st(0, 0), queuedStop);
      expect(st(0, 1), queuedPlay);
      await goTo(4.05);
      expect(st(0, 0), stopped);
      expect(st(0, 1), playing);
      expect(playingSlot(0), 1);
    });

    tc('mỗi track chỉ 1 clip Playing; đổi ý trước ranh giới → lệnh sau thắng', () async {
      await clip(0, 0);
      await clip(0, 1);
      await clip(0, 2);
      await launch(0, 0);
      await goTo(1);
      await launch(0, 1);
      await launch(0, 2);
      expect(st(0, 1), stopped);
      await goTo(4.05);
      final row = [for (var s = 0; s < 3; s++) st(0, s)];
      expect(row.where((x) => x == playing).length, 1);
      expect(st(0, 2), playing);
    });

    tc('launch lại clip đang phát → retrigger tại ranh giới (phase về đầu)', () async {
      await clip(0, 0, beats: 8);
      await launch(0, 0);
      await goTo(2);
      await launch(0, 0);
      expect(st(0, 0), queuedPlay);
      await goTo(4.5);
      expect(st(0, 0), playing);
      expect(progress(0), closeTo(0.5 / 8, 0.01), reason: 'phát lại từ beat 4, không phải 4.5/8');
    });

    tc('CLIP_STOP: Playing → QueuedStop → Stopped tại ranh giới', () async {
      await clip(0, 0);
      await launch(0, 0);
      await goTo(5);
      await cmd(LeCommandType.LE_CMD_CLIP_STOP, track: 0);
      expect(st(0, 0), queuedStop);
      await goTo(7.9);
      expect(st(0, 0), queuedStop);
      await goTo(8.05);
      expect(st(0, 0), stopped);
      expect(playingSlot(0), -1);
    });

    tc('stop trước khi tới ranh giới launch → huỷ, về Stopped', () async {
      await clip(0, 0);
      await clip(1, 0);
      await launch(0, 0);
      await goTo(1);
      await launch(1, 0);
      await cmd(LeCommandType.LE_CMD_CLIP_STOP, track: 1);
      expect(st(1, 0), stopped);
      await goTo(4.05);
      expect(st(1, 0), stopped);
    });

    tc('launch ô trống = dừng track theo quantize', () async {
      await clip(0, 0);
      await launch(0, 0);
      await goTo(1);
      await launch(0, 5);
      expect(st(0, 0), queuedStop);
      await goTo(4.05);
      expect(st(0, 0), stopped);
    });

    tc('SCENE_LAUNCH: ô có clip launch, ô trống dừng track, cùng ranh giới', () async {
      await clip(0, 0);
      await clip(1, 0);
      await clip(0, 1);
      await clip(2, 1);
      await cmd(LeCommandType.LE_CMD_SCENE_LAUNCH, slot: 0);
      expect([st(0, 0), st(1, 0)], [playing, playing]);
      await goTo(2);
      await cmd(LeCommandType.LE_CMD_SCENE_LAUNCH, slot: 1);
      expect(st(0, 1), queuedPlay);
      expect(st(2, 1), queuedPlay);
      expect(st(1, 0), queuedStop, reason: 'track 1 không có clip ở scene 1 → dừng');
      await goTo(4.05);
      expect([st(0, 1), st(2, 1), st(1, 0), st(0, 0)], [playing, playing, stopped, stopped]);
    });

    tc('STOP_ALL: mọi track dừng tại ranh giới', () async {
      await clip(0, 0);
      await clip(3, 2);
      await cmd(LeCommandType.LE_CMD_SCENE_LAUNCH, slot: 0);
      await launch(3, 2);
      await goTo(4.5);
      await cmd(LeCommandType.LE_CMD_STOP_ALL);
      expect([st(0, 0), st(3, 2)], [queuedStop, queuedStop]);
      await goTo(8.05);
      expect([st(0, 0), st(3, 2)], [stopped, stopped]);
    });

    group('TRANSPORT_STOP (04 §3.4)', () {
      tc('dừng ngay, beat về 0, clip Stopped', () async {
        await clip(0, 0);
        await launch(0, 0);
        await goTo(6);
        await cmd(LeCommandType.LE_CMD_TRANSPORT_STOP);
        expect(transportPlaying(), isFalse);
        expect(e().readState().beat, closeTo(0, 1e-9));
        expect(st(0, 0), stopped);
      });

      tc('đang Recording → bỏ take dở, ô về Empty, không có clip', () async {
        await clip(0, 0);
        await launch(0, 0);
        await cmd(LeCommandType.LE_CMD_CLIP_RECORD, track: 1, slot: 0, i0: 2);
        await goTo(5);
        expect(st(1, 0), recording);
        await cmd(LeCommandType.LE_CMD_TRANSPORT_STOP);
        expect(st(1, 0), empty);
        expect(e().readState().anyRecording, 0);
        expect(clipLength(1, 0), isNull);
      });

      tc('đang Overdubbing → kết thúc overdub, giữ clip (Stopped)', () async {
        await clip(0, 0);
        await launch(0, 0);
        await cmd(LeCommandType.LE_CMD_OVERDUB_TOGGLE, track: 0);
        await goTo(2);
        await cmd(LeCommandType.LE_CMD_TRANSPORT_STOP);
        expect(st(0, 0), stopped);
        expect(clipLength(0, 0), 4);
      });

      tc('đang Queued* → huỷ lệnh chờ; play lại không tự áp dụng', () async {
        await clip(0, 0);
        await clip(0, 1);
        await clip(1, 0);
        await cmd(LeCommandType.LE_CMD_SCENE_LAUNCH, slot: 0); // (0,0) và (1,0) cùng phát từ beat 0
        await goTo(1);
        await launch(0, 1); // (0,0) QueuedStop, (0,1) QueuedPlay
        await cmd(LeCommandType.LE_CMD_CLIP_RECORD, track: 2, slot: 0, i0: 1); // QueuedRecord
        await cmd(LeCommandType.LE_CMD_CLIP_STOP, track: 1); // (1,0) QueuedStop
        expect([st(0, 0), st(0, 1), st(2, 0), st(1, 0)], [queuedStop, queuedPlay, queuedRecord, queuedStop]);
        await cmd(LeCommandType.LE_CMD_TRANSPORT_STOP);
        expect([st(0, 0), st(0, 1), st(2, 0), st(1, 0)], [stopped, stopped, empty, stopped]);
        await cmd(LeCommandType.LE_CMD_TRANSPORT_PLAY);
        await goTo(8.1);
        expect([st(0, 0), st(0, 1), st(2, 0), st(1, 0)], [stopped, stopped, empty, stopped]);
      });
    });

    tc('trackClipProgress chạy 0 → 1 theo độ dài clip', () async {
      await clip(0, 0, beats: 8);
      await launch(0, 0);
      await goTo(2);
      expect(progress(0), closeTo(0.25, 0.01));
      await goTo(6);
      expect(progress(0), closeTo(0.75, 0.01));
      await goTo(10);
      expect(progress(0), closeTo(0.25, 0.01), reason: 'loop');
    });

    group('thu âm', () {
      tc('CLIP_RECORD 1 bar: QueuedRecord → Recording → đủ độ dài thì Playing', () async {
        await clip(0, 0);
        await launch(0, 0);
        await goTo(1);
        expect(await cmd(LeCommandType.LE_CMD_CLIP_RECORD, track: 1, slot: 0, i0: 1), isTrue);
        expect(st(1, 0), queuedRecord);
        await goTo(4.05);
        expect(st(1, 0), recording);
        expect(e().readState().anyRecording, 1);
        await goTo(8.05);
        expect(st(1, 0), playing);
        expect(e().readState().anyRecording, 0);
        expect(clipLength(1, 0), 4, reason: 'take đã thu phải có trong clip.info');
      });

      tc('transport dừng + CLIP_RECORD → thu ngay từ beat 0', () async {
        await cmd(LeCommandType.LE_CMD_CLIP_RECORD, track: 0, slot: 0, i0: 1);
        expect(transportPlaying(), isTrue);
        expect(st(0, 0), recording);
      });

      tc('count-in 1 bar: QueuedRecord kéo dài qua count-in rồi mới Recording', () async {
        await cmd(LeCommandType.LE_CMD_SET_COUNT_IN, i0: 1);
        await cmd(LeCommandType.LE_CMD_CLIP_RECORD, track: 0, slot: 0, i0: 1);
        expect(st(0, 0), queuedRecord);
        await goTo(3.9);
        expect(st(0, 0), queuedRecord);
        await goTo(4.05);
        expect(st(0, 0), recording);
      });

      tc('thu tự do + RECORD_STOP → làm tròn LÊN theo quantize, tối thiểu 1 bar', () async {
        await cmd(LeCommandType.LE_CMD_CLIP_RECORD, track: 0, slot: 0, i0: 0); // bắt đầu beat 0
        await goTo(5.2);
        await cmd(LeCommandType.LE_CMD_RECORD_STOP, track: 0);
        expect(st(0, 0), recording, reason: 'kết thúc ở ranh giới bar kế tiếp (beat 8)');
        await goTo(8.05);
        expect(st(0, 0), playing);
        expect(clipLength(0, 0), 8);
      });

      tc('thu tự do rất ngắn (quantize 1/4) → vẫn tối thiểu 1 bar', () async {
        await cmd(LeCommandType.LE_CMD_SET_QUANTIZE, i0: LeQuantize.LE_Q_1_4);
        await cmd(LeCommandType.LE_CMD_CLIP_RECORD, track: 0, slot: 0, i0: 0);
        await goTo(0.5);
        await cmd(LeCommandType.LE_CMD_RECORD_STOP, track: 0);
        await goTo(4.05);
        expect(st(0, 0), playing);
        expect(clipLength(0, 0), 4);
      });

      tc('RECORD_STOP lúc QueuedRecord → huỷ, ô về Empty', () async {
        await clip(0, 0);
        await launch(0, 0);
        await goTo(1);
        await cmd(LeCommandType.LE_CMD_CLIP_RECORD, track: 1, slot: 0, i0: 1);
        expect(st(1, 0), queuedRecord);
        await cmd(LeCommandType.LE_CMD_RECORD_STOP, track: 1);
        expect(st(1, 0), empty);
        await goTo(8.1);
        expect(st(1, 0), empty);
      });

      tc('thu vào ô đã có clip → bỏ qua (ô giữ nguyên)', () async {
        await clip(0, 0);
        await cmd(LeCommandType.LE_CMD_CLIP_RECORD, track: 0, slot: 0, i0: 1);
        expect(st(0, 0), stopped);
      });

      tc('thu xong phát RECORDING_FINISHED(track, slot)', () async {
        if (!h.deliversEvents) return;
        final ev = e().events.firstWhere((x) => x is RecordingFinished).timeout(const Duration(seconds: 5));
        await cmd(LeCommandType.LE_CMD_CLIP_RECORD, track: 2, slot: 3, i0: 1);
        await h.advanceBeats(4.1);
        await h.settle();
        final r = await ev as RecordingFinished;
        expect([r.track, r.slot], [2, 3]);
      });
    });

    group('MIDI', () {
      List<Map<String, dynamic>> notes(int t, int s) => [
        for (final n in (e().callOk('clip.getMidi', {'track': t, 'slot': s})['notes'] as List))
          (n as Map).cast<String, dynamic>(),
      ];

      Future<void> instrumentTrack(int t) => op('track.configure', {'track': t, 'kind': 'instrument', 'name': 'Keys'});

      tc('thu MIDI trên track instrument: nốt trong lúc Recording thành clip MIDI đúng vị trí', () async {
        await instrumentTrack(3);
        await cmd(LeCommandType.LE_CMD_CLIP_RECORD, track: 3, slot: 0, i0: 1); // thu từ beat 0
        await goTo(0.5);
        await cmd(LeCommandType.LE_CMD_NOTE_ON, track: 3, i0: 60, f0: 0.8);
        await goTo(1.5);
        await cmd(LeCommandType.LE_CMD_NOTE_OFF, track: 3, i0: 60);
        await goTo(2.0);
        await cmd(LeCommandType.LE_CMD_NOTE_ON, track: 3, i0: 64, f0: 1.0);
        await goTo(2.25);
        await cmd(LeCommandType.LE_CMD_NOTE_OFF, track: 3, i0: 64);
        await goTo(4.05);
        expect(st(3, 0), playing);
        final info = e().callOk('clip.info', {'track': 3, 'slot': 0});
        expect(info['kind'], 'midi');
        expect(info['lengthBeats'], 4);
        final n = notes(3, 0);
        expect(n.length, 2);
        expect(n[0]['p'], 60);
        expect(n[0]['v'], closeTo(102, 1));
        expect(n[0]['s'], closeTo(0.5, 0.01));
        expect(n[0]['d'], closeTo(1.0, 0.01));
        expect(n[1]['p'], 64);
        expect(n[1]['s'], closeTo(2.0, 0.01));
        expect(n[1]['d'], closeTo(0.25, 0.01));
      });

      tc('phím còn giữ lúc hết take → nốt kéo tới cuối take', () async {
        await instrumentTrack(3);
        await cmd(LeCommandType.LE_CMD_CLIP_RECORD, track: 3, slot: 1, i0: 1);
        await goTo(3);
        await cmd(LeCommandType.LE_CMD_NOTE_ON, track: 3, i0: 67, f0: 0.5);
        await goTo(4.05);
        final n = notes(3, 1).single;
        expect(n['s'], closeTo(3, 0.01));
        expect(n['d'], closeTo(1, 0.01));
      });

      tc('track audio: take là audio (không phải MIDI)', () async {
        await cmd(LeCommandType.LE_CMD_CLIP_RECORD, track: 4, slot: 0, i0: 1);
        await goTo(4.05);
        expect(e().callOk('clip.info', {'track': 4, 'slot': 0})['kind'], 'audio');
      });

      tc('midi.setRecordQuantize 1/4: nốt thu được về lưới lúc thu xong', () async {
        await instrumentTrack(3);
        await op('midi.setRecordQuantize', {'grid': 0.25});
        await cmd(LeCommandType.LE_CMD_CLIP_RECORD, track: 3, slot: 2, i0: 1);
        await goTo(0.62);
        await cmd(LeCommandType.LE_CMD_NOTE_ON, track: 3, i0: 60, f0: 0.8);
        await goTo(1.1);
        await cmd(LeCommandType.LE_CMD_NOTE_OFF, track: 3, i0: 60);
        await goTo(4.05);
        await op('midi.setRecordQuantize', {'grid': 0});
        expect(notes(3, 2).single['s'], closeTo(0.5, 1e-6));
      });

      tc('clip.setMidi → clip.getMidi trả đúng nốt', () async {
        await op('clip.setMidi', {
          'track': 0,
          'slot': 0,
          'clipId': 'c_m',
          'lengthBeats': 4,
          'notes': [
            {'p': 36, 'v': 110, 's': 0.0, 'd': 0.25},
            {'p': 38, 'v': 90, 's': 1.0, 'd': 0.25},
          ],
        });
        final n = notes(0, 0);
        expect(n.map((x) => x['p']), [36, 38]);
        expect(n[1]['s'], 1.0);
      });

      tc('midiClip.quantize: điểm bắt đầu về lưới gần nhất, giữ độ dài', () async {
        await op('clip.setMidi', {
          'track': 0,
          'slot': 0,
          'clipId': 'c_q',
          'lengthBeats': 4,
          'notes': [
            {'p': 60, 'v': 100, 's': 0.13, 'd': 0.5},
            {'p': 62, 'v': 100, 's': 1.9, 'd': 0.25},
          ],
        });
        await op('midiClip.quantize', {'track': 0, 'slot': 0, 'grid': 0.25});
        final n = notes(0, 0);
        expect(n.map((x) => x['s']), [0.25, 2.0]);
        expect(n.map((x) => x['d']), [0.5, 0.25]);
      });
    });

    tc('clip.setParams: đổi gainDb / warp của clip audio, clip.info phản ánh (không decode lại)', () async {
      await cmd(LeCommandType.LE_CMD_CLIP_RECORD, track: 4, slot: 3, i0: 1);
      await goTo(4.05);
      await op('clip.setParams', {'track': 4, 'slot': 3, 'gainDb': -6.0, 'warp': 'stretch'});
      final info = e().callOk('clip.info', {'track': 4, 'slot': 3});
      expect(info['gainDb'], closeTo(-6, 1e-6));
      expect(info['warp'], 'stretch');
      expect(st(4, 3), playing, reason: 'đổi tham số không làm dừng clip');
      await op('clip.setParams', {'track': 4, 'slot': 3, 'warp': 'repitch'});
      final info2 = e().callOk('clip.info', {'track': 4, 'slot': 3});
      expect(info2['warp'], 'repitch');
      expect(info2['gainDb'], closeTo(-6, 1e-6), reason: 'trường không gửi thì giữ nguyên');
    });

    group('capture → sampler (P3-02/03)', () {
      late Directory dir;
      setUp(() => dir = Directory.systemTemp.createTempSync('capture_'));
      tearDown(() => dir.deleteSync(recursive: true));

      String newPath() {
        final d = Directory('${dir.path}/instruments/i_${DateTime.now().microsecondsSinceEpoch}')
          ..createSync(recursive: true);
        return '${d.path}/source.caf';
      }

      Future<Map<String, dynamic>> waitJob(int id) async {
        for (var i = 0; i < 2000; i++) {
          final r = e().callOk('job.result', {'jobId': id});
          if (r['status'] != 'running') return r;
          await h.settle();
          await Future<void>.delayed(const Duration(milliseconds: 1));
        }
        fail('job $id không xong');
      }

      Future<String> capture(double seconds) async {
        final path = newPath();
        await op('capture.start', {'path': path, 'maxSeconds': 10});
        await h.advanceSeconds(seconds);
        await op('capture.stop');
        return path;
      }

      tc('capture.start → capture.stop: trả {file, seconds}, file đã ghi', () async {
        final path = newPath();
        await op('capture.start', {'path': path, 'maxSeconds': 10});
        await h.advanceSeconds(1.5);
        final r = await op('capture.stop');
        expect(r['file'], path);
        expect(r['seconds'], closeTo(1.5, 0.05));
        expect(File(path).existsSync(), isTrue);
      });

      tc('tới maxSeconds tự dừng + RECORDING_FINISHED(-2,-2); capture.stop sau đó vẫn trả kết quả', () async {
        final path = newPath();
        final ev = h.deliversEvents
            ? e().events.firstWhere((x) => x is RecordingFinished && x.track == -2).timeout(const Duration(seconds: 5))
            : null;
        await op('capture.start', {'path': path, 'maxSeconds': 1});
        await h.advanceSeconds(1.3);
        await h.settle();
        if (ev != null) {
          final r = await ev as RecordingFinished;
          expect([r.track, r.slot], [-2, -2]);
          expect(r.frames, closeTo(48000, 480));
        }
        final res = await op('capture.stop');
        expect(res['seconds'], closeTo(1.0, 0.02));
        expect((await op('capture.stop'))['file'], path, reason: 'idempotent');
      });

      String sample({required bool pitched}) {
        final path = newPath().replaceFirst('source.caf', pitched ? 'sine.wav' : 'noise.wav');
        h.addSample(path, pitched: pitched);
        return path;
      }

      tc('capture.start: thiếu path / maxSeconds ngoài 0.1..120 → INVALID_ARG; input tắt → MIC_PERMISSION', () async {
        expect(errCode({'op': 'capture.start', 'maxSeconds': 1}), 'INVALID_ARG');
        expect(errCode({'op': 'capture.start', 'path': newPath()}), 'INVALID_ARG');
        expect(errCode({'op': 'capture.start', 'path': newPath(), 'maxSeconds': 0.05}), 'INVALID_ARG');
        expect(errCode({'op': 'capture.start', 'path': newPath(), 'maxSeconds': 121}), 'INVALID_ARG');
        await op('audio.setInputEnabled', {'enabled': false});
        expect(errCode({'op': 'capture.start', 'path': newPath(), 'maxSeconds': 1}), 'MIC_PERMISSION');
        await op('audio.setInputEnabled', {'enabled': true});
      });

      tc('capture.analyze → job: 512 cặp peaks, trim nằm trong file, confidence 0..1, frames + sampleRate', () async {
        final path = sample(pitched: true);
        final r = await waitJob(e().callJob('capture.analyze', {'file': path}));
        expect(r['status'], 'done');
        final res = (r['result'] as Map).cast<String, dynamic>();
        expect((res['peaks'] as List).length, 1024);
        expect(res['silent'], isFalse);
        expect(res['sampleRate'], 48000);
        expect(res['frames'] as int, closeTo(48000, 480));
        expect(res['trimStartSample'] as int, lessThan(res['trimEndSample'] as int));
        expect(res['trimEndSample'] as int, lessThanOrEqualTo(res['frames'] as int));
        expect(res['confidence'] as num, inInclusiveRange(0, 1));
        expect(res['rootNote'] as int, inInclusiveRange(0, 127));
      });

      tc('capture.analyze: mẫu im lặng → silent, rootNote −1, vẫn đủ 512 cặp peaks', () async {
        h.makeCaptureSilent();
        final path = await capture(1.0);
        final r = await waitJob(e().callJob('capture.analyze', {'file': path}));
        expect(r['status'], 'done');
        final res = (r['result'] as Map).cast<String, dynamic>();
        expect(res['silent'], isTrue);
        expect(res['rootNote'], -1);
        expect((res['peaks'] as List).length, 1024);
        expect(res['frames'] as int, closeTo(48000, 480));
      });

      tc('createFromRecording: không cao độ + thiếu rootNote → PITCH_NOT_DETECTED; có rootNote → xong', () async {
        final noise = sample(pitched: false);
        final failed = await waitJob(
          e().callJob('instrument.createFromRecording', {'instrumentId': 'i_a', 'file': noise, 'mode': 'natural'}),
        );
        expect(failed['status'], 'failed');
        expect((failed['error'] as Map)['code'], 'PITCH_NOT_DETECTED');
        final ok = await waitJob(
          e().callJob('instrument.createFromRecording', {
            'instrumentId': 'i_b',
            'file': noise,
            'rootNote': 60,
            'mode': 'classic',
          }),
        );
        expect(ok['status'], 'done');
        final res = (ok['result'] as Map).cast<String, dynamic>();
        expect(res['rootNote'], 60);
        expect(res['zones'], 13);
        expect(res['cached'], isA<bool>());
      });

      tc('createFromRecording: mẫu có cao độ, không truyền rootNote → tự dò; mẫu im lặng → INVALID_ARG', () async {
        final ok = await waitJob(
          e().callJob('instrument.createFromRecording', {
            'instrumentId': 'i_c',
            'file': sample(pitched: true),
            'mode': 'natural',
          }),
        );
        expect(ok['status'], 'done');
        expect((ok['result'] as Map)['rootNote'] as int, inInclusiveRange(0, 127));
        h.makeCaptureSilent();
        final silent = await capture(1.0);
        final failed = await waitJob(
          e().callJob('instrument.createFromRecording', {
            'instrumentId': 'i_d',
            'file': silent,
            'rootNote': 60,
            'mode': 'natural',
          }),
        );
        expect(failed['status'], 'failed');
        expect((failed['error'] as Map)['code'], 'INVALID_ARG');
      });

      tc('track.setInstrument {kind:"user"}: id đã tạo → job xong; id lạ → INVALID_ARG', () async {
        final created = await waitJob(
          e().callJob('instrument.createFromRecording', {
            'instrumentId': 'i_u',
            'file': sample(pitched: true),
            'rootNote': 60,
            'mode': 'natural',
          }),
        );
        expect(created['status'], 'done');
        await op('track.configure', {'track': 6, 'kind': 'instrument', 'name': 'U'});
        final job = e().callJob('track.setInstrument', {
          'track': 6,
          'instrument': {'kind': 'user', 'id': 'i_u'},
        });
        expect((await waitJob(job))['status'], 'done');
        expect(
          errCode({
            'op': 'track.setInstrument',
            'track': 6,
            'instrument': {'kind': 'user', 'id': 'i_none'},
          }),
          'INVALID_ARG',
        );
      });
    });

    group('FX · envelope · export (P3-07/12/17)', () {
      late Directory dir;
      setUp(() => dir = Directory.systemTemp.createTempSync('p3_'));
      tearDown(() => dir.deleteSync(recursive: true));

      Future<Map<String, dynamic>> waitJob(int id) async {
        for (var i = 0; i < 4000; i++) {
          final r = e().callOk('job.result', {'jobId': id});
          if (r['status'] != 'running') return r;
          await h.settle();
          await Future<void>.delayed(const Duration(milliseconds: 1));
        }
        fail('job $id không xong');
      }

      tc(
        'fx.set / fx.remove: track 0..7, index 0..2, 5 loại (bí danh "compressor"); master hoặc sai → INVALID_ARG',
        () async {
          await op('fx.set', {
            'track': 0,
            'index': 0,
            'type': 'filter',
            'params': {'0': 1, '1': 800, '2': 2},
            'bypass': false,
          });
          for (final (i, type) in [(1, 'delay'), (2, 'reverb')]) {
            await op('fx.set', {'track': 3, 'index': i, 'type': type, 'params': <String, Object>{}, 'bypass': true});
          }
          await op('fx.set', {
            'track': 7,
            'index': 0,
            'type': 'comp',
            'params': {'0': -24, '1': 4},
            'bypass': false,
          });
          await op('fx.set', {
            'track': 6,
            'index': 2,
            'type': 'compressor',
            'params': {'4': 3},
            'bypass': false,
          });
          expect(
            errCode({'op': 'fx.set', 'track': -1, 'index': 0, 'type': 'eq3', 'params': {}}),
            'INVALID_ARG',
            reason: 'master không có slot người dùng (EQ3/Limiter cố định, chỉnh bằng FX_PARAM track −1)',
          );
          expect(errCode({'op': 'fx.set', 'track': 0, 'index': 3, 'type': 'filter', 'params': {}}), 'INVALID_ARG');
          expect(errCode({'op': 'fx.set', 'track': 8, 'index': 0, 'type': 'filter', 'params': {}}), 'INVALID_ARG');
          expect(errCode({'op': 'fx.set', 'track': 0, 'index': 0, 'type': 'chorus', 'params': {}}), 'INVALID_ARG');
          expect(
            errCode({
              'op': 'fx.set',
              'track': 0,
              'index': 0,
              'type': 'filter',
              'params': {'3': 1},
            }),
            'INVALID_ARG',
            reason: 'id không có ở loại filter',
          );
          expect(
            errCode({
              'op': 'fx.set',
              'track': 0,
              'index': 0,
              'type': 'filter',
              'params': {'1': 'x'},
            }),
            'INVALID_ARG',
          );
          await op('fx.remove', {'track': 0, 'index': 0});
          await op('fx.remove', {'track': 0, 'index': 0}); // slot đã trống → vẫn ok (idempotent)
          expect(errCode({'op': 'fx.remove', 'track': 0, 'index': 5}), 'INVALID_ARG');
        },
      );

      tc(
        'FX_PARAM / FX_BYPASS vào slot có FX + master (slot 0 EQ3, slot 1 limiter) → le_send true, clip vẫn phát',
        () async {
          await clip(0, 0);
          await launch(0, 0);
          await op('fx.set', {
            'track': 0,
            'index': 1,
            'type': 'delay',
            'params': {'0': 4},
            'bypass': false,
          });
          expect(await cmd(LeCommandType.LE_CMD_FX_PARAM, track: 0, slot: 1, i0: 1, f0: 0.6), isTrue);
          expect(await cmd(LeCommandType.LE_CMD_FX_BYPASS, track: 0, slot: 1, i0: 1), isTrue);
          expect(await cmd(LeCommandType.LE_CMD_FX_PARAM, track: -1, slot: 0, i0: 0, f0: 2), isTrue);
          expect(await cmd(LeCommandType.LE_CMD_FX_PARAM, track: -1, slot: 1, i0: 0, f0: -1.5), isTrue);
          expect(
            await cmd(LeCommandType.LE_CMD_FX_PARAM, track: 0, slot: 2, i0: 0, f0: 1),
            isFalse,
            reason: 'slot trống',
          );
          expect(
            await cmd(LeCommandType.LE_CMD_FX_PARAM, track: 0, slot: 1, i0: 4, f0: 1),
            isFalse,
            reason: 'delay không có id 4',
          );
          expect(
            await cmd(LeCommandType.LE_CMD_FX_PARAM, track: -1, slot: 2, i0: 0, f0: 1),
            isFalse,
            reason: 'master chỉ slot 0/1',
          );
          expect(
            await cmd(LeCommandType.LE_CMD_FX_BYPASS, track: -1, slot: 0, i0: 1),
            isTrue,
            reason: 'bypass EQ master',
          );
          expect(await cmd(LeCommandType.LE_CMD_FX_BYPASS, track: -1, slot: 0, i0: 0), isTrue);
          expect(
            await cmd(LeCommandType.LE_CMD_FX_BYPASS, track: -1, slot: 1, i0: 1),
            isFalse,
            reason: 'limiter không bypass',
          );
          await op('fx.remove', {'track': 0, 'index': 1});
          expect(st(0, 0), playing, reason: 'thêm/bớt FX không làm dừng clip');
        },
      );

      tc(
        'instrument.setEnvelope: gọi ngay sau createFromRecording (job chưa xong) vẫn ok; s > 1 → INVALID_ARG',
        () async {
          final path = '${dir.path}/src.wav';
          h.addSample(path, pitched: true);
          final job = e().callJob('instrument.createFromRecording', {
            'instrumentId': 'i_env',
            'file': path,
            'rootNote': 60,
            'mode': 'natural',
          });
          await op('instrument.setEnvelope', {'instrumentId': 'i_env', 'a': 0.01, 'd': 0.3, 's': 0.6, 'r': 0.8});
          expect((await waitJob(job))['status'], 'done');
          await op('instrument.setEnvelope', {'instrumentId': 'i_env', 'a': 0.2, 'd': 0.1, 's': 1.0, 'r': 2.0});
          expect(
            errCode({'op': 'instrument.setEnvelope', 'instrumentId': 'i_env', 'a': 0.01, 'd': 0.1, 's': 1.5, 'r': 0.3}),
            'INVALID_ARG',
          );
          expect(
            errCode({
              'op': 'instrument.setEnvelope',
              'instrumentId': 'i_env',
              'a': 0.01,
              'd': 0.1,
              's': 0.5,
              'r': 10.5,
            }),
            'INVALID_ARG',
            reason: 'a/d/r 0..10 giây',
          );
          await op('instrument.setEnvelope', {'instrumentId': 'i_env', 'a': 10, 'd': 10, 's': 0, 'r': 10});
          expect(
            errCode({
              'op': 'instrument.setEnvelope',
              'instrumentId': 'i_none',
              'a': 0.01,
              'd': 0.1,
              's': 0.5,
              'r': 0.3,
            }),
            'INVALID_ARG',
          );
        },
      );

      tc(
        'export.jamStart → jamStop: trả {file, seconds, droppedFrames}, file đã ghi; jamStop lần 2 → INVALID_ARG',
        () async {
          final path = '${dir.path}/Exports/jam_1.wav';
          Directory('${dir.path}/Exports').createSync();
          await clip(0, 0);
          await launch(0, 0);
          await op('export.jamStart', {'path': path});
          expect(errCode({'op': 'export.jamStart', 'path': path}), 'INVALID_ARG', reason: 'đang ghi');
          await h.advanceSeconds(2.0);
          final r = await op('export.jamStop');
          expect(r['file'], path);
          expect(r['seconds'] as num, closeTo(2.0, 0.05));
          expect(r['droppedFrames'] as int, greaterThanOrEqualTo(0));
          expect(File(path).existsSync(), isTrue);
          expect(errCode({'op': 'export.jamStop'}), 'INVALID_ARG');

          // Đuôi .m4a → AAC.
          final m4a = '${dir.path}/Exports/jam_2.m4a';
          await op('export.jamStart', {'path': m4a});
          await h.advanceSeconds(1.0);
          expect((await op('export.jamStop'))['file'], m4a);
          expect(File(m4a).existsSync(), isTrue);
        },
      );

      tc(
        'export.scene → job xong, result {file, seconds = bars·beatsPerBar·60/bpm}; stems <base>_t<n> với n đếm từ 1',
        () async {
          await clip(0, 2);
          await clip(3, 2);
          final path = '${dir.path}/scene3.wav';
          expect(
            errCode({'op': 'export.scene', 'scene': 2, 'bars': 2, 'path': path, 'format': 'ogg', 'stems': false}),
            'INVALID_ARG',
          );
          final r = await waitJob(
            e().callJob('export.scene', {'scene': 2, 'bars': 2, 'path': path, 'format': 'wav', 'stems': false}),
          );
          expect(r['status'], 'done');
          final res = (r['result'] as Map).cast<String, dynamic>();
          expect(res['file'], path);
          expect(res['seconds'] as num, closeTo(4.0, 1e-3)); // 2 bar · 4 beat · 0.5 s @120
          expect(res['frames'] as int, closeTo(4 * 48000, 1));
          expect(res['clippedSamples'] as int, greaterThanOrEqualTo(0));
          expect(File(path).existsSync(), isTrue);
          expect(
            errCode({'op': 'export.scene', 'scene': 2, 'bars': 513, 'path': path, 'format': 'wav', 'stems': false}),
            'INVALID_ARG',
            reason: 'bars 1..512',
          );
          expect(st(0, 2), stopped, reason: 'export offline không đụng phần đang chơi live');

          final stemPath = '${dir.path}/scene3s.wav';
          final rs = await waitJob(
            e().callJob('export.scene', {'scene': 2, 'bars': 1, 'path': stemPath, 'format': 'wav', 'stems': true}),
          );
          final stems = [for (final x in ((rs['result'] as Map)['stems'] as List)) '$x'];
          expect(stems.toSet(), {
            '${dir.path}/scene3s_t1.wav',
            '${dir.path}/scene3s_t4.wav',
          }, reason: 'chỉ track có clip ở scene đó');
          expect(stems.every((f) => File(f).existsSync()), isTrue);
        },
      );

      tc('audio.setInputEnabled: false → inputChannels 0 (chỉ phát, transport vẫn chạy); true → ≥ 1', () async {
        await clip(0, 0);
        await launch(0, 0);
        expect((await op('audio.setInputEnabled', {'enabled': false}))['inputChannels'], 0);
        expect(e().callOk('engine.info')['inputChannels'], 0);
        await h.advanceBeats(1);
        expect(st(0, 0), playing, reason: 'tắt input không làm dừng phần đang phát');
        expect((await op('audio.setInputEnabled', {'enabled': true}))['inputChannels'] as int, greaterThanOrEqualTo(1));
        expect(errCode({'op': 'audio.setInputEnabled', 'enabled': 'yes'}), 'INVALID_ARG');
      });
    });

    group('Pedal mode + nút LOOP (04 §2.5, 05 §2 LE_CMD_LOOP_BUTTON)', () {
      Map<String, dynamic> tempoState() => (e().callOk('engine.info')['tempoState'] as Map).cast<String, dynamic>();
      // "Chưa có vòng đầu": engine trả 0, Fake trả null — hợp đồng chấp nhận cả hai.
      Map<String, dynamic> modeAndTempo() => {'mode': tempoState()['mode'], 'hasTempo': tempoState()['hasTempo']};
      final noFirstLoop = anyOf(isNull, 0);

      Future<void> audioTrack(int t) async {
        await op('track.configure', {'track': t, 'kind': 'audio', 'name': 'A$t'});
        await cmd(LeCommandType.LE_CMD_TRACK_ARM, track: t, i0: 1);
      }

      /// Engine chốt take có thể chậm vài block (ghi file trên worker) → chạy từng block tới khi [done].
      Future<void> until(bool Function() done, String what) async {
        for (var i = 0; i < 400 && !done(); i++) {
          await h.advanceSeconds(128 / 48000);
          await Future<void>.delayed(Duration.zero);
        }
        expect(done(), isTrue, reason: what);
      }

      /// Vòng đầu dài [seconds] trên track [t] ô 0.
      Future<void> firstLoop(int t, double seconds) async {
        expect(await cmd(LeCommandType.LE_CMD_CLIP_RECORD, track: t, slot: 0, i0: 0), isTrue);
        await h.advanceSeconds(seconds);
        expect(await cmd(LeCommandType.LE_CMD_RECORD_STOP, track: t), isTrue);
        await until(() => clipLength(t, 0) != null, 'vòng đầu chốt xong');
      }

      tc('transport.setTempoMode: fixed → hasTempo; firstLoop chưa có clip → chờ vòng đầu; có clip → có tempo; '
          'mode lạ → INVALID_ARG', () async {
        expect((await op('transport.setTempoMode', {'mode': 'fixed'}))['hasTempo'], isTrue);
        expect(modeAndTempo(), {'mode': 'fixed', 'hasTempo': true});
        expect((await op('transport.setTempoMode', {'mode': 'firstLoop'}))['hasTempo'], isFalse);
        expect(modeAndTempo(), {'mode': 'firstLoop', 'hasTempo': false});
        expect(tempoState()['firstLoopBeats'], noFirstLoop);
        await clip(2, 0);
        expect(tempoState()['hasTempo'], isTrue, reason: 'model đã có clip → BPM lấy từ model');
        expect(errCode({'op': 'transport.setTempoMode', 'mode': 'link'}), 'INVALID_ARG');
      });

      tc('vòng đầu: thu ngay, transport chưa chạy; chốt sau 5 s → 8 beat @96 BPM, transport chạy, clip phát', () async {
        await op('transport.setTempoMode', {'mode': 'firstLoop'});
        await audioTrack(4);
        final tempo = h.deliversEvents
            ? e().events.firstWhere((x) => x is TempoChanged).timeout(const Duration(seconds: 5))
            : null;
        expect(await cmd(LeCommandType.LE_CMD_CLIP_RECORD, track: 4, slot: 0, i0: 0), isTrue);
        expect(st(4, 0), recording, reason: 'không quantize, không count-in');
        expect(transportPlaying(), isFalse, reason: 'transport chưa chạy trong vòng đầu');
        await h.advanceSeconds(5.0);
        expect(await cmd(LeCommandType.LE_CMD_RECORD_STOP, track: 4), isTrue);
        await until(() => clipLength(4, 0) != null, 'vòng đầu chốt xong');
        // 60·4/5 = 48 < 80 → nb = 8 → 96 BPM ∈ [80, 160).
        expect(e().readState().bpm, closeTo(96, 0.5));
        expect(clipLength(4, 0), 8);
        expect(transportPlaying(), isTrue);
        expect(st(4, 0), playing, reason: 'phát tiếp liền mạch');
        expect(tempoState()['hasTempo'], isTrue);
        if (tempo != null) expect((await tempo as TempoChanged).bpm, closeTo(96, 0.5));
      });

      tc('vòng sau (pedal mode): độ dài làm tròn LÊN bội số của vòng đầu', () async {
        await op('transport.setTempoMode', {'mode': 'firstLoop'});
        await audioTrack(4);
        await audioTrack(5);
        await firstLoop(4, 2.4); // 60·4/2.4 = 100 BPM, 4 beat
        expect(clipLength(4, 0), 4);
        expect(await cmd(LeCommandType.LE_CMD_CLIP_RECORD, track: 5, slot: 0, i0: 0), isTrue);
        for (var i = 0; i < 64 && st(5, 0) != recording; i++) {
          await h.advanceBeats(0.25); // bắt đầu theo quantize (1 bar)
        }
        expect(st(5, 0), recording);
        final start = h.beat;
        await h.advanceBeats(5); // thu ~5 beat
        expect(await cmd(LeCommandType.LE_CMD_RECORD_STOP, track: 5), isTrue);
        await goTo(start + 8.5);
        expect(clipLength(5, 0), 8, reason: 'ceil(5/4)·4 = 8');
        expect(st(5, 0), playing);
      });

      tc('setTempoMode {firstLoop, firstLoopBeats: 8} (mở lại project) → vòng sau làm tròn lên bội số 8', () async {
        final r = await op('transport.setTempoMode', {'mode': 'firstLoop', 'firstLoopBeats': 8});
        expect(r['hasTempo'], isFalse, reason: 'chưa có clip');
        await clip(2, 0, beats: 8); // clip của project vừa mở → có tempo
        expect(tempoState()['hasTempo'], isTrue);
        expect(tempoState()['firstLoopBeats'], 8);
        await audioTrack(5);
        expect(await cmd(LeCommandType.LE_CMD_CLIP_RECORD, track: 5, slot: 0, i0: 0), isTrue);
        for (var i = 0; i < 64 && st(5, 0) != recording; i++) {
          await h.advanceBeats(0.25);
        }
        final start = h.beat;
        await h.advanceBeats(9); // thu ~9 beat → làm tròn lên 16 (quantize 1 bar sẽ ra 12)
        expect(await cmd(LeCommandType.LE_CMD_RECORD_STOP, track: 5), isTrue);
        await goTo(start + 16.5);
        expect(clipLength(5, 0), 16);
      });

      tc('về "chưa có tempo" → TEMPO_CHANGED(value = 0); lúc nhận event thì tempoState.hasTempo đã là false', () async {
        await op('transport.setTempoMode', {'mode': 'firstLoop'});
        await audioTrack(4);
        await firstLoop(4, 2.4);
        final zero = h.deliversEvents
            ? e().events.firstWhere((x) => x is TempoChanged && x.bpm == 0).timeout(const Duration(seconds: 5))
            : null;
        await cmd(LeCommandType.LE_CMD_TRANSPORT_STOP);
        await op('clip.clear', {'track': 4, 'slot': 0});
        await until(() => tempoState()['hasTempo'] == false, 'về chưa có tempo');
        if (zero != null) expect((await zero as TempoChanged).bpm, 0);
        expect(tempoState()['firstLoopBeats'], noFirstLoop);
      });

      tc('pedal mode: dừng transport + xoá hết clip → lại chờ vòng đầu', () async {
        await op('transport.setTempoMode', {'mode': 'firstLoop'});
        await audioTrack(4);
        await firstLoop(4, 2.4);
        expect(tempoState()['hasTempo'], isTrue);
        await cmd(LeCommandType.LE_CMD_TRANSPORT_STOP);
        expect(tempoState()['hasTempo'], isTrue, reason: 'còn clip');
        await op('clip.clear', {'track': 4, 'slot': 0});
        expect(tempoState()['hasTempo'], isFalse);
      });

      tc(
        'LOOP_BUTTON slot −1: Empty → thu → chốt → Playing → overdub → Playing; Stopped → launch; track sai → false',
        () async {
          await op('transport.setTempoMode', {'mode': 'fixed'});
          await audioTrack(4);
          Future<bool> loop([int slot = -1]) => cmd(LeCommandType.LE_CMD_LOOP_BUTTON, track: 4, slot: slot);
          expect(await loop(), isTrue);
          expect([queuedRecord, recording], contains(st(4, 0)), reason: 'ô trống đầu tiên → thu tự do');
          await goTo(2);
          expect(st(4, 0), recording);
          expect(await loop(), isTrue); // chốt: làm tròn lên 1 bar
          await goTo(4.05);
          expect(st(4, 0), playing);
          expect(clipLength(4, 0), 4);
          expect(await loop(), isTrue);
          expect(st(4, 0), overdubbing);
          expect(await loop(), isTrue);
          expect(st(4, 0), playing);
          await cmd(LeCommandType.LE_CMD_CLIP_STOP, track: 4);
          await goTo(8.05);
          expect(st(4, 0), stopped);
          expect(await loop(0), isTrue, reason: 'Stopped → launch');
          expect([queuedPlay, playing], contains(st(4, 0)));
          expect(e().send(LeCommandType.LE_CMD_LOOP_BUTTON, track: 9, slot: -1), isFalse);
        },
      );
    });

    group('Settings (P4-12/13)', () {
      tc('midi.listDevices → {inputs:[{id,name,enabled,open}], outputs:[{id,name}]}', () {
        final r = e().callOk('midi.listDevices');
        expect(r['inputs'], isA<List<Object?>>());
        expect(r['outputs'], isA<List<Object?>>());
        for (final d in r['inputs'] as List) {
          expect((d as Map).keys, containsAll(['id', 'name', 'enabled', 'open']));
          expect([d['enabled'], d['open']], everyElement(isA<bool>()));
        }
        for (final d in r['outputs'] as List) {
          expect((d as Map).keys, containsAll(['id', 'name']));
        }
      });

      tc('midi.enableDevice: nhớ cả id chưa cắm; thiếu enabled → INVALID_ARG', () async {
        await op('midi.enableDevice', {'id': 'usb:chua-cam', 'enabled': true});
        await op('midi.enableDevice', {'id': 'usb:chua-cam', 'enabled': false});
        expect(errCode({'op': 'midi.enableDevice', 'id': 'usb:chua-cam'}), 'INVALID_ARG');
      });

      tc('learn: message nốt / CC kế tiếp → MIDI_LEARNED + learnResult {deviceName, kind, channel, number}', () async {
        final ev = h.deliversEvents
            ? e().events.firstWhere((x) => x is MidiLearned).timeout(const Duration(seconds: 5))
            : null;
        await op('midi.learnStart', {
          'target': {'kind': 'clip', 'track': 1, 'slot': 2},
        });
        await h.midiIn([0xB3, 21, 64]); // CC 21, kênh 4 (0-based 3)
        await h.settle();
        if (ev != null) {
          final l = await ev as MidiLearned;
          expect([l.isCc, l.number], [true, 21]);
        }
        final r = e().callOk('midi.learnResult');
        expect([r['kind'], r['channel'], r['number'], r['deviceName']], ['cc', 3, 21, 'virtual']);
      });

      tc('midi.learnStart / setMappings nhận target footswitch {kind:"trackStop"} và {kind:"undoOverdub"}', () async {
        for (final k in const ['trackStop', 'undoOverdub']) {
          await op('midi.learnStart', {
            'target': {'kind': k},
          });
          await op('midi.learnCancel');
          await op('midi.setMappings', {
            'mappings': [
              {
                'src': {'device': '', 'kind': 'cc', 'channel': -1, 'number': 64},
                'target': {'kind': k},
              },
            ],
          });
        }
        await op('midi.setMappings', {'mappings': <Object>[]});
      });

      tc('midi.setMappings: kind ≠ note/cc, channel ngoài −1..15, target sai → INVALID_ARG; hợp lệ → ok', () async {
        Map<String, Object> m({Object kind = 'note', Object channel = -1, Object? target}) => {
          'src': {'device': '', 'kind': kind, 'channel': channel, 'number': 36},
          'target': target ?? {'kind': 'stopAll'},
        };
        await op('midi.setMappings', {
          'mappings': [
            m(),
            m(kind: 'cc', channel: 15, target: {'kind': 'trackGain', 'track': 0}),
          ],
        });
        for (final bad in [
          m(kind: 'pitchbend'),
          m(channel: 16),
          m(channel: -2),
          m(target: {'kind': 'tempo'}),
        ]) {
          expect(
            errCode({
              'op': 'midi.setMappings',
              'mappings': [bad],
            }),
            'INVALID_ARG',
            reason: '$bad',
          );
        }
        await op('midi.setMappings', {'mappings': <Object>[]});
      });

      tc('preview.play {source sfz|audio, base library|project, note?, durationMs?} → {}; nguồn sai → INVALID_ARG; '
          'preview.stop → {}', () async {
        final lib = h.libraryDir;
        h.addSample('$lib/preview/tone.wav', pitched: true);
        h.addTextFile('$lib/preview/tone.sfz', '<region> sample=tone.wav pitch_keycenter=60\n');
        await op('preview.play', {
          'source': {'kind': 'sfz', 'path': 'preview/tone.sfz'},
        });
        await op('preview.play', {
          'source': {'kind': 'audio', 'file': 'preview/tone.wav', 'base': 'library'},
          'note': 60,
          'durationMs': 1500,
        });
        // base "project": tương đối theo thư mục của project.open (Bản thu của tôi).
        final proj = '$lib/preview_project.loopproj';
        await op('project.open', {'dir': proj});
        h.addSample('$proj/instruments/i_1/source.wav', pitched: true);
        await op('preview.play', {
          'source': {'kind': 'audio', 'file': 'instruments/i_1/source.wav', 'base': 'project'},
        });
        expect(
          errCode({
            'op': 'preview.play',
            'source': {'kind': 'audio', 'file': 'preview/khong_co.wav'},
          }),
          'FILE_NOT_FOUND',
        );
        for (final bad in <Map<String, Object>>[
          {'kind': 'midi', 'path': 'x'},
          {'kind': 'sfz', 'path': ''},
          {'kind': 'audio', 'file': 'a.wav', 'base': 'cloud'},
        ]) {
          expect(errCode({'op': 'preview.play', 'source': bad}), 'INVALID_ARG', reason: '$bad');
        }
        expect(
          errCode({
            'op': 'preview.play',
            'source': {'kind': 'sfz', 'path': 'kits/kit_808/kit_808.sfz'},
            'note': 200,
          }),
          'INVALID_ARG',
        );
        await op('preview.stop');
      });

      tc('memory.pressure {warning|critical} → {freedMB, usedMB} + MEMORY_WARNING; engine.info.memoryMB', () async {
        final ev = h.deliversEvents
            ? e().events.firstWhere((x) => x is MemoryWarning).timeout(const Duration(seconds: 5))
            : null;
        final r = await op('memory.pressure', {'level': 'critical'});
        expect([r['freedMB'], r['usedMB']], everyElement(isA<num>()));
        await h.settle();
        if (ev != null) expect((await ev as MemoryWarning).critical, isTrue);
        await op('memory.pressure', {'level': 'warning'});
        expect(errCode({'op': 'memory.pressure', 'level': 'panic'}), 'INVALID_ARG');
        expect(e().callOk('engine.info')['memoryMB'], isA<num>());
      });

      tc(
        'midi.learnStart nhận đủ 8 kind target (khớp LearnAction, gồm loopButton); kind lạ → INVALID_ARG; learnCancel ok',
        () async {
          const targets = <Map<String, Object>>[
            {'kind': 'clip', 'track': 2, 'slot': 5},
            {'kind': 'scene', 'slot': 3},
            {'kind': 'transport', 'action': 'toggle'},
            {'kind': 'stopAll'},
            {'kind': 'loopButton'}, // footswitch → như LE_CMD_LOOP_BUTTON slot −1 trên track đang chọn (05 §3)
            {'kind': 'trackGain', 'track': 1, 'minDb': -60, 'maxDb': 6},
            {'kind': 'trackMute', 'track': 7},
            {'kind': 'fx', 'track': -1, 'slot': 0, 'param': 2},
            {'kind': 'fx', 'track': 4, 'slot': 2, 'param': 1, 'min': 0, 'max': 0.5},
          ];
          for (final t in targets) {
            await op('midi.learnStart', {'target': t});
            await op('midi.learnCancel');
          }
          expect(
            errCode({
              'op': 'midi.learnStart',
              'target': {'kind': 'tempo'},
            }),
            'INVALID_ARG',
          );
          expect(
            errCode({
              'op': 'midi.learnStart',
              'target': {'kind': 'transport', 'action': 'record'},
            }),
            'INVALID_ARG',
          );
        },
      );

      tc(
        'latency.setOffset {samples:int ±96000} → ok, engine.info phản ánh, project.open không reset; sai → INVALID_ARG',
        () async {
          await op('latency.setOffset', {'samples': 96000});
          await op('latency.setOffset', {'samples': -96});
          await op('project.open', {'dir': '/tmp/contract.loopproj'});
          expect(e().callOk('engine.info')['latencyOffsetSamples'], -96, reason: 'thiết lập toàn cục');
          expect(errCode({'op': 'latency.setOffset', 'samples': 'x'}), 'INVALID_ARG');
          expect(errCode({'op': 'latency.setOffset', 'samples': 1.5}), 'INVALID_ARG');
          expect(errCode({'op': 'latency.setOffset', 'samples': 96001}), 'INVALID_ARG');
          await op('latency.setOffset', {'samples': 0});
        },
      );

      tc(
        'latency.calibrate: input tắt → MIC_PERMISSION; đang đo → INVALID_ARG; không tín hiệu → job AUDIO_DEVICE / NO_SIGNAL',
        () async {
          await op('audio.setInputEnabled', {'enabled': false});
          expect(errCode({'op': 'latency.calibrate'}), 'MIC_PERMISSION');
          await op('audio.setInputEnabled', {'enabled': true});
          h.makeCalibrationSilent();
          final id = e().callJob('latency.calibrate', {});
          expect(errCode({'op': 'latency.calibrate'}), 'INVALID_ARG', reason: 'đang đo');
          Map<String, dynamic> r = const {};
          for (var i = 0; i < 600; i++) {
            r = e().callOk('job.result', {'jobId': id});
            if (r['status'] != 'running') break;
            await h.advanceSeconds(0.1);
            await Future<void>.delayed(const Duration(milliseconds: 1));
          }
          expect(r['status'], 'failed');
          final err = (r['error'] as Map).cast<String, dynamic>();
          expect(err['code'], 'AUDIO_DEVICE');
          expect(err['message'], 'NO_SIGNAL');
        },
      );

      tc('link.enable {enabled, startStopSync} → ok, LeState.linkEnabled phản ánh', () async {
        await op('link.enable', {'enabled': true, 'startStopSync': true});
        expect(e().readState().linkEnabled, 1);
        await op('link.enable', {'enabled': false, 'startStopSync': true});
        expect(e().readState().linkEnabled, 0);
      });
    });

    tc('OVERDUB_TOGGLE: Playing ⇄ Overdubbing', () async {
      await clip(0, 0);
      await launch(0, 0);
      await cmd(LeCommandType.LE_CMD_OVERDUB_TOGGLE, track: 0);
      expect(st(0, 0), overdubbing);
      await cmd(LeCommandType.LE_CMD_OVERDUB_TOGGLE, track: 0);
      expect(st(0, 0), playing);
    });

    tc('overdub clip audio xong → clip.info hasUndo = true; clip.undoOverdub → false, clip vẫn phát', () async {
      await cmd(LeCommandType.LE_CMD_CLIP_RECORD, track: 4, slot: 3, i0: 1); // track audio → clip audio 1 bar
      await goTo(4.05);
      await h.advanceSeconds(0.2); // take ghi xong file → sẵn sàng overdub
      await h.settle();
      bool hasUndo() => e().callOk('clip.info', {'track': 4, 'slot': 3})['hasUndo'] == true;
      expect(hasUndo(), isFalse);
      await cmd(LeCommandType.LE_CMD_OVERDUB_TOGGLE, track: 4);
      expect(st(4, 3), overdubbing);
      await h.advanceBeats(1);
      await cmd(LeCommandType.LE_CMD_OVERDUB_TOGGLE, track: 4);
      await h.advanceSeconds(0.1); // engine lưu lượt overdub bất đồng bộ
      await h.settle();
      expect(hasUndo(), isTrue);
      await op('clip.undoOverdub', {'track': 4, 'slot': 3});
      expect(hasUndo(), isFalse);
      expect(st(4, 3), playing);
    });

    tc('clip MIDI: overdub không tạo lớp undo (hasUndo luôn false)', () async {
      await op('track.configure', {'track': 0, 'kind': 'instrument', 'name': 'Keys'});
      await clip(0, 0);
      await launch(0, 0);
      await cmd(LeCommandType.LE_CMD_OVERDUB_TOGGLE, track: 0);
      await h.advanceBeats(1);
      await cmd(LeCommandType.LE_CMD_OVERDUB_TOGGLE, track: 0);
      await h.advanceSeconds(0.1);
      await h.settle();
      expect(e().callOk('clip.info', {'track': 0, 'slot': 0})['hasUndo'], isFalse);
    });

    tc('hết lượt overdub MIDI → RECORDING_FINISHED(track, slot); clip.getMidi có nốt vừa chồng (04 §5.4)', () async {
      await op('track.configure', {'track': 2, 'kind': 'instrument', 'name': 'Keys'});
      await clip(2, 1);
      await launch(2, 1);
      final ev = h.deliversEvents
          ? e().events
                .firstWhere((x) => x is RecordingFinished && x.track == 2 && x.slot == 1)
                .timeout(const Duration(seconds: 5))
          : null;
      await cmd(LeCommandType.LE_CMD_OVERDUB_TOGGLE, track: 2);
      await h.advanceBeats(0.5);
      await cmd(LeCommandType.LE_CMD_NOTE_ON, track: 2, i0: 64, f0: 0.7);
      await h.advanceBeats(0.5);
      await cmd(LeCommandType.LE_CMD_NOTE_OFF, track: 2, i0: 64);
      await cmd(LeCommandType.LE_CMD_OVERDUB_TOGGLE, track: 2);
      await h.advanceSeconds(0.1);
      await h.settle();
      if (ev != null) {
        final r = await ev as RecordingFinished;
        expect([r.track, r.slot], [2, 1]);
      }
      final notes = e().callOk('clip.getMidi', {'track': 2, 'slot': 1})['notes'] as List;
      expect(notes.map((n) => (n as Map)['p']), [64]);
      expect(st(2, 1), playing);
    });

    tc('overdub MIDI: nốt chồng → CLIP_CHANGED(track, slot) ≤ 10 lần/s, luôn một lần cuối trước RECORDING_FINISHED; '
        'clip.getMidi lúc đó đã có nốt', () async {
      await op('track.configure', {'track': 3, 'kind': 'instrument', 'name': 'Keys'});
      await clip(3, 2);
      await launch(3, 2);
      final got = <EngineEvent>[];
      final sub = e().events.listen(got.add);
      await cmd(LeCommandType.LE_CMD_OVERDUB_TOGGLE, track: 3);
      await h.advanceBeats(0.25);
      for (var k = 0; k < 20; k++) {
        await cmd(LeCommandType.LE_CMD_NOTE_ON, track: 3, i0: 60 + k % 12, f0: 0.7);
        await h.advanceSeconds(0.025);
        await cmd(LeCommandType.LE_CMD_NOTE_OFF, track: 3, i0: 60 + k % 12);
        await h.advanceSeconds(0.025);
      }
      final midway = (e().callOk('clip.getMidi', {'track': 3, 'slot': 2})['notes'] as List).length;
      expect(midway, 20, reason: 'đang overdub đã đọc được nốt (app vẽ ngay khi có CLIP_CHANGED)');
      await cmd(LeCommandType.LE_CMD_OVERDUB_TOGGLE, track: 3);
      await h.advanceSeconds(0.1);
      await h.settle();
      await sub.cancel();
      if (!h.deliversEvents) return;
      final changed = [
        for (final x in got)
          if (x is ClipChanged && x.track == 3 && x.slot == 2) x,
      ];
      final finished = got.indexWhere((x) => x is RecordingFinished && x.track == 3 && x.slot == 2);
      expect(changed.length, inInclusiveRange(2, 12), reason: '20 nốt trong 1 s → ≤ 10 lần/s (+ lần cuối)');
      expect(
        finished,
        greaterThan(got.lastIndexOf(changed.last)),
        reason: 'CLIP_CHANGED cuối trước RECORDING_FINISHED',
      );
    });

    tc('project.open reset state RT: 120 BPM, 4/4, quantize 1 bar (thiết lập toàn cục giữ nguyên)', () async {
      await cmd(LeCommandType.LE_CMD_SET_BPM, d0: 90);
      await op('transport.setTimeSignature', {'num': 3, 'den': 4});
      await cmd(LeCommandType.LE_CMD_SET_QUANTIZE, i0: LeQuantize.LE_Q_1_4);
      await op('midi.setRecordQuantize', {'grid': 0.25});
      await op('project.open', {'dir': '/tmp/contract2.loopproj'});
      expect(e().readState().bpm, closeTo(120, 1e-9));
      expect(e().readState().beatsPerBar, 4);
      await clip(0, 0);
      await clip(0, 1);
      await launch(0, 0);
      await goTo(1.5);
      await launch(0, 1);
      expect(st(0, 1), queuedPlay, reason: 'quantize về 1 bar: chờ tới beat 4, không phải 1/4');
      await goTo(4.05);
      expect(st(0, 1), playing);
      await op('midi.setRecordQuantize', {'grid': 0});
    });

    tc('engine.info có inputChannels, nonFiniteSamples (int ≥ 0) và latencyOffsetSamples', () {
      final info = e().callOk('engine.info');
      expect(info['inputChannels'], isA<int>());
      expect(info['nonFiniteSamples'] as int, greaterThanOrEqualTo(0));
      expect(info['latencyOffsetSamples'], isA<int>());
    });

    tc('track.configure {color:"#RRGGBB"} → ok (LED Launchpad); sai dạng → INVALID_ARG', () {
      expect(
        errCode({'op': 'track.configure', 'track': 2, 'kind': 'instrument', 'name': 'K', 'color': '#E5C07B'}),
        isNull,
      );
      for (final bad in ['E5C07B', '#E5C07', '#GGGGGG', 'red']) {
        expect(
          errCode({'op': 'track.configure', 'track': 2, 'kind': 'instrument', 'name': 'K', 'color': bad}),
          'INVALID_ARG',
          reason: bad,
        );
      }
    });

    tc('chuỗi chứa NUL (U+0000) → INVALID_ARG (05 §1); tên có dấu + emoji vẫn ok', () {
      expect(errCode({'op': 'track.configure', 'track': 1, 'kind': 'audio', 'name': 'a\u0000b'}), 'INVALID_ARG');
      expect(errCode({'op': 'track.configure', 'track': 1, 'kind': 'audio', 'name': 'Trống 🥁 ồ'}), isNull);
    });

    tc('job.result / job.cancel: jobId không phải số nguyên → INVALID_ARG; jobId lạ → JOB_NOT_FOUND', () {
      String? code(Map<String, dynamic> r) {
        final res = e().call(r);
        return res['ok'] == true ? null : (res['error'] as Map)['code'] as String?;
      }

      expect(code({'op': 'job.result', 'jobId': 'x'}), 'INVALID_ARG');
      expect(code({'op': 'job.cancel', 'jobId': 1.5}), 'INVALID_ARG');
      expect(code({'op': 'job.result', 'jobId': 987654}), 'JOB_NOT_FOUND');
    });

    tc('lệnh sai track/slot → le_send false', () {
      expect(e().send(LeCommandType.LE_CMD_CLIP_LAUNCH, track: 8, slot: 0), isFalse);
      expect(e().send(LeCommandType.LE_CMD_CLIP_LAUNCH, track: 0, slot: -1), isFalse);
      expect(e().send(LeCommandType.LE_CMD_SCENE_LAUNCH, slot: 9), isFalse);
    });

    tc('clip.clear: ô về Empty, clip đang phát dừng', () async {
      await clip(0, 0);
      await launch(0, 0);
      await op('clip.clear', {'track': 0, 'slot': 0});
      expect(st(0, 0), empty);
      expect(playingSlot(0), -1);
    });
  });
}
