import 'package:freezed_annotation/freezed_annotation.dart';

part 'project.freezed.dart';
part 'project.g.dart';

/// Model project (06 §2, schema v1). Nguồn sự thật nằm ở Dart; engine chỉ giữ bản cần để phát.
/// Bất biến (freezed): mọi thay đổi đi qua `ProjectController` (07 §5).

// ───────────────────────── Enum (giá trị JSON theo 06 §2) ─────────────────────────

enum TrackKind { audio, instrument }

enum MonitorMode {
  off,
  auto,
  @JsonValue('on')
  always;

  /// `LE_CMD_TRACK_MONITOR` i0: 0 off / 1 auto / 2 on.
  int get leValue => index;
}

enum MetronomeMode {
  off,
  @JsonValue('on')
  always,
  recordOnly;

  /// `LE_CMD_METRONOME` i0: 0 tắt / 1 bật / 2 chỉ khi thu.
  int get leValue => index;
}

enum QuantizeGrid {
  @JsonValue('none')
  none,
  @JsonValue('1/16')
  sixteenth,
  @JsonValue('1/8')
  eighth,
  @JsonValue('1/4')
  quarter,
  @JsonValue('1/2')
  half,
  @JsonValue('1bar')
  bar1,
  @JsonValue('2bar')
  bar2,
  @JsonValue('4bar')
  bar4;

  /// `LeQuantize` (LE_Q_NONE = 0 … LE_Q_4_BAR = 7).
  int get leValue => index;
}

enum WarpMode { stretch, repitch }

/// `transport.tempoMode` (04 §2.4–2.5): `firstLoop` = pedal mode, vòng đầu quyết định BPM.
enum TempoMode { fixed, firstLoop }

enum InstrumentMode { natural, classic }

enum FxType { filter, delay, reverb, eq3, comp }

// ───────────────────────── Project ─────────────────────────

@freezed
abstract class Project with _$Project {
  const Project._();

  const factory Project({
    required int schemaVersion,
    required String id,
    required String name,
    @UtcDateTimeConverter() required DateTime createdAt,
    @UtcDateTimeConverter() required DateTime modifiedAt,
    required String appVersion,
    required Transport transport,
    @Default(<Scene>[]) List<Scene> scenes,
    @Default(<Track>[]) List<Track> tracks,
    @Default(<UserInstrument>[]) List<UserInstrument> userInstruments,
    @Default(Master()) Master master,
    @Default(<MidiMapping>[]) List<MidiMapping> midiMappings,
    @Default(LinkSettings()) LinkSettings link,
    @Default(<Map<String, dynamic>>[]) List<Map<String, dynamic>> launchLog,
  }) = _Project;

  factory Project.fromJson(Map<String, dynamic> json) => _$ProjectFromJson(json);

  /// Track ở cột [index] (0..7), null nếu không có.
  Track? trackAt(int index) {
    for (final t in tracks) {
      if (t.index == index) return t;
    }
    return null;
  }
}

@freezed
abstract class Transport with _$Transport {
  const factory Transport({
    @Default(120.0) double bpm,
    @Default(<int>[4, 4]) List<int> timeSignature,
    @Default(QuantizeGrid.bar1) QuantizeGrid quantize,
    @Default(Metronome()) Metronome metronome,
    @Default(1) int countInBars,
    @Default(TempoMode.fixed) TempoMode tempoMode,

    /// Độ dài vòng đầu (beat) của pedal mode: các vòng sau làm tròn lên bội số của nó, kể cả sau khi mở lại project
    /// (05 §3 `transport.setTempoMode {firstLoopBeats}`). null = chưa có vòng đầu.
    double? firstLoopBeats,
  }) = _Transport;

  factory Transport.fromJson(Map<String, dynamic> json) => _$TransportFromJson(json);
}

