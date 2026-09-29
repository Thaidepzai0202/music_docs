// GENERATED CODE - DO NOT MODIFY BY HAND

part of 'project.dart';

// **************************************************************************
// JsonSerializableGenerator
// **************************************************************************

_Project _$ProjectFromJson(Map<String, dynamic> json) => _Project(
  schemaVersion: (json['schemaVersion'] as num).toInt(),
  id: json['id'] as String,
  name: json['name'] as String,
  createdAt: const UtcDateTimeConverter().fromJson(json['createdAt'] as String),
  modifiedAt: const UtcDateTimeConverter().fromJson(json['modifiedAt'] as String),
  appVersion: json['appVersion'] as String,
  transport: Transport.fromJson(json['transport'] as Map<String, dynamic>),
  scenes:
      (json['scenes'] as List<dynamic>?)?.map((e) => Scene.fromJson(e as Map<String, dynamic>)).toList() ??
      const <Scene>[],
  tracks:
      (json['tracks'] as List<dynamic>?)?.map((e) => Track.fromJson(e as Map<String, dynamic>)).toList() ??
      const <Track>[],
  userInstruments:
      (json['userInstruments'] as List<dynamic>?)
          ?.map((e) => UserInstrument.fromJson(e as Map<String, dynamic>))
          .toList() ??
      const <UserInstrument>[],
  master: json['master'] == null ? const Master() : Master.fromJson(json['master'] as Map<String, dynamic>),
  midiMappings:
      (json['midiMappings'] as List<dynamic>?)?.map((e) => MidiMapping.fromJson(e as Map<String, dynamic>)).toList() ??
      const <MidiMapping>[],
  link: json['link'] == null ? const LinkSettings() : LinkSettings.fromJson(json['link'] as Map<String, dynamic>),
  launchLog:
      (json['launchLog'] as List<dynamic>?)?.map((e) => e as Map<String, dynamic>).toList() ??
      const <Map<String, dynamic>>[],
);

Map<String, dynamic> _$ProjectToJson(_Project instance) => <String, dynamic>{
  'schemaVersion': instance.schemaVersion,
  'id': instance.id,
  'name': instance.name,
  'createdAt': const UtcDateTimeConverter().toJson(instance.createdAt),
  'modifiedAt': const UtcDateTimeConverter().toJson(instance.modifiedAt),
  'appVersion': instance.appVersion,
  'transport': instance.transport.toJson(),
  'scenes': instance.scenes.map((e) => e.toJson()).toList(),
  'tracks': instance.tracks.map((e) => e.toJson()).toList(),
  'userInstruments': instance.userInstruments.map((e) => e.toJson()).toList(),
  'master': instance.master.toJson(),
  'midiMappings': instance.midiMappings.map((e) => e.toJson()).toList(),
  'link': instance.link.toJson(),
  'launchLog': instance.launchLog,
};

_Transport _$TransportFromJson(Map<String, dynamic> json) => _Transport(
  bpm: (json['bpm'] as num?)?.toDouble() ?? 120.0,
  timeSignature:
      (json['timeSignature'] as List<dynamic>?)?.map((e) => (e as num).toInt()).toList() ?? const <int>[4, 4],
  quantize: $enumDecodeNullable(_$QuantizeGridEnumMap, json['quantize']) ?? QuantizeGrid.bar1,
  metronome: json['metronome'] == null
      ? const Metronome()
      : Metronome.fromJson(json['metronome'] as Map<String, dynamic>),
  countInBars: (json['countInBars'] as num?)?.toInt() ?? 1,
);

Map<String, dynamic> _$TransportToJson(_Transport instance) => <String, dynamic>{
  'bpm': instance.bpm,
  'timeSignature': instance.timeSignature,
  'quantize': _$QuantizeGridEnumMap[instance.quantize]!,
  'metronome': instance.metronome.toJson(),
  'countInBars': instance.countInBars,
};

const _$QuantizeGridEnumMap = {
  QuantizeGrid.none: 'none',
  QuantizeGrid.sixteenth: '1/16',
  QuantizeGrid.eighth: '1/8',
  QuantizeGrid.quarter: '1/4',
  QuantizeGrid.half: '1/2',
  QuantizeGrid.bar1: '1bar',
  QuantizeGrid.bar2: '2bar',
  QuantizeGrid.bar4: '4bar',
};

_Metronome _$MetronomeFromJson(Map<String, dynamic> json) => _Metronome(
  mode: $enumDecodeNullable(_$MetronomeModeEnumMap, json['mode']) ?? MetronomeMode.recordOnly,
  volume: (json['volume'] as num?)?.toDouble() ?? 0.7,
);

Map<String, dynamic> _$MetronomeToJson(_Metronome instance) => <String, dynamic>{
  'mode': _$MetronomeModeEnumMap[instance.mode]!,
  'volume': instance.volume,
};

const _$MetronomeModeEnumMap = {
  MetronomeMode.off: 'off',
  MetronomeMode.always: 'on',
  MetronomeMode.recordOnly: 'recordOnly',
};

_Scene _$SceneFromJson(Map<String, dynamic> json) =>
    _Scene(index: (json['index'] as num).toInt(), name: json['name'] as String);

Map<String, dynamic> _$SceneToJson(_Scene instance) => <String, dynamic>{
  'index': instance.index,
  'name': instance.name,
};

_Track _$TrackFromJson(Map<String, dynamic> json) => _Track(
  id: json['id'] as String,
  index: (json['index'] as num).toInt(),
  name: json['name'] as String,
  color: json['color'] as String,
  kind: $enumDecode(_$TrackKindEnumMap, json['kind']),
  instrument: json['instrument'] == null ? null : InstrumentRef.fromJson(json['instrument'] as Map<String, dynamic>),
  mixer: json['mixer'] == null ? const Mixer() : Mixer.fromJson(json['mixer'] as Map<String, dynamic>),
  monitor: $enumDecodeNullable(_$MonitorModeEnumMap, json['monitor']) ?? MonitorMode.off,
  fx:
      (json['fx'] as List<dynamic>?)?.map((e) => FxSlot.fromJson(e as Map<String, dynamic>)).toList() ??
      const <FxSlot>[],
  clips:
      (json['clips'] as List<dynamic>?)?.map((e) => Clip.fromJson(e as Map<String, dynamic>)).toList() ??
      const <Clip>[],
);

Map<String, dynamic> _$TrackToJson(_Track instance) => <String, dynamic>{
  'id': instance.id,
  'index': instance.index,
  'name': instance.name,
  'color': instance.color,
  'kind': _$TrackKindEnumMap[instance.kind]!,
  'instrument': ?instance.instrument?.toJson(),
  'mixer': instance.mixer.toJson(),
  'monitor': _$MonitorModeEnumMap[instance.monitor]!,
  'fx': instance.fx.map((e) => e.toJson()).toList(),
  'clips': instance.clips.map((e) => e.toJson()).toList(),
};

const _$TrackKindEnumMap = {TrackKind.audio: 'audio', TrackKind.instrument: 'instrument'};

const _$MonitorModeEnumMap = {MonitorMode.off: 'off', MonitorMode.auto: 'auto', MonitorMode.always: 'on'};

SfzInstrumentRef _$SfzInstrumentRefFromJson(Map<String, dynamic> json) =>
    SfzInstrumentRef(path: json['path'] as String, $type: json['kind'] as String?);

Map<String, dynamic> _$SfzInstrumentRefToJson(SfzInstrumentRef instance) => <String, dynamic>{
  'path': instance.path,
  'kind': instance.$type,
};

UserInstrumentRef _$UserInstrumentRefFromJson(Map<String, dynamic> json) =>
    UserInstrumentRef(id: json['id'] as String, $type: json['kind'] as String?);

Map<String, dynamic> _$UserInstrumentRefToJson(UserInstrumentRef instance) => <String, dynamic>{
  'id': instance.id,
  'kind': instance.$type,
};