@freezed
abstract class Metronome with _$Metronome {
  const factory Metronome({@Default(MetronomeMode.recordOnly) MetronomeMode mode, @Default(0.7) double volume}) =
      _Metronome;

  factory Metronome.fromJson(Map<String, dynamic> json) => _$MetronomeFromJson(json);
}

@freezed
abstract class Scene with _$Scene {
  const factory Scene({required int index, required String name}) = _Scene;

  factory Scene.fromJson(Map<String, dynamic> json) => _$SceneFromJson(json);
}

// ───────────────────────── Track ─────────────────────────

@freezed
abstract class Track with _$Track {
  const Track._();

  const factory Track({
    required String id,

    /// Cột 0..7.
    required int index,
    required String name,

    /// "#RRGGBB"
    required String color,
    required TrackKind kind,
    InstrumentRef? instrument,
    @Default(Mixer()) Mixer mixer,
    @Default(MonitorMode.off) MonitorMode monitor,
    @Default(<FxSlot>[]) List<FxSlot> fx,

    /// Chỉ ô có clip mới xuất hiện (06 §2).
    @Default(<Clip>[]) List<Clip> clips,
  }) = _Track;

  factory Track.fromJson(Map<String, dynamic> json) => _$TrackFromJson(json);

  Clip? clipAt(int slot) {
    for (final c in clips) {
      if (c.slot == slot) return c;
    }
    return null;
  }
}

/// Nhạc cụ của track instrument: SFZ trong Library/ hoặc nhạc cụ tự thu (P3).
@Freezed(unionKey: 'kind')
sealed class InstrumentRef with _$InstrumentRef {
  /// [path] tương đối so với `Library/`.
  const factory InstrumentRef.sfz({required String path}) = SfzInstrumentRef;

  /// [id] trỏ tới `Project.userInstruments`.
  const factory InstrumentRef.user({required String id}) = UserInstrumentRef;

  factory InstrumentRef.fromJson(Map<String, dynamic> json) => _$InstrumentRefFromJson(json);
}

@freezed
abstract class Mixer with _$Mixer {
  const factory Mixer({
    @Default(0.0) double gainDb,
    @Default(0.0) double pan,
    @Default(false) bool mute,
    @Default(false) bool solo,
  }) = _Mixer;

  factory Mixer.fromJson(Map<String, dynamic> json) => _$MixerFromJson(json);
}

@freezed
abstract class FxSlot with _$FxSlot {
  const factory FxSlot({
    required FxType type,
    @Default(false) bool bypass,

    /// Key là `paramId` dạng chuỗi (04 §9).
    @Default(<String, double>{}) Map<String, double> params,
  }) = _FxSlot;

  factory FxSlot.fromJson(Map<String, dynamic> json) => _$FxSlotFromJson(json);
}

// ───────────────────────── Clip ─────────────────────────

@Freezed(unionKey: 'kind')
sealed class Clip with _$Clip {
  const Clip._();

  const factory Clip.midi({
    required int slot,
    required String id,
    required String name,
    required double lengthBeats,
    @Default(<Note>[]) List<Note> notes,
  }) = MidiClip;

  const factory Clip.audio({
    required int slot,
    required String id,
    required String name,

    /// Tương đối so với thư mục project, ví dụ `audio/c_<uuid>.caf`.
    required String file,
    required double lengthBeats,
    required double originalBpm,
    @Default(WarpMode.stretch) WarpMode warp,
    @Default(0.0) double gainDb,
    @Default(AudioLoop()) AudioLoop loop,

    /// Tag thư viện (06 §2/§4: id tiếng Anh cố định, vd `drums`) chép khi gán loop; take tự thu `[]`.
    /// Gợi ý "loop trống → Re-Pitch" đọc từ đây (còn sau khi mở lại project).
    @Default(<String>[]) List<String> tags,
  }) = AudioClip;

  factory Clip.fromJson(Map<String, dynamic> json) => _$ClipFromJson(json);
}