_Mixer _$MixerFromJson(Map<String, dynamic> json) => _Mixer(
  gainDb: (json['gainDb'] as num?)?.toDouble() ?? 0.0,
  pan: (json['pan'] as num?)?.toDouble() ?? 0.0,
  mute: json['mute'] as bool? ?? false,
  solo: json['solo'] as bool? ?? false,
);

Map<String, dynamic> _$MixerToJson(_Mixer instance) => <String, dynamic>{
  'gainDb': instance.gainDb,
  'pan': instance.pan,
  'mute': instance.mute,
  'solo': instance.solo,
};

_FxSlot _$FxSlotFromJson(Map<String, dynamic> json) => _FxSlot(
  type: $enumDecode(_$FxTypeEnumMap, json['type']),
  bypass: json['bypass'] as bool? ?? false,
  params:
      (json['params'] as Map<String, dynamic>?)?.map((k, e) => MapEntry(k, (e as num).toDouble())) ??
      const <String, double>{},
);

Map<String, dynamic> _$FxSlotToJson(_FxSlot instance) => <String, dynamic>{
  'type': _$FxTypeEnumMap[instance.type]!,
  'bypass': instance.bypass,
  'params': instance.params,
};

const _$FxTypeEnumMap = {
  FxType.filter: 'filter',
  FxType.delay: 'delay',
  FxType.reverb: 'reverb',
  FxType.eq3: 'eq3',
  FxType.comp: 'comp',
};

MidiClip _$MidiClipFromJson(Map<String, dynamic> json) => MidiClip(
  slot: (json['slot'] as num).toInt(),
  id: json['id'] as String,
  name: json['name'] as String,
  lengthBeats: (json['lengthBeats'] as num).toDouble(),
  notes:
      (json['notes'] as List<dynamic>?)?.map((e) => Note.fromJson(e as Map<String, dynamic>)).toList() ??
      const <Note>[],
  $type: json['kind'] as String?,
);

Map<String, dynamic> _$MidiClipToJson(MidiClip instance) => <String, dynamic>{
  'slot': instance.slot,
  'id': instance.id,
  'name': instance.name,
  'lengthBeats': instance.lengthBeats,
  'notes': instance.notes.map((e) => e.toJson()).toList(),
  'kind': instance.$type,
};

AudioClip _$AudioClipFromJson(Map<String, dynamic> json) => AudioClip(
  slot: (json['slot'] as num).toInt(),
  id: json['id'] as String,
  name: json['name'] as String,
  file: json['file'] as String,
  lengthBeats: (json['lengthBeats'] as num).toDouble(),
  originalBpm: (json['originalBpm'] as num).toDouble(),
  warp: $enumDecodeNullable(_$WarpModeEnumMap, json['warp']) ?? WarpMode.stretch,
  gainDb: (json['gainDb'] as num?)?.toDouble() ?? 0.0,
  loop: json['loop'] == null ? const AudioLoop() : AudioLoop.fromJson(json['loop'] as Map<String, dynamic>),
  $type: json['kind'] as String?,
);

Map<String, dynamic> _$AudioClipToJson(AudioClip instance) => <String, dynamic>{
  'slot': instance.slot,
  'id': instance.id,
  'name': instance.name,
  'file': instance.file,
  'lengthBeats': instance.lengthBeats,
  'originalBpm': instance.originalBpm,
  'warp': _$WarpModeEnumMap[instance.warp]!,
  'gainDb': instance.gainDb,
  'loop': instance.loop.toJson(),
  'kind': instance.$type,
};

const _$WarpModeEnumMap = {WarpMode.stretch: 'stretch', WarpMode.repitch: 'repitch'};

_Note _$NoteFromJson(Map<String, dynamic> json) => _Note(
  p: (json['p'] as num).toInt(),
  v: (json['v'] as num).toInt(),
  s: (json['s'] as num).toDouble(),
  d: (json['d'] as num).toDouble(),
);

Map<String, dynamic> _$NoteToJson(_Note instance) => <String, dynamic>{
  'p': instance.p,
  'v': instance.v,
  's': instance.s,
  'd': instance.d,
};

_AudioLoop _$AudioLoopFromJson(Map<String, dynamic> json) =>
    _AudioLoop(startSample: (json['startSample'] as num?)?.toInt() ?? 0);