/// Nốt MIDI: p = pitch, v = velocity 1–127, s = start (beat), d = duration (beat).
@freezed
abstract class Note with _$Note {
  const factory Note({required int p, required int v, required double s, required double d}) = _Note;

  factory Note.fromJson(Map<String, dynamic> json) => _$NoteFromJson(json);
}

@freezed
abstract class AudioLoop with _$AudioLoop {
  const factory AudioLoop({@Default(0) int startSample}) = _AudioLoop;

  factory AudioLoop.fromJson(Map<String, dynamic> json) => _$AudioLoopFromJson(json);
}

// ───────────────────────── Nhạc cụ tự thu, master, MIDI, Link ─────────────────────────

@freezed
abstract class UserInstrument with _$UserInstrument {
  const factory UserInstrument({
    required String id,
    required String name,

    /// Tương đối so với thư mục project.
    required String source,
    required int rootNote,
    @Default(0.0) double cents,
    @Default(1.0) double confidence,
    @Default(InstrumentMode.natural) InstrumentMode mode,
    @Default(Envelope()) Envelope envelope,
  }) = _UserInstrument;

  factory UserInstrument.fromJson(Map<String, dynamic> json) => _$UserInstrumentFromJson(json);
}

@freezed
abstract class Envelope with _$Envelope {
  const factory Envelope({
    @Default(0.005) double a,
    @Default(0.2) double d,
    @Default(0.8) double s,
    @Default(0.3) double r,
  }) = _Envelope;

  factory Envelope.fromJson(Map<String, dynamic> json) => _$EnvelopeFromJson(json);
}

@freezed
abstract class Master with _$Master {
  const factory Master({
    @Default(0.0) double gainDb,
    @Default(<double>[0, 0, 0]) List<double> eq3,

    /// Bypass EQ3 master (`FX_BYPASS track −1 slot 0`). Limiter không bypass được.
    @Default(false) bool eq3Bypass,
    @Default(-0.3) double limiterCeilingDb,
  }) = _Master;

  factory Master.fromJson(Map<String, dynamic> json) => _$MasterFromJson(json);
}

@freezed
abstract class MidiMapping with _$MidiMapping {
  const factory MidiMapping({
    required MidiSource src,

    /// `{kind:"clip",track,slot}` | `{kind:"fx",track,slot,param}` | … — kiểu cụ thể chốt ở P4,
    /// tạm giữ nguyên Map để không mất dữ liệu khi round-trip.
    required Map<String, dynamic> target,
  }) = _MidiMapping;

  factory MidiMapping.fromJson(Map<String, dynamic> json) => _$MidiMappingFromJson(json);
}

@freezed
abstract class MidiSource with _$MidiSource {
  const factory MidiSource({
    required String device,

    /// "note" | "cc"
    required String kind,
    required int channel,
    required int number,
  }) = _MidiSource;

  factory MidiSource.fromJson(Map<String, dynamic> json) => _$MidiSourceFromJson(json);
}

@freezed
abstract class LinkSettings with _$LinkSettings {
  const factory LinkSettings({@Default(false) bool enabled, @Default(true) bool startStopSync}) = _LinkSettings;

  factory LinkSettings.fromJson(Map<String, dynamic> json) => _$LinkSettingsFromJson(json);
}

// ───────────────────────── Converter ─────────────────────────

/// ISO-8601 UTC. Không ghi phần mili giây khi bằng 0 → `2027-03-20T10:15:00Z` giữ nguyên khi round-trip.
class UtcDateTimeConverter implements JsonConverter<DateTime, String> {
  const UtcDateTimeConverter();

  @override
  DateTime fromJson(String json) => DateTime.parse(json).toUtc();

  @override
  String toJson(DateTime value) {
    final s = value.toUtc().toIso8601String();
    return s.endsWith('.000Z') ? '${s.substring(0, s.length - 5)}Z' : s;
  }
}