Map<String, dynamic> _$AudioLoopToJson(_AudioLoop instance) => <String, dynamic>{'startSample': instance.startSample};

_UserInstrument _$UserInstrumentFromJson(Map<String, dynamic> json) => _UserInstrument(
  id: json['id'] as String,
  name: json['name'] as String,
  source: json['source'] as String,
  rootNote: (json['rootNote'] as num).toInt(),
  cents: (json['cents'] as num?)?.toDouble() ?? 0.0,
  confidence: (json['confidence'] as num?)?.toDouble() ?? 1.0,
  mode: $enumDecodeNullable(_$InstrumentModeEnumMap, json['mode']) ?? InstrumentMode.natural,
  envelope: json['envelope'] == null ? const Envelope() : Envelope.fromJson(json['envelope'] as Map<String, dynamic>),
);

Map<String, dynamic> _$UserInstrumentToJson(_UserInstrument instance) => <String, dynamic>{
  'id': instance.id,
  'name': instance.name,
  'source': instance.source,
  'rootNote': instance.rootNote,
  'cents': instance.cents,
  'confidence': instance.confidence,
  'mode': _$InstrumentModeEnumMap[instance.mode]!,
  'envelope': instance.envelope.toJson(),
};

const _$InstrumentModeEnumMap = {InstrumentMode.natural: 'natural', InstrumentMode.classic: 'classic'};

_Envelope _$EnvelopeFromJson(Map<String, dynamic> json) => _Envelope(
  a: (json['a'] as num?)?.toDouble() ?? 0.005,
  d: (json['d'] as num?)?.toDouble() ?? 0.2,
  s: (json['s'] as num?)?.toDouble() ?? 0.8,
  r: (json['r'] as num?)?.toDouble() ?? 0.3,
);

Map<String, dynamic> _$EnvelopeToJson(_Envelope instance) => <String, dynamic>{
  'a': instance.a,
  'd': instance.d,
  's': instance.s,
  'r': instance.r,
};

_Master _$MasterFromJson(Map<String, dynamic> json) => _Master(
  gainDb: (json['gainDb'] as num?)?.toDouble() ?? 0.0,
  eq3: (json['eq3'] as List<dynamic>?)?.map((e) => (e as num).toDouble()).toList() ?? const <double>[0, 0, 0],
  limiterCeilingDb: (json['limiterCeilingDb'] as num?)?.toDouble() ?? -0.3,
);

Map<String, dynamic> _$MasterToJson(_Master instance) => <String, dynamic>{
  'gainDb': instance.gainDb,
  'eq3': instance.eq3,
  'limiterCeilingDb': instance.limiterCeilingDb,
};

_MidiMapping _$MidiMappingFromJson(Map<String, dynamic> json) => _MidiMapping(
  src: MidiSource.fromJson(json['src'] as Map<String, dynamic>),
  target: json['target'] as Map<String, dynamic>,
);

Map<String, dynamic> _$MidiMappingToJson(_MidiMapping instance) => <String, dynamic>{
  'src': instance.src.toJson(),
  'target': instance.target,
};

_MidiSource _$MidiSourceFromJson(Map<String, dynamic> json) => _MidiSource(
  device: json['device'] as String,
  kind: json['kind'] as String,
  channel: (json['channel'] as num).toInt(),
  number: (json['number'] as num).toInt(),
);

Map<String, dynamic> _$MidiSourceToJson(_MidiSource instance) => <String, dynamic>{
  'device': instance.device,
  'kind': instance.kind,
  'channel': instance.channel,
  'number': instance.number,
};

_LinkSettings _$LinkSettingsFromJson(Map<String, dynamic> json) =>
    _LinkSettings(enabled: json['enabled'] as bool? ?? false, startStopSync: json['startStopSync'] as bool? ?? true);

Map<String, dynamic> _$LinkSettingsToJson(_LinkSettings instance) => <String, dynamic>{
  'enabled': instance.enabled,
  'startStopSync': instance.startStopSync,
};
