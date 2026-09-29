// GENERATED CODE - DO NOT MODIFY BY HAND
// coverage:ignore-file
// ignore_for_file: type=lint
// ignore_for_file: unused_element, deprecated_member_use, deprecated_member_use_from_same_package, use_function_type_syntax_for_parameters, unnecessary_const, avoid_init_to_null, invalid_override_different_default_values_named, prefer_expression_function_bodies, annotate_overrides, invalid_annotation_target, unnecessary_question_mark

part of 'project.dart';

// **************************************************************************
// FreezedGenerator
// **************************************************************************

// dart format off
T _$identity<T>(T value) => value;

/// @nodoc
mixin _$Project {

 int get schemaVersion; String get id; String get name;@UtcDateTimeConverter() DateTime get createdAt;@UtcDateTimeConverter() DateTime get modifiedAt; String get appVersion; Transport get transport; List<Scene> get scenes; List<Track> get tracks; List<UserInstrument> get userInstruments; Master get master; List<MidiMapping> get midiMappings; LinkSettings get link; List<Map<String, dynamic>> get launchLog;
/// Create a copy of Project
/// with the given fields replaced by the non-null parameter values.
@JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
$ProjectCopyWith<Project> get copyWith => _$ProjectCopyWithImpl<Project>(this as Project, _$identity);

  /// Serializes this Project to a JSON map.
  Map<String, dynamic> toJson();


@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is Project&&(identical(other.schemaVersion, schemaVersion) || other.schemaVersion == schemaVersion)&&(identical(other.id, id) || other.id == id)&&(identical(other.name, name) || other.name == name)&&(identical(other.createdAt, createdAt) || other.createdAt == createdAt)&&(identical(other.modifiedAt, modifiedAt) || other.modifiedAt == modifiedAt)&&(identical(other.appVersion, appVersion) || other.appVersion == appVersion)&&(identical(other.transport, transport) || other.transport == transport)&&const DeepCollectionEquality().equals(other.scenes, scenes)&&const DeepCollectionEquality().equals(other.tracks, tracks)&&const DeepCollectionEquality().equals(other.userInstruments, userInstruments)&&(identical(other.master, master) || other.master == master)&&const DeepCollectionEquality().equals(other.midiMappings, midiMappings)&&(identical(other.link, link) || other.link == link)&&const DeepCollectionEquality().equals(other.launchLog, launchLog));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,schemaVersion,id,name,createdAt,modifiedAt,appVersion,transport,const DeepCollectionEquality().hash(scenes),const DeepCollectionEquality().hash(tracks),const DeepCollectionEquality().hash(userInstruments),master,const DeepCollectionEquality().hash(midiMappings),link,const DeepCollectionEquality().hash(launchLog));

@override
String toString() {
  return 'Project(schemaVersion: $schemaVersion, id: $id, name: $name, createdAt: $createdAt, modifiedAt: $modifiedAt, appVersion: $appVersion, transport: $transport, scenes: $scenes, tracks: $tracks, userInstruments: $userInstruments, master: $master, midiMappings: $midiMappings, link: $link, launchLog: $launchLog)';
}


}

/// @nodoc
abstract mixin class $ProjectCopyWith<$Res>  {
  factory $ProjectCopyWith(Project value, $Res Function(Project) _then) = _$ProjectCopyWithImpl;
@useResult
$Res call({
 int schemaVersion, String id, String name,@UtcDateTimeConverter() DateTime createdAt,@UtcDateTimeConverter() DateTime modifiedAt, String appVersion, Transport transport, List<Scene> scenes, List<Track> tracks, List<UserInstrument> userInstruments, Master master, List<MidiMapping> midiMappings, LinkSettings link, List<Map<String, dynamic>> launchLog
});


$TransportCopyWith<$Res> get transport;$MasterCopyWith<$Res> get master;$LinkSettingsCopyWith<$Res> get link;

}
/// @nodoc
class _$ProjectCopyWithImpl<$Res>
    implements $ProjectCopyWith<$Res> {
  _$ProjectCopyWithImpl(this._self, this._then);

  final Project _self;
  final $Res Function(Project) _then;

/// Create a copy of Project
/// with the given fields replaced by the non-null parameter values.
@pragma('vm:prefer-inline') @override $Res call({Object? schemaVersion = null,Object? id = null,Object? name = null,Object? createdAt = null,Object? modifiedAt = null,Object? appVersion = null,Object? transport = null,Object? scenes = null,Object? tracks = null,Object? userInstruments = null,Object? master = null,Object? midiMappings = null,Object? link = null,Object? launchLog = null,}) {
  return _then(_self.copyWith(
schemaVersion: null == schemaVersion ? _self.schemaVersion : schemaVersion // ignore: cast_nullable_to_non_nullable
as int,id: null == id ? _self.id : id // ignore: cast_nullable_to_non_nullable
as String,name: null == name ? _self.name : name // ignore: cast_nullable_to_non_nullable
as String,createdAt: null == createdAt ? _self.createdAt : createdAt // ignore: cast_nullable_to_non_nullable
as DateTime,modifiedAt: null == modifiedAt ? _self.modifiedAt : modifiedAt // ignore: cast_nullable_to_non_nullable
as DateTime,appVersion: null == appVersion ? _self.appVersion : appVersion // ignore: cast_nullable_to_non_nullable
as String,transport: null == transport ? _self.transport : transport // ignore: cast_nullable_to_non_nullable
as Transport,scenes: null == scenes ? _self.scenes : scenes // ignore: cast_nullable_to_non_nullable
as List<Scene>,tracks: null == tracks ? _self.tracks : tracks // ignore: cast_nullable_to_non_nullable
as List<Track>,userInstruments: null == userInstruments ? _self.userInstruments : userInstruments // ignore: cast_nullable_to_non_nullable
as List<UserInstrument>,master: null == master ? _self.master : master // ignore: cast_nullable_to_non_nullable
as Master,midiMappings: null == midiMappings ? _self.midiMappings : midiMappings // ignore: cast_nullable_to_non_nullable
as List<MidiMapping>,link: null == link ? _self.link : link // ignore: cast_nullable_to_non_nullable
as LinkSettings,launchLog: null == launchLog ? _self.launchLog : launchLog // ignore: cast_nullable_to_non_nullable
as List<Map<String, dynamic>>,
  ));
}
/// Create a copy of Project
/// with the given fields replaced by the non-null parameter values.
@override
@pragma('vm:prefer-inline')
$TransportCopyWith<$Res> get transport {
  
  return $TransportCopyWith<$Res>(_self.transport, (value) {
    return _then(_self.copyWith(transport: value));
  });
}/// Create a copy of Project
/// with the given fields replaced by the non-null parameter values.
@override
@pragma('vm:prefer-inline')
$MasterCopyWith<$Res> get master {
  
  return $MasterCopyWith<$Res>(_self.master, (value) {
    return _then(_self.copyWith(master: value));
  });
}/// Create a copy of Project
/// with the given fields replaced by the non-null parameter values.
@override
@pragma('vm:prefer-inline')
$LinkSettingsCopyWith<$Res> get link {
  
  return $LinkSettingsCopyWith<$Res>(_self.link, (value) {
    return _then(_self.copyWith(link: value));
  });
}
}


/// Adds pattern-matching-related methods to [Project].
extension ProjectPatterns on Project {
/// A variant of `map` that fallback to returning `orElse`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeMap<TResult extends Object?>(TResult Function( _Project value)?  $default,{required TResult orElse(),}){
final _that = this;
switch (_that) {
case _Project() when $default != null:
return $default(_that);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// Callbacks receives the raw object, upcasted.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case final Subclass2 value:
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult map<TResult extends Object?>(TResult Function( _Project value)  $default,){
final _that = this;
switch (_that) {
case _Project():
return $default(_that);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `map` that fallback to returning `null`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? mapOrNull<TResult extends Object?>(TResult? Function( _Project value)?  $default,){
final _that = this;
switch (_that) {
case _Project() when $default != null:
return $default(_that);case _:
  return null;

}
}
/// A variant of `when` that fallback to an `orElse` callback.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeWhen<TResult extends Object?>(TResult Function( int schemaVersion,  String id,  String name, @UtcDateTimeConverter()  DateTime createdAt, @UtcDateTimeConverter()  DateTime modifiedAt,  String appVersion,  Transport transport,  List<Scene> scenes,  List<Track> tracks,  List<UserInstrument> userInstruments,  Master master,  List<MidiMapping> midiMappings,  LinkSettings link,  List<Map<String, dynamic>> launchLog)?  $default,{required TResult orElse(),}) {final _that = this;
switch (_that) {
case _Project() when $default != null:
return $default(_that.schemaVersion,_that.id,_that.name,_that.createdAt,_that.modifiedAt,_that.appVersion,_that.transport,_that.scenes,_that.tracks,_that.userInstruments,_that.master,_that.midiMappings,_that.link,_that.launchLog);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// As opposed to `map`, this offers destructuring.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case Subclass2(:final field2):
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult when<TResult extends Object?>(TResult Function( int schemaVersion,  String id,  String name, @UtcDateTimeConverter()  DateTime createdAt, @UtcDateTimeConverter()  DateTime modifiedAt,  String appVersion,  Transport transport,  List<Scene> scenes,  List<Track> tracks,  List<UserInstrument> userInstruments,  Master master,  List<MidiMapping> midiMappings,  LinkSettings link,  List<Map<String, dynamic>> launchLog)  $default,) {final _that = this;
switch (_that) {
case _Project():
return $default(_that.schemaVersion,_that.id,_that.name,_that.createdAt,_that.modifiedAt,_that.appVersion,_that.transport,_that.scenes,_that.tracks,_that.userInstruments,_that.master,_that.midiMappings,_that.link,_that.launchLog);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `when` that fallback to returning `null`
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? whenOrNull<TResult extends Object?>(TResult? Function( int schemaVersion,  String id,  String name, @UtcDateTimeConverter()  DateTime createdAt, @UtcDateTimeConverter()  DateTime modifiedAt,  String appVersion,  Transport transport,  List<Scene> scenes,  List<Track> tracks,  List<UserInstrument> userInstruments,  Master master,  List<MidiMapping> midiMappings,  LinkSettings link,  List<Map<String, dynamic>> launchLog)?  $default,) {final _that = this;
switch (_that) {
case _Project() when $default != null:
return $default(_that.schemaVersion,_that.id,_that.name,_that.createdAt,_that.modifiedAt,_that.appVersion,_that.transport,_that.scenes,_that.tracks,_that.userInstruments,_that.master,_that.midiMappings,_that.link,_that.launchLog);case _:
  return null;

}
}

}

/// @nodoc
@JsonSerializable()

class _Project extends Project {
  const _Project({required this.schemaVersion, required this.id, required this.name, @UtcDateTimeConverter() required this.createdAt, @UtcDateTimeConverter() required this.modifiedAt, required this.appVersion, required this.transport, final  List<Scene> scenes = const <Scene>[], final  List<Track> tracks = const <Track>[], final  List<UserInstrument> userInstruments = const <UserInstrument>[], this.master = const Master(), final  List<MidiMapping> midiMappings = const <MidiMapping>[], this.link = const LinkSettings(), final  List<Map<String, dynamic>> launchLog = const <Map<String, dynamic>>[]}): _scenes = scenes,_tracks = tracks,_userInstruments = userInstruments,_midiMappings = midiMappings,_launchLog = launchLog,super._();
  factory _Project.fromJson(Map<String, dynamic> json) => _$ProjectFromJson(json);

@override final  int schemaVersion;
@override final  String id;
@override final  String name;
@override@UtcDateTimeConverter() final  DateTime createdAt;
@override@UtcDateTimeConverter() final  DateTime modifiedAt;
@override final  String appVersion;
@override final  Transport transport;
 final  List<Scene> _scenes;
@override@JsonKey() List<Scene> get scenes {
  if (_scenes is EqualUnmodifiableListView) return _scenes;
  // ignore: implicit_dynamic_type
  return EqualUnmodifiableListView(_scenes);
}

 final  List<Track> _tracks;
@override@JsonKey() List<Track> get tracks {
  if (_tracks is EqualUnmodifiableListView) return _tracks;
  // ignore: implicit_dynamic_type
  return EqualUnmodifiableListView(_tracks);
}

 final  List<UserInstrument> _userInstruments;
@override@JsonKey() List<UserInstrument> get userInstruments {
  if (_userInstruments is EqualUnmodifiableListView) return _userInstruments;
  // ignore: implicit_dynamic_type
  return EqualUnmodifiableListView(_userInstruments);
}

@override@JsonKey() final  Master master;
 final  List<MidiMapping> _midiMappings;
@override@JsonKey() List<MidiMapping> get midiMappings {
  if (_midiMappings is EqualUnmodifiableListView) return _midiMappings;
  // ignore: implicit_dynamic_type
  return EqualUnmodifiableListView(_midiMappings);
}

@override@JsonKey() final  LinkSettings link;
 final  List<Map<String, dynamic>> _launchLog;
@override@JsonKey() List<Map<String, dynamic>> get launchLog {
  if (_launchLog is EqualUnmodifiableListView) return _launchLog;
  // ignore: implicit_dynamic_type
  return EqualUnmodifiableListView(_launchLog);
}


/// Create a copy of Project
/// with the given fields replaced by the non-null parameter values.
@override @JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
_$ProjectCopyWith<_Project> get copyWith => __$ProjectCopyWithImpl<_Project>(this, _$identity);

@override
Map<String, dynamic> toJson() {
  return _$ProjectToJson(this, );
}

@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is _Project&&(identical(other.schemaVersion, schemaVersion) || other.schemaVersion == schemaVersion)&&(identical(other.id, id) || other.id == id)&&(identical(other.name, name) || other.name == name)&&(identical(other.createdAt, createdAt) || other.createdAt == createdAt)&&(identical(other.modifiedAt, modifiedAt) || other.modifiedAt == modifiedAt)&&(identical(other.appVersion, appVersion) || other.appVersion == appVersion)&&(identical(other.transport, transport) || other.transport == transport)&&const DeepCollectionEquality().equals(other._scenes, _scenes)&&const DeepCollectionEquality().equals(other._tracks, _tracks)&&const DeepCollectionEquality().equals(other._userInstruments, _userInstruments)&&(identical(other.master, master) || other.master == master)&&const DeepCollectionEquality().equals(other._midiMappings, _midiMappings)&&(identical(other.link, link) || other.link == link)&&const DeepCollectionEquality().equals(other._launchLog, _launchLog));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,schemaVersion,id,name,createdAt,modifiedAt,appVersion,transport,const DeepCollectionEquality().hash(_scenes),const DeepCollectionEquality().hash(_tracks),const DeepCollectionEquality().hash(_userInstruments),master,const DeepCollectionEquality().hash(_midiMappings),link,const DeepCollectionEquality().hash(_launchLog));

@override
String toString() {
  return 'Project(schemaVersion: $schemaVersion, id: $id, name: $name, createdAt: $createdAt, modifiedAt: $modifiedAt, appVersion: $appVersion, transport: $transport, scenes: $scenes, tracks: $tracks, userInstruments: $userInstruments, master: $master, midiMappings: $midiMappings, link: $link, launchLog: $launchLog)';
}


}

/// @nodoc
abstract mixin class _$ProjectCopyWith<$Res> implements $ProjectCopyWith<$Res> {
  factory _$ProjectCopyWith(_Project value, $Res Function(_Project) _then) = __$ProjectCopyWithImpl;
@override @useResult
$Res call({
 int schemaVersion, String id, String name,@UtcDateTimeConverter() DateTime createdAt,@UtcDateTimeConverter() DateTime modifiedAt, String appVersion, Transport transport, List<Scene> scenes, List<Track> tracks, List<UserInstrument> userInstruments, Master master, List<MidiMapping> midiMappings, LinkSettings link, List<Map<String, dynamic>> launchLog
});


@override $TransportCopyWith<$Res> get transport;@override $MasterCopyWith<$Res> get master;@override $LinkSettingsCopyWith<$Res> get link;

}
/// @nodoc
class __$ProjectCopyWithImpl<$Res>
    implements _$ProjectCopyWith<$Res> {
  __$ProjectCopyWithImpl(this._self, this._then);

  final _Project _self;
  final $Res Function(_Project) _then;

/// Create a copy of Project
/// with the given fields replaced by the non-null parameter values.
@override @pragma('vm:prefer-inline') $Res call({Object? schemaVersion = null,Object? id = null,Object? name = null,Object? createdAt = null,Object? modifiedAt = null,Object? appVersion = null,Object? transport = null,Object? scenes = null,Object? tracks = null,Object? userInstruments = null,Object? master = null,Object? midiMappings = null,Object? link = null,Object? launchLog = null,}) {
  return _then(_Project(
schemaVersion: null == schemaVersion ? _self.schemaVersion : schemaVersion // ignore: cast_nullable_to_non_nullable
as int,id: null == id ? _self.id : id // ignore: cast_nullable_to_non_nullable
as String,name: null == name ? _self.name : name // ignore: cast_nullable_to_non_nullable
as String,createdAt: null == createdAt ? _self.createdAt : createdAt // ignore: cast_nullable_to_non_nullable
as DateTime,modifiedAt: null == modifiedAt ? _self.modifiedAt : modifiedAt // ignore: cast_nullable_to_non_nullable
as DateTime,appVersion: null == appVersion ? _self.appVersion : appVersion // ignore: cast_nullable_to_non_nullable
as String,transport: null == transport ? _self.transport : transport // ignore: cast_nullable_to_non_nullable
as Transport,scenes: null == scenes ? _self._scenes : scenes // ignore: cast_nullable_to_non_nullable
as List<Scene>,tracks: null == tracks ? _self._tracks : tracks // ignore: cast_nullable_to_non_nullable
as List<Track>,userInstruments: null == userInstruments ? _self._userInstruments : userInstruments // ignore: cast_nullable_to_non_nullable
as List<UserInstrument>,master: null == master ? _self.master : master // ignore: cast_nullable_to_non_nullable
as Master,midiMappings: null == midiMappings ? _self._midiMappings : midiMappings // ignore: cast_nullable_to_non_nullable
as List<MidiMapping>,link: null == link ? _self.link : link // ignore: cast_nullable_to_non_nullable
as LinkSettings,launchLog: null == launchLog ? _self._launchLog : launchLog // ignore: cast_nullable_to_non_nullable
as List<Map<String, dynamic>>,
  ));
}

/// Create a copy of Project
/// with the given fields replaced by the non-null parameter values.
@override
@pragma('vm:prefer-inline')
$TransportCopyWith<$Res> get transport {
  
  return $TransportCopyWith<$Res>(_self.transport, (value) {
    return _then(_self.copyWith(transport: value));
  });
}/// Create a copy of Project
/// with the given fields replaced by the non-null parameter values.
@override
@pragma('vm:prefer-inline')
$MasterCopyWith<$Res> get master {
  
  return $MasterCopyWith<$Res>(_self.master, (value) {
    return _then(_self.copyWith(master: value));
  });
}/// Create a copy of Project
/// with the given fields replaced by the non-null parameter values.
@override
@pragma('vm:prefer-inline')
$LinkSettingsCopyWith<$Res> get link {
  
  return $LinkSettingsCopyWith<$Res>(_self.link, (value) {
    return _then(_self.copyWith(link: value));
  });
}
}


/// @nodoc
mixin _$Transport {

 double get bpm; List<int> get timeSignature; QuantizeGrid get quantize; Metronome get metronome; int get countInBars; TempoMode get tempoMode;/// Độ dài vòng đầu (beat) của pedal mode: các vòng sau làm tròn lên bội số của nó, kể cả sau khi mở lại project
/// (05 §3 `transport.setTempoMode {firstLoopBeats}`). null = chưa có vòng đầu.
 double? get firstLoopBeats;
/// Create a copy of Transport
/// with the given fields replaced by the non-null parameter values.
@JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
$TransportCopyWith<Transport> get copyWith => _$TransportCopyWithImpl<Transport>(this as Transport, _$identity);

  /// Serializes this Transport to a JSON map.
  Map<String, dynamic> toJson();


@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is Transport&&(identical(other.bpm, bpm) || other.bpm == bpm)&&const DeepCollectionEquality().equals(other.timeSignature, timeSignature)&&(identical(other.quantize, quantize) || other.quantize == quantize)&&(identical(other.metronome, metronome) || other.metronome == metronome)&&(identical(other.countInBars, countInBars) || other.countInBars == countInBars)&&(identical(other.tempoMode, tempoMode) || other.tempoMode == tempoMode)&&(identical(other.firstLoopBeats, firstLoopBeats) || other.firstLoopBeats == firstLoopBeats));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,bpm,const DeepCollectionEquality().hash(timeSignature),quantize,metronome,countInBars,tempoMode,firstLoopBeats);

@override
String toString() {
  return 'Transport(bpm: $bpm, timeSignature: $timeSignature, quantize: $quantize, metronome: $metronome, countInBars: $countInBars, tempoMode: $tempoMode, firstLoopBeats: $firstLoopBeats)';
}


}

/// @nodoc
abstract mixin class $TransportCopyWith<$Res>  {
  factory $TransportCopyWith(Transport value, $Res Function(Transport) _then) = _$TransportCopyWithImpl;
@useResult
$Res call({
 double bpm, List<int> timeSignature, QuantizeGrid quantize, Metronome metronome, int countInBars, TempoMode tempoMode, double? firstLoopBeats
});


$MetronomeCopyWith<$Res> get metronome;

}
/// @nodoc
class _$TransportCopyWithImpl<$Res>
    implements $TransportCopyWith<$Res> {
  _$TransportCopyWithImpl(this._self, this._then);

  final Transport _self;
  final $Res Function(Transport) _then;

/// Create a copy of Transport
/// with the given fields replaced by the non-null parameter values.
@pragma('vm:prefer-inline') @override $Res call({Object? bpm = null,Object? timeSignature = null,Object? quantize = null,Object? metronome = null,Object? countInBars = null,Object? tempoMode = null,Object? firstLoopBeats = freezed,}) {
  return _then(_self.copyWith(
bpm: null == bpm ? _self.bpm : bpm // ignore: cast_nullable_to_non_nullable
as double,timeSignature: null == timeSignature ? _self.timeSignature : timeSignature // ignore: cast_nullable_to_non_nullable
as List<int>,quantize: null == quantize ? _self.quantize : quantize // ignore: cast_nullable_to_non_nullable
as QuantizeGrid,metronome: null == metronome ? _self.metronome : metronome // ignore: cast_nullable_to_non_nullable
as Metronome,countInBars: null == countInBars ? _self.countInBars : countInBars // ignore: cast_nullable_to_non_nullable
as int,tempoMode: null == tempoMode ? _self.tempoMode : tempoMode // ignore: cast_nullable_to_non_nullable
as TempoMode,firstLoopBeats: freezed == firstLoopBeats ? _self.firstLoopBeats : firstLoopBeats // ignore: cast_nullable_to_non_nullable
as double?,
  ));
}
/// Create a copy of Transport
/// with the given fields replaced by the non-null parameter values.
@override
@pragma('vm:prefer-inline')
$MetronomeCopyWith<$Res> get metronome {
  
  return $MetronomeCopyWith<$Res>(_self.metronome, (value) {
    return _then(_self.copyWith(metronome: value));
  });
}
}


/// Adds pattern-matching-related methods to [Transport].
extension TransportPatterns on Transport {
/// A variant of `map` that fallback to returning `orElse`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeMap<TResult extends Object?>(TResult Function( _Transport value)?  $default,{required TResult orElse(),}){
final _that = this;
switch (_that) {
case _Transport() when $default != null:
return $default(_that);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// Callbacks receives the raw object, upcasted.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case final Subclass2 value:
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult map<TResult extends Object?>(TResult Function( _Transport value)  $default,){
final _that = this;
switch (_that) {
case _Transport():
return $default(_that);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `map` that fallback to returning `null`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? mapOrNull<TResult extends Object?>(TResult? Function( _Transport value)?  $default,){
final _that = this;
switch (_that) {
case _Transport() when $default != null:
return $default(_that);case _:
  return null;

}
}
/// A variant of `when` that fallback to an `orElse` callback.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeWhen<TResult extends Object?>(TResult Function( double bpm,  List<int> timeSignature,  QuantizeGrid quantize,  Metronome metronome,  int countInBars,  TempoMode tempoMode,  double? firstLoopBeats)?  $default,{required TResult orElse(),}) {final _that = this;
switch (_that) {
case _Transport() when $default != null:
return $default(_that.bpm,_that.timeSignature,_that.quantize,_that.metronome,_that.countInBars,_that.tempoMode,_that.firstLoopBeats);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// As opposed to `map`, this offers destructuring.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case Subclass2(:final field2):
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult when<TResult extends Object?>(TResult Function( double bpm,  List<int> timeSignature,  QuantizeGrid quantize,  Metronome metronome,  int countInBars,  TempoMode tempoMode,  double? firstLoopBeats)  $default,) {final _that = this;
switch (_that) {
case _Transport():
return $default(_that.bpm,_that.timeSignature,_that.quantize,_that.metronome,_that.countInBars,_that.tempoMode,_that.firstLoopBeats);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `when` that fallback to returning `null`
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? whenOrNull<TResult extends Object?>(TResult? Function( double bpm,  List<int> timeSignature,  QuantizeGrid quantize,  Metronome metronome,  int countInBars,  TempoMode tempoMode,  double? firstLoopBeats)?  $default,) {final _that = this;
switch (_that) {
case _Transport() when $default != null:
return $default(_that.bpm,_that.timeSignature,_that.quantize,_that.metronome,_that.countInBars,_that.tempoMode,_that.firstLoopBeats);case _:
  return null;

}
}

}

/// @nodoc
@JsonSerializable()

class _Transport implements Transport {
  const _Transport({this.bpm = 120.0, final  List<int> timeSignature = const <int>[4, 4], this.quantize = QuantizeGrid.bar1, this.metronome = const Metronome(), this.countInBars = 1, this.tempoMode = TempoMode.fixed, this.firstLoopBeats}): _timeSignature = timeSignature;
  factory _Transport.fromJson(Map<String, dynamic> json) => _$TransportFromJson(json);

@override@JsonKey() final  double bpm;
 final  List<int> _timeSignature;
@override@JsonKey() List<int> get timeSignature {
  if (_timeSignature is EqualUnmodifiableListView) return _timeSignature;
  // ignore: implicit_dynamic_type
  return EqualUnmodifiableListView(_timeSignature);
}

@override@JsonKey() final  QuantizeGrid quantize;
@override@JsonKey() final  Metronome metronome;
@override@JsonKey() final  int countInBars;
@override@JsonKey() final  TempoMode tempoMode;
/// Độ dài vòng đầu (beat) của pedal mode: các vòng sau làm tròn lên bội số của nó, kể cả sau khi mở lại project
/// (05 §3 `transport.setTempoMode {firstLoopBeats}`). null = chưa có vòng đầu.
@override final  double? firstLoopBeats;

/// Create a copy of Transport
/// with the given fields replaced by the non-null parameter values.
@override @JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
_$TransportCopyWith<_Transport> get copyWith => __$TransportCopyWithImpl<_Transport>(this, _$identity);

@override
Map<String, dynamic> toJson() {
  return _$TransportToJson(this, );
}

@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is _Transport&&(identical(other.bpm, bpm) || other.bpm == bpm)&&const DeepCollectionEquality().equals(other._timeSignature, _timeSignature)&&(identical(other.quantize, quantize) || other.quantize == quantize)&&(identical(other.metronome, metronome) || other.metronome == metronome)&&(identical(other.countInBars, countInBars) || other.countInBars == countInBars)&&(identical(other.tempoMode, tempoMode) || other.tempoMode == tempoMode)&&(identical(other.firstLoopBeats, firstLoopBeats) || other.firstLoopBeats == firstLoopBeats));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,bpm,const DeepCollectionEquality().hash(_timeSignature),quantize,metronome,countInBars,tempoMode,firstLoopBeats);

@override
String toString() {
  return 'Transport(bpm: $bpm, timeSignature: $timeSignature, quantize: $quantize, metronome: $metronome, countInBars: $countInBars, tempoMode: $tempoMode, firstLoopBeats: $firstLoopBeats)';
}


}

/// @nodoc
abstract mixin class _$TransportCopyWith<$Res> implements $TransportCopyWith<$Res> {
  factory _$TransportCopyWith(_Transport value, $Res Function(_Transport) _then) = __$TransportCopyWithImpl;
@override @useResult
$Res call({
 double bpm, List<int> timeSignature, QuantizeGrid quantize, Metronome metronome, int countInBars, TempoMode tempoMode, double? firstLoopBeats
});


@override $MetronomeCopyWith<$Res> get metronome;

}
/// @nodoc
class __$TransportCopyWithImpl<$Res>
    implements _$TransportCopyWith<$Res> {
  __$TransportCopyWithImpl(this._self, this._then);

  final _Transport _self;
  final $Res Function(_Transport) _then;

/// Create a copy of Transport
/// with the given fields replaced by the non-null parameter values.
@override @pragma('vm:prefer-inline') $Res call({Object? bpm = null,Object? timeSignature = null,Object? quantize = null,Object? metronome = null,Object? countInBars = null,Object? tempoMode = null,Object? firstLoopBeats = freezed,}) {
  return _then(_Transport(
bpm: null == bpm ? _self.bpm : bpm // ignore: cast_nullable_to_non_nullable
as double,timeSignature: null == timeSignature ? _self._timeSignature : timeSignature // ignore: cast_nullable_to_non_nullable
as List<int>,quantize: null == quantize ? _self.quantize : quantize // ignore: cast_nullable_to_non_nullable
as QuantizeGrid,metronome: null == metronome ? _self.metronome : metronome // ignore: cast_nullable_to_non_nullable
as Metronome,countInBars: null == countInBars ? _self.countInBars : countInBars // ignore: cast_nullable_to_non_nullable
as int,tempoMode: null == tempoMode ? _self.tempoMode : tempoMode // ignore: cast_nullable_to_non_nullable
as TempoMode,firstLoopBeats: freezed == firstLoopBeats ? _self.firstLoopBeats : firstLoopBeats // ignore: cast_nullable_to_non_nullable
as double?,
  ));
}

/// Create a copy of Transport
/// with the given fields replaced by the non-null parameter values.
@override
@pragma('vm:prefer-inline')
$MetronomeCopyWith<$Res> get metronome {
  
  return $MetronomeCopyWith<$Res>(_self.metronome, (value) {
    return _then(_self.copyWith(metronome: value));
  });
}
}


/// @nodoc
mixin _$Metronome {

 MetronomeMode get mode; double get volume;
/// Create a copy of Metronome
/// with the given fields replaced by the non-null parameter values.
@JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
$MetronomeCopyWith<Metronome> get copyWith => _$MetronomeCopyWithImpl<Metronome>(this as Metronome, _$identity);

  /// Serializes this Metronome to a JSON map.
  Map<String, dynamic> toJson();


@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is Metronome&&(identical(other.mode, mode) || other.mode == mode)&&(identical(other.volume, volume) || other.volume == volume));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,mode,volume);

@override
String toString() {
  return 'Metronome(mode: $mode, volume: $volume)';
}


}

/// @nodoc
abstract mixin class $MetronomeCopyWith<$Res>  {
  factory $MetronomeCopyWith(Metronome value, $Res Function(Metronome) _then) = _$MetronomeCopyWithImpl;
@useResult
$Res call({
 MetronomeMode mode, double volume
});




}
/// @nodoc
class _$MetronomeCopyWithImpl<$Res>
    implements $MetronomeCopyWith<$Res> {
  _$MetronomeCopyWithImpl(this._self, this._then);

  final Metronome _self;
  final $Res Function(Metronome) _then;

/// Create a copy of Metronome
/// with the given fields replaced by the non-null parameter values.
@pragma('vm:prefer-inline') @override $Res call({Object? mode = null,Object? volume = null,}) {
  return _then(_self.copyWith(
mode: null == mode ? _self.mode : mode // ignore: cast_nullable_to_non_nullable
as MetronomeMode,volume: null == volume ? _self.volume : volume // ignore: cast_nullable_to_non_nullable
as double,
  ));
}

}


/// Adds pattern-matching-related methods to [Metronome].
extension MetronomePatterns on Metronome {
/// A variant of `map` that fallback to returning `orElse`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeMap<TResult extends Object?>(TResult Function( _Metronome value)?  $default,{required TResult orElse(),}){
final _that = this;
switch (_that) {
case _Metronome() when $default != null:
return $default(_that);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// Callbacks receives the raw object, upcasted.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case final Subclass2 value:
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult map<TResult extends Object?>(TResult Function( _Metronome value)  $default,){
final _that = this;
switch (_that) {
case _Metronome():
return $default(_that);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `map` that fallback to returning `null`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? mapOrNull<TResult extends Object?>(TResult? Function( _Metronome value)?  $default,){
final _that = this;
switch (_that) {
case _Metronome() when $default != null:
return $default(_that);case _:
  return null;

}
}
/// A variant of `when` that fallback to an `orElse` callback.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeWhen<TResult extends Object?>(TResult Function( MetronomeMode mode,  double volume)?  $default,{required TResult orElse(),}) {final _that = this;
switch (_that) {
case _Metronome() when $default != null:
return $default(_that.mode,_that.volume);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// As opposed to `map`, this offers destructuring.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case Subclass2(:final field2):
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult when<TResult extends Object?>(TResult Function( MetronomeMode mode,  double volume)  $default,) {final _that = this;
switch (_that) {
case _Metronome():
return $default(_that.mode,_that.volume);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `when` that fallback to returning `null`
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? whenOrNull<TResult extends Object?>(TResult? Function( MetronomeMode mode,  double volume)?  $default,) {final _that = this;
switch (_that) {
case _Metronome() when $default != null:
return $default(_that.mode,_that.volume);case _:
  return null;

}
}

}

/// @nodoc
@JsonSerializable()

class _Metronome implements Metronome {
  const _Metronome({this.mode = MetronomeMode.recordOnly, this.volume = 0.7});
  factory _Metronome.fromJson(Map<String, dynamic> json) => _$MetronomeFromJson(json);

@override@JsonKey() final  MetronomeMode mode;
@override@JsonKey() final  double volume;

/// Create a copy of Metronome
/// with the given fields replaced by the non-null parameter values.
@override @JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
_$MetronomeCopyWith<_Metronome> get copyWith => __$MetronomeCopyWithImpl<_Metronome>(this, _$identity);

@override
Map<String, dynamic> toJson() {
  return _$MetronomeToJson(this, );
}

@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is _Metronome&&(identical(other.mode, mode) || other.mode == mode)&&(identical(other.volume, volume) || other.volume == volume));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,mode,volume);

@override
String toString() {
  return 'Metronome(mode: $mode, volume: $volume)';
}


}

/// @nodoc
abstract mixin class _$MetronomeCopyWith<$Res> implements $MetronomeCopyWith<$Res> {
  factory _$MetronomeCopyWith(_Metronome value, $Res Function(_Metronome) _then) = __$MetronomeCopyWithImpl;
@override @useResult
$Res call({
 MetronomeMode mode, double volume
});




}
/// @nodoc
class __$MetronomeCopyWithImpl<$Res>
    implements _$MetronomeCopyWith<$Res> {
  __$MetronomeCopyWithImpl(this._self, this._then);

  final _Metronome _self;
  final $Res Function(_Metronome) _then;

/// Create a copy of Metronome
/// with the given fields replaced by the non-null parameter values.
@override @pragma('vm:prefer-inline') $Res call({Object? mode = null,Object? volume = null,}) {
  return _then(_Metronome(
mode: null == mode ? _self.mode : mode // ignore: cast_nullable_to_non_nullable
as MetronomeMode,volume: null == volume ? _self.volume : volume // ignore: cast_nullable_to_non_nullable
as double,
  ));
}


}


/// @nodoc
mixin _$Scene {

 int get index; String get name;
/// Create a copy of Scene
/// with the given fields replaced by the non-null parameter values.
@JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
$SceneCopyWith<Scene> get copyWith => _$SceneCopyWithImpl<Scene>(this as Scene, _$identity);

  /// Serializes this Scene to a JSON map.
  Map<String, dynamic> toJson();


@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is Scene&&(identical(other.index, index) || other.index == index)&&(identical(other.name, name) || other.name == name));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,index,name);

@override
String toString() {
  return 'Scene(index: $index, name: $name)';
}


}

/// @nodoc
abstract mixin class $SceneCopyWith<$Res>  {
  factory $SceneCopyWith(Scene value, $Res Function(Scene) _then) = _$SceneCopyWithImpl;
@useResult
$Res call({
 int index, String name
});




}
/// @nodoc
class _$SceneCopyWithImpl<$Res>
    implements $SceneCopyWith<$Res> {
  _$SceneCopyWithImpl(this._self, this._then);

  final Scene _self;
  final $Res Function(Scene) _then;

/// Create a copy of Scene
/// with the given fields replaced by the non-null parameter values.
@pragma('vm:prefer-inline') @override $Res call({Object? index = null,Object? name = null,}) {
  return _then(_self.copyWith(
index: null == index ? _self.index : index // ignore: cast_nullable_to_non_nullable
as int,name: null == name ? _self.name : name // ignore: cast_nullable_to_non_nullable
as String,
  ));
}

}


/// Adds pattern-matching-related methods to [Scene].
extension ScenePatterns on Scene {
/// A variant of `map` that fallback to returning `orElse`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeMap<TResult extends Object?>(TResult Function( _Scene value)?  $default,{required TResult orElse(),}){
final _that = this;
switch (_that) {
case _Scene() when $default != null:
return $default(_that);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// Callbacks receives the raw object, upcasted.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case final Subclass2 value:
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult map<TResult extends Object?>(TResult Function( _Scene value)  $default,){
final _that = this;
switch (_that) {
case _Scene():
return $default(_that);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `map` that fallback to returning `null`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? mapOrNull<TResult extends Object?>(TResult? Function( _Scene value)?  $default,){
final _that = this;
switch (_that) {
case _Scene() when $default != null:
return $default(_that);case _:
  return null;

}
}
/// A variant of `when` that fallback to an `orElse` callback.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeWhen<TResult extends Object?>(TResult Function( int index,  String name)?  $default,{required TResult orElse(),}) {final _that = this;
switch (_that) {
case _Scene() when $default != null:
return $default(_that.index,_that.name);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// As opposed to `map`, this offers destructuring.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case Subclass2(:final field2):
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult when<TResult extends Object?>(TResult Function( int index,  String name)  $default,) {final _that = this;
switch (_that) {
case _Scene():
return $default(_that.index,_that.name);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `when` that fallback to returning `null`
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? whenOrNull<TResult extends Object?>(TResult? Function( int index,  String name)?  $default,) {final _that = this;
switch (_that) {
case _Scene() when $default != null:
return $default(_that.index,_that.name);case _:
  return null;

}
}

}

/// @nodoc
@JsonSerializable()

class _Scene implements Scene {
  const _Scene({required this.index, required this.name});
  factory _Scene.fromJson(Map<String, dynamic> json) => _$SceneFromJson(json);

@override final  int index;
@override final  String name;

/// Create a copy of Scene
/// with the given fields replaced by the non-null parameter values.
@override @JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
_$SceneCopyWith<_Scene> get copyWith => __$SceneCopyWithImpl<_Scene>(this, _$identity);

@override
Map<String, dynamic> toJson() {
  return _$SceneToJson(this, );
}

@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is _Scene&&(identical(other.index, index) || other.index == index)&&(identical(other.name, name) || other.name == name));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,index,name);

@override
String toString() {
  return 'Scene(index: $index, name: $name)';
}


}

/// @nodoc
abstract mixin class _$SceneCopyWith<$Res> implements $SceneCopyWith<$Res> {
  factory _$SceneCopyWith(_Scene value, $Res Function(_Scene) _then) = __$SceneCopyWithImpl;
@override @useResult
$Res call({
 int index, String name
});




}
/// @nodoc
class __$SceneCopyWithImpl<$Res>
    implements _$SceneCopyWith<$Res> {
  __$SceneCopyWithImpl(this._self, this._then);

  final _Scene _self;
  final $Res Function(_Scene) _then;

/// Create a copy of Scene
/// with the given fields replaced by the non-null parameter values.
@override @pragma('vm:prefer-inline') $Res call({Object? index = null,Object? name = null,}) {
  return _then(_Scene(
index: null == index ? _self.index : index // ignore: cast_nullable_to_non_nullable
as int,name: null == name ? _self.name : name // ignore: cast_nullable_to_non_nullable
as String,
  ));
}


}


/// @nodoc
mixin _$Track {

 String get id;/// Cột 0..7.
 int get index; String get name;/// "#RRGGBB"
 String get color; TrackKind get kind; InstrumentRef? get instrument; Mixer get mixer; MonitorMode get monitor; List<FxSlot> get fx;/// Chỉ ô có clip mới xuất hiện (06 §2).
 List<Clip> get clips;
/// Create a copy of Track
/// with the given fields replaced by the non-null parameter values.
@JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
$TrackCopyWith<Track> get copyWith => _$TrackCopyWithImpl<Track>(this as Track, _$identity);

  /// Serializes this Track to a JSON map.
  Map<String, dynamic> toJson();


@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is Track&&(identical(other.id, id) || other.id == id)&&(identical(other.index, index) || other.index == index)&&(identical(other.name, name) || other.name == name)&&(identical(other.color, color) || other.color == color)&&(identical(other.kind, kind) || other.kind == kind)&&(identical(other.instrument, instrument) || other.instrument == instrument)&&(identical(other.mixer, mixer) || other.mixer == mixer)&&(identical(other.monitor, monitor) || other.monitor == monitor)&&const DeepCollectionEquality().equals(other.fx, fx)&&const DeepCollectionEquality().equals(other.clips, clips));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,id,index,name,color,kind,instrument,mixer,monitor,const DeepCollectionEquality().hash(fx),const DeepCollectionEquality().hash(clips));

@override
String toString() {
  return 'Track(id: $id, index: $index, name: $name, color: $color, kind: $kind, instrument: $instrument, mixer: $mixer, monitor: $monitor, fx: $fx, clips: $clips)';
}


}

/// @nodoc
abstract mixin class $TrackCopyWith<$Res>  {
  factory $TrackCopyWith(Track value, $Res Function(Track) _then) = _$TrackCopyWithImpl;
@useResult
$Res call({
 String id, int index, String name, String color, TrackKind kind, InstrumentRef? instrument, Mixer mixer, MonitorMode monitor, List<FxSlot> fx, List<Clip> clips
});


$InstrumentRefCopyWith<$Res>? get instrument;$MixerCopyWith<$Res> get mixer;

}
/// @nodoc
class _$TrackCopyWithImpl<$Res>
    implements $TrackCopyWith<$Res> {
  _$TrackCopyWithImpl(this._self, this._then);

  final Track _self;
  final $Res Function(Track) _then;

/// Create a copy of Track
/// with the given fields replaced by the non-null parameter values.
@pragma('vm:prefer-inline') @override $Res call({Object? id = null,Object? index = null,Object? name = null,Object? color = null,Object? kind = null,Object? instrument = freezed,Object? mixer = null,Object? monitor = null,Object? fx = null,Object? clips = null,}) {
  return _then(_self.copyWith(
id: null == id ? _self.id : id // ignore: cast_nullable_to_non_nullable
as String,index: null == index ? _self.index : index // ignore: cast_nullable_to_non_nullable
as int,name: null == name ? _self.name : name // ignore: cast_nullable_to_non_nullable
as String,color: null == color ? _self.color : color // ignore: cast_nullable_to_non_nullable
as String,kind: null == kind ? _self.kind : kind // ignore: cast_nullable_to_non_nullable
as TrackKind,instrument: freezed == instrument ? _self.instrument : instrument // ignore: cast_nullable_to_non_nullable
as InstrumentRef?,mixer: null == mixer ? _self.mixer : mixer // ignore: cast_nullable_to_non_nullable
as Mixer,monitor: null == monitor ? _self.monitor : monitor // ignore: cast_nullable_to_non_nullable
as MonitorMode,fx: null == fx ? _self.fx : fx // ignore: cast_nullable_to_non_nullable
as List<FxSlot>,clips: null == clips ? _self.clips : clips // ignore: cast_nullable_to_non_nullable
as List<Clip>,
  ));
}
/// Create a copy of Track
/// with the given fields replaced by the non-null parameter values.
@override
@pragma('vm:prefer-inline')
$InstrumentRefCopyWith<$Res>? get instrument {
    if (_self.instrument == null) {
    return null;
  }

  return $InstrumentRefCopyWith<$Res>(_self.instrument!, (value) {
    return _then(_self.copyWith(instrument: value));
  });
}/// Create a copy of Track
/// with the given fields replaced by the non-null parameter values.
@override
@pragma('vm:prefer-inline')
$MixerCopyWith<$Res> get mixer {
  
  return $MixerCopyWith<$Res>(_self.mixer, (value) {
    return _then(_self.copyWith(mixer: value));
  });
}
}


/// Adds pattern-matching-related methods to [Track].
extension TrackPatterns on Track {
/// A variant of `map` that fallback to returning `orElse`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeMap<TResult extends Object?>(TResult Function( _Track value)?  $default,{required TResult orElse(),}){
final _that = this;
switch (_that) {
case _Track() when $default != null:
return $default(_that);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// Callbacks receives the raw object, upcasted.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case final Subclass2 value:
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult map<TResult extends Object?>(TResult Function( _Track value)  $default,){
final _that = this;
switch (_that) {
case _Track():
return $default(_that);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `map` that fallback to returning `null`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? mapOrNull<TResult extends Object?>(TResult? Function( _Track value)?  $default,){
final _that = this;
switch (_that) {
case _Track() when $default != null:
return $default(_that);case _:
  return null;

}
}
/// A variant of `when` that fallback to an `orElse` callback.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeWhen<TResult extends Object?>(TResult Function( String id,  int index,  String name,  String color,  TrackKind kind,  InstrumentRef? instrument,  Mixer mixer,  MonitorMode monitor,  List<FxSlot> fx,  List<Clip> clips)?  $default,{required TResult orElse(),}) {final _that = this;
switch (_that) {
case _Track() when $default != null:
return $default(_that.id,_that.index,_that.name,_that.color,_that.kind,_that.instrument,_that.mixer,_that.monitor,_that.fx,_that.clips);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// As opposed to `map`, this offers destructuring.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case Subclass2(:final field2):
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult when<TResult extends Object?>(TResult Function( String id,  int index,  String name,  String color,  TrackKind kind,  InstrumentRef? instrument,  Mixer mixer,  MonitorMode monitor,  List<FxSlot> fx,  List<Clip> clips)  $default,) {final _that = this;
switch (_that) {
case _Track():
return $default(_that.id,_that.index,_that.name,_that.color,_that.kind,_that.instrument,_that.mixer,_that.monitor,_that.fx,_that.clips);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `when` that fallback to returning `null`
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? whenOrNull<TResult extends Object?>(TResult? Function( String id,  int index,  String name,  String color,  TrackKind kind,  InstrumentRef? instrument,  Mixer mixer,  MonitorMode monitor,  List<FxSlot> fx,  List<Clip> clips)?  $default,) {final _that = this;
switch (_that) {
case _Track() when $default != null:
return $default(_that.id,_that.index,_that.name,_that.color,_that.kind,_that.instrument,_that.mixer,_that.monitor,_that.fx,_that.clips);case _:
  return null;

}
}

}

/// @nodoc
@JsonSerializable()

class _Track extends Track {
  const _Track({required this.id, required this.index, required this.name, required this.color, required this.kind, this.instrument, this.mixer = const Mixer(), this.monitor = MonitorMode.off, final  List<FxSlot> fx = const <FxSlot>[], final  List<Clip> clips = const <Clip>[]}): _fx = fx,_clips = clips,super._();
  factory _Track.fromJson(Map<String, dynamic> json) => _$TrackFromJson(json);

@override final  String id;
/// Cột 0..7.
@override final  int index;
@override final  String name;
/// "#RRGGBB"
@override final  String color;
@override final  TrackKind kind;
@override final  InstrumentRef? instrument;
@override@JsonKey() final  Mixer mixer;
@override@JsonKey() final  MonitorMode monitor;
 final  List<FxSlot> _fx;
@override@JsonKey() List<FxSlot> get fx {
  if (_fx is EqualUnmodifiableListView) return _fx;
  // ignore: implicit_dynamic_type
  return EqualUnmodifiableListView(_fx);
}

/// Chỉ ô có clip mới xuất hiện (06 §2).
 final  List<Clip> _clips;
/// Chỉ ô có clip mới xuất hiện (06 §2).
@override@JsonKey() List<Clip> get clips {
  if (_clips is EqualUnmodifiableListView) return _clips;
  // ignore: implicit_dynamic_type
  return EqualUnmodifiableListView(_clips);
}


/// Create a copy of Track
/// with the given fields replaced by the non-null parameter values.
@override @JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
_$TrackCopyWith<_Track> get copyWith => __$TrackCopyWithImpl<_Track>(this, _$identity);

@override
Map<String, dynamic> toJson() {
  return _$TrackToJson(this, );
}

@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is _Track&&(identical(other.id, id) || other.id == id)&&(identical(other.index, index) || other.index == index)&&(identical(other.name, name) || other.name == name)&&(identical(other.color, color) || other.color == color)&&(identical(other.kind, kind) || other.kind == kind)&&(identical(other.instrument, instrument) || other.instrument == instrument)&&(identical(other.mixer, mixer) || other.mixer == mixer)&&(identical(other.monitor, monitor) || other.monitor == monitor)&&const DeepCollectionEquality().equals(other._fx, _fx)&&const DeepCollectionEquality().equals(other._clips, _clips));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,id,index,name,color,kind,instrument,mixer,monitor,const DeepCollectionEquality().hash(_fx),const DeepCollectionEquality().hash(_clips));

@override
String toString() {
  return 'Track(id: $id, index: $index, name: $name, color: $color, kind: $kind, instrument: $instrument, mixer: $mixer, monitor: $monitor, fx: $fx, clips: $clips)';
}


}

/// @nodoc
abstract mixin class _$TrackCopyWith<$Res> implements $TrackCopyWith<$Res> {
  factory _$TrackCopyWith(_Track value, $Res Function(_Track) _then) = __$TrackCopyWithImpl;
@override @useResult
$Res call({
 String id, int index, String name, String color, TrackKind kind, InstrumentRef? instrument, Mixer mixer, MonitorMode monitor, List<FxSlot> fx, List<Clip> clips
});


@override $InstrumentRefCopyWith<$Res>? get instrument;@override $MixerCopyWith<$Res> get mixer;

}
/// @nodoc
class __$TrackCopyWithImpl<$Res>
    implements _$TrackCopyWith<$Res> {
  __$TrackCopyWithImpl(this._self, this._then);

  final _Track _self;
  final $Res Function(_Track) _then;

/// Create a copy of Track
/// with the given fields replaced by the non-null parameter values.
@override @pragma('vm:prefer-inline') $Res call({Object? id = null,Object? index = null,Object? name = null,Object? color = null,Object? kind = null,Object? instrument = freezed,Object? mixer = null,Object? monitor = null,Object? fx = null,Object? clips = null,}) {
  return _then(_Track(
id: null == id ? _self.id : id // ignore: cast_nullable_to_non_nullable
as String,index: null == index ? _self.index : index // ignore: cast_nullable_to_non_nullable
as int,name: null == name ? _self.name : name // ignore: cast_nullable_to_non_nullable
as String,color: null == color ? _self.color : color // ignore: cast_nullable_to_non_nullable
as String,kind: null == kind ? _self.kind : kind // ignore: cast_nullable_to_non_nullable
as TrackKind,instrument: freezed == instrument ? _self.instrument : instrument // ignore: cast_nullable_to_non_nullable
as InstrumentRef?,mixer: null == mixer ? _self.mixer : mixer // ignore: cast_nullable_to_non_nullable
as Mixer,monitor: null == monitor ? _self.monitor : monitor // ignore: cast_nullable_to_non_nullable
as MonitorMode,fx: null == fx ? _self._fx : fx // ignore: cast_nullable_to_non_nullable
as List<FxSlot>,clips: null == clips ? _self._clips : clips // ignore: cast_nullable_to_non_nullable
as List<Clip>,
  ));
}

/// Create a copy of Track
/// with the given fields replaced by the non-null parameter values.
@override
@pragma('vm:prefer-inline')
$InstrumentRefCopyWith<$Res>? get instrument {
    if (_self.instrument == null) {
    return null;
  }

  return $InstrumentRefCopyWith<$Res>(_self.instrument!, (value) {
    return _then(_self.copyWith(instrument: value));
  });
}/// Create a copy of Track
/// with the given fields replaced by the non-null parameter values.
@override
@pragma('vm:prefer-inline')
$MixerCopyWith<$Res> get mixer {
  
  return $MixerCopyWith<$Res>(_self.mixer, (value) {
    return _then(_self.copyWith(mixer: value));
  });
}
}

InstrumentRef _$InstrumentRefFromJson(
  Map<String, dynamic> json
) {
        switch (json['kind']) {
                  case 'sfz':
          return SfzInstrumentRef.fromJson(
            json
          );
                case 'user':
          return UserInstrumentRef.fromJson(
            json
          );
        
          default:
            throw CheckedFromJsonException(
  json,
  'kind',
  'InstrumentRef',
  'Invalid union type "${json['kind']}"!'
);
        }
      
}

/// @nodoc
mixin _$InstrumentRef {



  /// Serializes this InstrumentRef to a JSON map.
  Map<String, dynamic> toJson();


@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is InstrumentRef);
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => runtimeType.hashCode;

@override
String toString() {
  return 'InstrumentRef()';
}


}

/// @nodoc
class $InstrumentRefCopyWith<$Res>  {
$InstrumentRefCopyWith(InstrumentRef _, $Res Function(InstrumentRef) __);
}


/// Adds pattern-matching-related methods to [InstrumentRef].
extension InstrumentRefPatterns on InstrumentRef {
/// A variant of `map` that fallback to returning `orElse`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeMap<TResult extends Object?>({TResult Function( SfzInstrumentRef value)?  sfz,TResult Function( UserInstrumentRef value)?  user,required TResult orElse(),}){
final _that = this;
switch (_that) {
case SfzInstrumentRef() when sfz != null:
return sfz(_that);case UserInstrumentRef() when user != null:
return user(_that);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// Callbacks receives the raw object, upcasted.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case final Subclass2 value:
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult map<TResult extends Object?>({required TResult Function( SfzInstrumentRef value)  sfz,required TResult Function( UserInstrumentRef value)  user,}){
final _that = this;
switch (_that) {
case SfzInstrumentRef():
return sfz(_that);case UserInstrumentRef():
return user(_that);}
}
/// A variant of `map` that fallback to returning `null`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? mapOrNull<TResult extends Object?>({TResult? Function( SfzInstrumentRef value)?  sfz,TResult? Function( UserInstrumentRef value)?  user,}){
final _that = this;
switch (_that) {
case SfzInstrumentRef() when sfz != null:
return sfz(_that);case UserInstrumentRef() when user != null:
return user(_that);case _:
  return null;

}
}
/// A variant of `when` that fallback to an `orElse` callback.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeWhen<TResult extends Object?>({TResult Function( String path)?  sfz,TResult Function( String id)?  user,required TResult orElse(),}) {final _that = this;
switch (_that) {
case SfzInstrumentRef() when sfz != null:
return sfz(_that.path);case UserInstrumentRef() when user != null:
return user(_that.id);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// As opposed to `map`, this offers destructuring.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case Subclass2(:final field2):
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult when<TResult extends Object?>({required TResult Function( String path)  sfz,required TResult Function( String id)  user,}) {final _that = this;
switch (_that) {
case SfzInstrumentRef():
return sfz(_that.path);case UserInstrumentRef():
return user(_that.id);}
}
/// A variant of `when` that fallback to returning `null`
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? whenOrNull<TResult extends Object?>({TResult? Function( String path)?  sfz,TResult? Function( String id)?  user,}) {final _that = this;
switch (_that) {
case SfzInstrumentRef() when sfz != null:
return sfz(_that.path);case UserInstrumentRef() when user != null:
return user(_that.id);case _:
  return null;

}
}

}

/// @nodoc
@JsonSerializable()

class SfzInstrumentRef implements InstrumentRef {
  const SfzInstrumentRef({required this.path, final  String? $type}): $type = $type ?? 'sfz';
  factory SfzInstrumentRef.fromJson(Map<String, dynamic> json) => _$SfzInstrumentRefFromJson(json);

 final  String path;

@JsonKey(name: 'kind')
final String $type;


/// Create a copy of InstrumentRef
/// with the given fields replaced by the non-null parameter values.
@JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
$SfzInstrumentRefCopyWith<SfzInstrumentRef> get copyWith => _$SfzInstrumentRefCopyWithImpl<SfzInstrumentRef>(this, _$identity);

@override
Map<String, dynamic> toJson() {
  return _$SfzInstrumentRefToJson(this, );
}

@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is SfzInstrumentRef&&(identical(other.path, path) || other.path == path));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,path);

@override
String toString() {
  return 'InstrumentRef.sfz(path: $path)';
}


}

/// @nodoc
abstract mixin class $SfzInstrumentRefCopyWith<$Res> implements $InstrumentRefCopyWith<$Res> {
  factory $SfzInstrumentRefCopyWith(SfzInstrumentRef value, $Res Function(SfzInstrumentRef) _then) = _$SfzInstrumentRefCopyWithImpl;
@useResult
$Res call({
 String path
});




}
/// @nodoc
class _$SfzInstrumentRefCopyWithImpl<$Res>
    implements $SfzInstrumentRefCopyWith<$Res> {
  _$SfzInstrumentRefCopyWithImpl(this._self, this._then);

  final SfzInstrumentRef _self;
  final $Res Function(SfzInstrumentRef) _then;

/// Create a copy of InstrumentRef
/// with the given fields replaced by the non-null parameter values.
@pragma('vm:prefer-inline') $Res call({Object? path = null,}) {
  return _then(SfzInstrumentRef(
path: null == path ? _self.path : path // ignore: cast_nullable_to_non_nullable
as String,
  ));
}


}

/// @nodoc
@JsonSerializable()

class UserInstrumentRef implements InstrumentRef {
  const UserInstrumentRef({required this.id, final  String? $type}): $type = $type ?? 'user';
  factory UserInstrumentRef.fromJson(Map<String, dynamic> json) => _$UserInstrumentRefFromJson(json);

 final  String id;

@JsonKey(name: 'kind')
final String $type;


/// Create a copy of InstrumentRef
/// with the given fields replaced by the non-null parameter values.
@JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
$UserInstrumentRefCopyWith<UserInstrumentRef> get copyWith => _$UserInstrumentRefCopyWithImpl<UserInstrumentRef>(this, _$identity);

@override
Map<String, dynamic> toJson() {
  return _$UserInstrumentRefToJson(this, );
}

@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is UserInstrumentRef&&(identical(other.id, id) || other.id == id));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,id);

@override
String toString() {
  return 'InstrumentRef.user(id: $id)';
}


}

/// @nodoc
abstract mixin class $UserInstrumentRefCopyWith<$Res> implements $InstrumentRefCopyWith<$Res> {
  factory $UserInstrumentRefCopyWith(UserInstrumentRef value, $Res Function(UserInstrumentRef) _then) = _$UserInstrumentRefCopyWithImpl;
@useResult
$Res call({
 String id
});




}
/// @nodoc
class _$UserInstrumentRefCopyWithImpl<$Res>
    implements $UserInstrumentRefCopyWith<$Res> {
  _$UserInstrumentRefCopyWithImpl(this._self, this._then);

  final UserInstrumentRef _self;
  final $Res Function(UserInstrumentRef) _then;

/// Create a copy of InstrumentRef
/// with the given fields replaced by the non-null parameter values.
@pragma('vm:prefer-inline') $Res call({Object? id = null,}) {
  return _then(UserInstrumentRef(
id: null == id ? _self.id : id // ignore: cast_nullable_to_non_nullable
as String,
  ));
}


}


/// @nodoc
mixin _$Mixer {

 double get gainDb; double get pan; bool get mute; bool get solo;
/// Create a copy of Mixer
/// with the given fields replaced by the non-null parameter values.
@JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
$MixerCopyWith<Mixer> get copyWith => _$MixerCopyWithImpl<Mixer>(this as Mixer, _$identity);

  /// Serializes this Mixer to a JSON map.
  Map<String, dynamic> toJson();


@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is Mixer&&(identical(other.gainDb, gainDb) || other.gainDb == gainDb)&&(identical(other.pan, pan) || other.pan == pan)&&(identical(other.mute, mute) || other.mute == mute)&&(identical(other.solo, solo) || other.solo == solo));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,gainDb,pan,mute,solo);

@override
String toString() {
  return 'Mixer(gainDb: $gainDb, pan: $pan, mute: $mute, solo: $solo)';
}


}

/// @nodoc
abstract mixin class $MixerCopyWith<$Res>  {
  factory $MixerCopyWith(Mixer value, $Res Function(Mixer) _then) = _$MixerCopyWithImpl;
@useResult
$Res call({
 double gainDb, double pan, bool mute, bool solo
});




}
/// @nodoc
class _$MixerCopyWithImpl<$Res>
    implements $MixerCopyWith<$Res> {
  _$MixerCopyWithImpl(this._self, this._then);

  final Mixer _self;
  final $Res Function(Mixer) _then;

/// Create a copy of Mixer
/// with the given fields replaced by the non-null parameter values.
@pragma('vm:prefer-inline') @override $Res call({Object? gainDb = null,Object? pan = null,Object? mute = null,Object? solo = null,}) {
  return _then(_self.copyWith(
gainDb: null == gainDb ? _self.gainDb : gainDb // ignore: cast_nullable_to_non_nullable
as double,pan: null == pan ? _self.pan : pan // ignore: cast_nullable_to_non_nullable
as double,mute: null == mute ? _self.mute : mute // ignore: cast_nullable_to_non_nullable
as bool,solo: null == solo ? _self.solo : solo // ignore: cast_nullable_to_non_nullable
as bool,
  ));
}

}


/// Adds pattern-matching-related methods to [Mixer].
extension MixerPatterns on Mixer {
/// A variant of `map` that fallback to returning `orElse`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeMap<TResult extends Object?>(TResult Function( _Mixer value)?  $default,{required TResult orElse(),}){
final _that = this;
switch (_that) {
case _Mixer() when $default != null:
return $default(_that);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// Callbacks receives the raw object, upcasted.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case final Subclass2 value:
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult map<TResult extends Object?>(TResult Function( _Mixer value)  $default,){
final _that = this;
switch (_that) {
case _Mixer():
return $default(_that);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `map` that fallback to returning `null`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? mapOrNull<TResult extends Object?>(TResult? Function( _Mixer value)?  $default,){
final _that = this;
switch (_that) {
case _Mixer() when $default != null:
return $default(_that);case _:
  return null;

}
}
/// A variant of `when` that fallback to an `orElse` callback.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeWhen<TResult extends Object?>(TResult Function( double gainDb,  double pan,  bool mute,  bool solo)?  $default,{required TResult orElse(),}) {final _that = this;
switch (_that) {
case _Mixer() when $default != null:
return $default(_that.gainDb,_that.pan,_that.mute,_that.solo);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// As opposed to `map`, this offers destructuring.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case Subclass2(:final field2):
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult when<TResult extends Object?>(TResult Function( double gainDb,  double pan,  bool mute,  bool solo)  $default,) {final _that = this;
switch (_that) {
case _Mixer():
return $default(_that.gainDb,_that.pan,_that.mute,_that.solo);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `when` that fallback to returning `null`
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? whenOrNull<TResult extends Object?>(TResult? Function( double gainDb,  double pan,  bool mute,  bool solo)?  $default,) {final _that = this;
switch (_that) {
case _Mixer() when $default != null:
return $default(_that.gainDb,_that.pan,_that.mute,_that.solo);case _:
  return null;

}
}

}

/// @nodoc
@JsonSerializable()

class _Mixer implements Mixer {
  const _Mixer({this.gainDb = 0.0, this.pan = 0.0, this.mute = false, this.solo = false});
  factory _Mixer.fromJson(Map<String, dynamic> json) => _$MixerFromJson(json);

@override@JsonKey() final  double gainDb;
@override@JsonKey() final  double pan;
@override@JsonKey() final  bool mute;
@override@JsonKey() final  bool solo;

/// Create a copy of Mixer
/// with the given fields replaced by the non-null parameter values.
@override @JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
_$MixerCopyWith<_Mixer> get copyWith => __$MixerCopyWithImpl<_Mixer>(this, _$identity);

@override
Map<String, dynamic> toJson() {
  return _$MixerToJson(this, );
}

@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is _Mixer&&(identical(other.gainDb, gainDb) || other.gainDb == gainDb)&&(identical(other.pan, pan) || other.pan == pan)&&(identical(other.mute, mute) || other.mute == mute)&&(identical(other.solo, solo) || other.solo == solo));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,gainDb,pan,mute,solo);

@override
String toString() {
  return 'Mixer(gainDb: $gainDb, pan: $pan, mute: $mute, solo: $solo)';
}


}

/// @nodoc
abstract mixin class _$MixerCopyWith<$Res> implements $MixerCopyWith<$Res> {
  factory _$MixerCopyWith(_Mixer value, $Res Function(_Mixer) _then) = __$MixerCopyWithImpl;
@override @useResult
$Res call({
 double gainDb, double pan, bool mute, bool solo
});




}
/// @nodoc
class __$MixerCopyWithImpl<$Res>
    implements _$MixerCopyWith<$Res> {
  __$MixerCopyWithImpl(this._self, this._then);

  final _Mixer _self;
  final $Res Function(_Mixer) _then;

/// Create a copy of Mixer
/// with the given fields replaced by the non-null parameter values.
@override @pragma('vm:prefer-inline') $Res call({Object? gainDb = null,Object? pan = null,Object? mute = null,Object? solo = null,}) {
  return _then(_Mixer(
gainDb: null == gainDb ? _self.gainDb : gainDb // ignore: cast_nullable_to_non_nullable
as double,pan: null == pan ? _self.pan : pan // ignore: cast_nullable_to_non_nullable
as double,mute: null == mute ? _self.mute : mute // ignore: cast_nullable_to_non_nullable
as bool,solo: null == solo ? _self.solo : solo // ignore: cast_nullable_to_non_nullable
as bool,
  ));
}


}


/// @nodoc
mixin _$FxSlot {

 FxType get type; bool get bypass;/// Key là `paramId` dạng chuỗi (04 §9).
 Map<String, double> get params;
/// Create a copy of FxSlot
/// with the given fields replaced by the non-null parameter values.
@JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
$FxSlotCopyWith<FxSlot> get copyWith => _$FxSlotCopyWithImpl<FxSlot>(this as FxSlot, _$identity);

  /// Serializes this FxSlot to a JSON map.
  Map<String, dynamic> toJson();


@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is FxSlot&&(identical(other.type, type) || other.type == type)&&(identical(other.bypass, bypass) || other.bypass == bypass)&&const DeepCollectionEquality().equals(other.params, params));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,type,bypass,const DeepCollectionEquality().hash(params));

@override
String toString() {
  return 'FxSlot(type: $type, bypass: $bypass, params: $params)';
}


}

/// @nodoc
abstract mixin class $FxSlotCopyWith<$Res>  {
  factory $FxSlotCopyWith(FxSlot value, $Res Function(FxSlot) _then) = _$FxSlotCopyWithImpl;
@useResult
$Res call({
 FxType type, bool bypass, Map<String, double> params
});




}
/// @nodoc
class _$FxSlotCopyWithImpl<$Res>
    implements $FxSlotCopyWith<$Res> {
  _$FxSlotCopyWithImpl(this._self, this._then);

  final FxSlot _self;
  final $Res Function(FxSlot) _then;

/// Create a copy of FxSlot
/// with the given fields replaced by the non-null parameter values.
@pragma('vm:prefer-inline') @override $Res call({Object? type = null,Object? bypass = null,Object? params = null,}) {
  return _then(_self.copyWith(
type: null == type ? _self.type : type // ignore: cast_nullable_to_non_nullable
as FxType,bypass: null == bypass ? _self.bypass : bypass // ignore: cast_nullable_to_non_nullable
as bool,params: null == params ? _self.params : params // ignore: cast_nullable_to_non_nullable
as Map<String, double>,
  ));
}

}


/// Adds pattern-matching-related methods to [FxSlot].
extension FxSlotPatterns on FxSlot {
/// A variant of `map` that fallback to returning `orElse`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeMap<TResult extends Object?>(TResult Function( _FxSlot value)?  $default,{required TResult orElse(),}){
final _that = this;
switch (_that) {
case _FxSlot() when $default != null:
return $default(_that);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// Callbacks receives the raw object, upcasted.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case final Subclass2 value:
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult map<TResult extends Object?>(TResult Function( _FxSlot value)  $default,){
final _that = this;
switch (_that) {
case _FxSlot():
return $default(_that);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `map` that fallback to returning `null`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? mapOrNull<TResult extends Object?>(TResult? Function( _FxSlot value)?  $default,){
final _that = this;
switch (_that) {
case _FxSlot() when $default != null:
return $default(_that);case _:
  return null;

}
}
/// A variant of `when` that fallback to an `orElse` callback.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeWhen<TResult extends Object?>(TResult Function( FxType type,  bool bypass,  Map<String, double> params)?  $default,{required TResult orElse(),}) {final _that = this;
switch (_that) {
case _FxSlot() when $default != null:
return $default(_that.type,_that.bypass,_that.params);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// As opposed to `map`, this offers destructuring.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case Subclass2(:final field2):
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult when<TResult extends Object?>(TResult Function( FxType type,  bool bypass,  Map<String, double> params)  $default,) {final _that = this;
switch (_that) {
case _FxSlot():
return $default(_that.type,_that.bypass,_that.params);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `when` that fallback to returning `null`
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? whenOrNull<TResult extends Object?>(TResult? Function( FxType type,  bool bypass,  Map<String, double> params)?  $default,) {final _that = this;
switch (_that) {
case _FxSlot() when $default != null:
return $default(_that.type,_that.bypass,_that.params);case _:
  return null;

}
}

}

/// @nodoc
@JsonSerializable()

class _FxSlot implements FxSlot {
  const _FxSlot({required this.type, this.bypass = false, final  Map<String, double> params = const <String, double>{}}): _params = params;
  factory _FxSlot.fromJson(Map<String, dynamic> json) => _$FxSlotFromJson(json);

@override final  FxType type;
@override@JsonKey() final  bool bypass;
/// Key là `paramId` dạng chuỗi (04 §9).
 final  Map<String, double> _params;
/// Key là `paramId` dạng chuỗi (04 §9).
@override@JsonKey() Map<String, double> get params {
  if (_params is EqualUnmodifiableMapView) return _params;
  // ignore: implicit_dynamic_type
  return EqualUnmodifiableMapView(_params);
}


/// Create a copy of FxSlot
/// with the given fields replaced by the non-null parameter values.
@override @JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
_$FxSlotCopyWith<_FxSlot> get copyWith => __$FxSlotCopyWithImpl<_FxSlot>(this, _$identity);

@override
Map<String, dynamic> toJson() {
  return _$FxSlotToJson(this, );
}

@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is _FxSlot&&(identical(other.type, type) || other.type == type)&&(identical(other.bypass, bypass) || other.bypass == bypass)&&const DeepCollectionEquality().equals(other._params, _params));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,type,bypass,const DeepCollectionEquality().hash(_params));

@override
String toString() {
  return 'FxSlot(type: $type, bypass: $bypass, params: $params)';
}


}

/// @nodoc
abstract mixin class _$FxSlotCopyWith<$Res> implements $FxSlotCopyWith<$Res> {
  factory _$FxSlotCopyWith(_FxSlot value, $Res Function(_FxSlot) _then) = __$FxSlotCopyWithImpl;
@override @useResult
$Res call({
 FxType type, bool bypass, Map<String, double> params
});




}
/// @nodoc
class __$FxSlotCopyWithImpl<$Res>
    implements _$FxSlotCopyWith<$Res> {
  __$FxSlotCopyWithImpl(this._self, this._then);

  final _FxSlot _self;
  final $Res Function(_FxSlot) _then;

/// Create a copy of FxSlot
/// with the given fields replaced by the non-null parameter values.
@override @pragma('vm:prefer-inline') $Res call({Object? type = null,Object? bypass = null,Object? params = null,}) {
  return _then(_FxSlot(
type: null == type ? _self.type : type // ignore: cast_nullable_to_non_nullable
as FxType,bypass: null == bypass ? _self.bypass : bypass // ignore: cast_nullable_to_non_nullable
as bool,params: null == params ? _self._params : params // ignore: cast_nullable_to_non_nullable
as Map<String, double>,
  ));
}


}

Clip _$ClipFromJson(
  Map<String, dynamic> json
) {
        switch (json['kind']) {
                  case 'midi':
          return MidiClip.fromJson(
            json
          );
                case 'audio':
          return AudioClip.fromJson(
            json
          );
        
          default:
            throw CheckedFromJsonException(
  json,
  'kind',
  'Clip',
  'Invalid union type "${json['kind']}"!'
);
        }
      
}

/// @nodoc
mixin _$Clip {

 int get slot; String get id; String get name; double get lengthBeats;
/// Create a copy of Clip
/// with the given fields replaced by the non-null parameter values.
@JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
$ClipCopyWith<Clip> get copyWith => _$ClipCopyWithImpl<Clip>(this as Clip, _$identity);

  /// Serializes this Clip to a JSON map.
  Map<String, dynamic> toJson();


@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is Clip&&(identical(other.slot, slot) || other.slot == slot)&&(identical(other.id, id) || other.id == id)&&(identical(other.name, name) || other.name == name)&&(identical(other.lengthBeats, lengthBeats) || other.lengthBeats == lengthBeats));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,slot,id,name,lengthBeats);

@override
String toString() {
  return 'Clip(slot: $slot, id: $id, name: $name, lengthBeats: $lengthBeats)';
}


}

/// @nodoc
abstract mixin class $ClipCopyWith<$Res>  {
  factory $ClipCopyWith(Clip value, $Res Function(Clip) _then) = _$ClipCopyWithImpl;
@useResult
$Res call({
 int slot, String id, String name, double lengthBeats
});




}
/// @nodoc
class _$ClipCopyWithImpl<$Res>
    implements $ClipCopyWith<$Res> {
  _$ClipCopyWithImpl(this._self, this._then);

  final Clip _self;
  final $Res Function(Clip) _then;

/// Create a copy of Clip
/// with the given fields replaced by the non-null parameter values.
@pragma('vm:prefer-inline') @override $Res call({Object? slot = null,Object? id = null,Object? name = null,Object? lengthBeats = null,}) {
  return _then(_self.copyWith(
slot: null == slot ? _self.slot : slot // ignore: cast_nullable_to_non_nullable
as int,id: null == id ? _self.id : id // ignore: cast_nullable_to_non_nullable
as String,name: null == name ? _self.name : name // ignore: cast_nullable_to_non_nullable
as String,lengthBeats: null == lengthBeats ? _self.lengthBeats : lengthBeats // ignore: cast_nullable_to_non_nullable
as double,
  ));
}

}


/// Adds pattern-matching-related methods to [Clip].
extension ClipPatterns on Clip {
/// A variant of `map` that fallback to returning `orElse`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeMap<TResult extends Object?>({TResult Function( MidiClip value)?  midi,TResult Function( AudioClip value)?  audio,required TResult orElse(),}){
final _that = this;
switch (_that) {
case MidiClip() when midi != null:
return midi(_that);case AudioClip() when audio != null:
return audio(_that);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// Callbacks receives the raw object, upcasted.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case final Subclass2 value:
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult map<TResult extends Object?>({required TResult Function( MidiClip value)  midi,required TResult Function( AudioClip value)  audio,}){
final _that = this;
switch (_that) {
case MidiClip():
return midi(_that);case AudioClip():
return audio(_that);}
}
/// A variant of `map` that fallback to returning `null`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? mapOrNull<TResult extends Object?>({TResult? Function( MidiClip value)?  midi,TResult? Function( AudioClip value)?  audio,}){
final _that = this;
switch (_that) {
case MidiClip() when midi != null:
return midi(_that);case AudioClip() when audio != null:
return audio(_that);case _:
  return null;

}
}
/// A variant of `when` that fallback to an `orElse` callback.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeWhen<TResult extends Object?>({TResult Function( int slot,  String id,  String name,  double lengthBeats,  List<Note> notes)?  midi,TResult Function( int slot,  String id,  String name,  String file,  double lengthBeats,  double originalBpm,  WarpMode warp,  double gainDb,  AudioLoop loop,  List<String> tags)?  audio,required TResult orElse(),}) {final _that = this;
switch (_that) {
case MidiClip() when midi != null:
return midi(_that.slot,_that.id,_that.name,_that.lengthBeats,_that.notes);case AudioClip() when audio != null:
return audio(_that.slot,_that.id,_that.name,_that.file,_that.lengthBeats,_that.originalBpm,_that.warp,_that.gainDb,_that.loop,_that.tags);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// As opposed to `map`, this offers destructuring.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case Subclass2(:final field2):
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult when<TResult extends Object?>({required TResult Function( int slot,  String id,  String name,  double lengthBeats,  List<Note> notes)  midi,required TResult Function( int slot,  String id,  String name,  String file,  double lengthBeats,  double originalBpm,  WarpMode warp,  double gainDb,  AudioLoop loop,  List<String> tags)  audio,}) {final _that = this;
switch (_that) {
case MidiClip():
return midi(_that.slot,_that.id,_that.name,_that.lengthBeats,_that.notes);case AudioClip():
return audio(_that.slot,_that.id,_that.name,_that.file,_that.lengthBeats,_that.originalBpm,_that.warp,_that.gainDb,_that.loop,_that.tags);}
}
/// A variant of `when` that fallback to returning `null`
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? whenOrNull<TResult extends Object?>({TResult? Function( int slot,  String id,  String name,  double lengthBeats,  List<Note> notes)?  midi,TResult? Function( int slot,  String id,  String name,  String file,  double lengthBeats,  double originalBpm,  WarpMode warp,  double gainDb,  AudioLoop loop,  List<String> tags)?  audio,}) {final _that = this;
switch (_that) {
case MidiClip() when midi != null:
return midi(_that.slot,_that.id,_that.name,_that.lengthBeats,_that.notes);case AudioClip() when audio != null:
return audio(_that.slot,_that.id,_that.name,_that.file,_that.lengthBeats,_that.originalBpm,_that.warp,_that.gainDb,_that.loop,_that.tags);case _:
  return null;

}
}

}

/// @nodoc
@JsonSerializable()

class MidiClip extends Clip {
  const MidiClip({required this.slot, required this.id, required this.name, required this.lengthBeats, final  List<Note> notes = const <Note>[], final  String? $type}): _notes = notes,$type = $type ?? 'midi',super._();
  factory MidiClip.fromJson(Map<String, dynamic> json) => _$MidiClipFromJson(json);

@override final  int slot;
@override final  String id;
@override final  String name;
@override final  double lengthBeats;
 final  List<Note> _notes;
@JsonKey() List<Note> get notes {
  if (_notes is EqualUnmodifiableListView) return _notes;
  // ignore: implicit_dynamic_type
  return EqualUnmodifiableListView(_notes);
}


@JsonKey(name: 'kind')
final String $type;


/// Create a copy of Clip
/// with the given fields replaced by the non-null parameter values.
@override @JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
$MidiClipCopyWith<MidiClip> get copyWith => _$MidiClipCopyWithImpl<MidiClip>(this, _$identity);

@override
Map<String, dynamic> toJson() {
  return _$MidiClipToJson(this, );
}

@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is MidiClip&&(identical(other.slot, slot) || other.slot == slot)&&(identical(other.id, id) || other.id == id)&&(identical(other.name, name) || other.name == name)&&(identical(other.lengthBeats, lengthBeats) || other.lengthBeats == lengthBeats)&&const DeepCollectionEquality().equals(other._notes, _notes));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,slot,id,name,lengthBeats,const DeepCollectionEquality().hash(_notes));

@override
String toString() {
  return 'Clip.midi(slot: $slot, id: $id, name: $name, lengthBeats: $lengthBeats, notes: $notes)';
}


}

/// @nodoc
abstract mixin class $MidiClipCopyWith<$Res> implements $ClipCopyWith<$Res> {
  factory $MidiClipCopyWith(MidiClip value, $Res Function(MidiClip) _then) = _$MidiClipCopyWithImpl;
@override @useResult
$Res call({
 int slot, String id, String name, double lengthBeats, List<Note> notes
});




}
/// @nodoc
class _$MidiClipCopyWithImpl<$Res>
    implements $MidiClipCopyWith<$Res> {
  _$MidiClipCopyWithImpl(this._self, this._then);

  final MidiClip _self;
  final $Res Function(MidiClip) _then;

/// Create a copy of Clip
/// with the given fields replaced by the non-null parameter values.
@override @pragma('vm:prefer-inline') $Res call({Object? slot = null,Object? id = null,Object? name = null,Object? lengthBeats = null,Object? notes = null,}) {
  return _then(MidiClip(
slot: null == slot ? _self.slot : slot // ignore: cast_nullable_to_non_nullable
as int,id: null == id ? _self.id : id // ignore: cast_nullable_to_non_nullable
as String,name: null == name ? _self.name : name // ignore: cast_nullable_to_non_nullable
as String,lengthBeats: null == lengthBeats ? _self.lengthBeats : lengthBeats // ignore: cast_nullable_to_non_nullable
as double,notes: null == notes ? _self._notes : notes // ignore: cast_nullable_to_non_nullable
as List<Note>,
  ));
}


}

/// @nodoc
@JsonSerializable()

class AudioClip extends Clip {
  const AudioClip({required this.slot, required this.id, required this.name, required this.file, required this.lengthBeats, required this.originalBpm, this.warp = WarpMode.stretch, this.gainDb = 0.0, this.loop = const AudioLoop(), final  List<String> tags = const <String>[], final  String? $type}): _tags = tags,$type = $type ?? 'audio',super._();
  factory AudioClip.fromJson(Map<String, dynamic> json) => _$AudioClipFromJson(json);

@override final  int slot;
@override final  String id;
@override final  String name;
/// Tương đối so với thư mục project, ví dụ `audio/c_<uuid>.caf`.
 final  String file;
@override final  double lengthBeats;
 final  double originalBpm;
@JsonKey() final  WarpMode warp;
@JsonKey() final  double gainDb;
@JsonKey() final  AudioLoop loop;
/// Tag thư viện (06 §2/§4: id tiếng Anh cố định, vd `drums`) chép khi gán loop; take tự thu `[]`.
/// Gợi ý "loop trống → Re-Pitch" đọc từ đây (còn sau khi mở lại project).
 final  List<String> _tags;
/// Tag thư viện (06 §2/§4: id tiếng Anh cố định, vd `drums`) chép khi gán loop; take tự thu `[]`.
/// Gợi ý "loop trống → Re-Pitch" đọc từ đây (còn sau khi mở lại project).
@JsonKey() List<String> get tags {
  if (_tags is EqualUnmodifiableListView) return _tags;
  // ignore: implicit_dynamic_type
  return EqualUnmodifiableListView(_tags);
}


@JsonKey(name: 'kind')
final String $type;


/// Create a copy of Clip
/// with the given fields replaced by the non-null parameter values.
@override @JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
$AudioClipCopyWith<AudioClip> get copyWith => _$AudioClipCopyWithImpl<AudioClip>(this, _$identity);

@override
Map<String, dynamic> toJson() {
  return _$AudioClipToJson(this, );
}

@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is AudioClip&&(identical(other.slot, slot) || other.slot == slot)&&(identical(other.id, id) || other.id == id)&&(identical(other.name, name) || other.name == name)&&(identical(other.file, file) || other.file == file)&&(identical(other.lengthBeats, lengthBeats) || other.lengthBeats == lengthBeats)&&(identical(other.originalBpm, originalBpm) || other.originalBpm == originalBpm)&&(identical(other.warp, warp) || other.warp == warp)&&(identical(other.gainDb, gainDb) || other.gainDb == gainDb)&&(identical(other.loop, loop) || other.loop == loop)&&const DeepCollectionEquality().equals(other._tags, _tags));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,slot,id,name,file,lengthBeats,originalBpm,warp,gainDb,loop,const DeepCollectionEquality().hash(_tags));

@override
String toString() {
  return 'Clip.audio(slot: $slot, id: $id, name: $name, file: $file, lengthBeats: $lengthBeats, originalBpm: $originalBpm, warp: $warp, gainDb: $gainDb, loop: $loop, tags: $tags)';
}


}

/// @nodoc
abstract mixin class $AudioClipCopyWith<$Res> implements $ClipCopyWith<$Res> {
  factory $AudioClipCopyWith(AudioClip value, $Res Function(AudioClip) _then) = _$AudioClipCopyWithImpl;
@override @useResult
$Res call({
 int slot, String id, String name, String file, double lengthBeats, double originalBpm, WarpMode warp, double gainDb, AudioLoop loop, List<String> tags
});


$AudioLoopCopyWith<$Res> get loop;

}
/// @nodoc
class _$AudioClipCopyWithImpl<$Res>
    implements $AudioClipCopyWith<$Res> {
  _$AudioClipCopyWithImpl(this._self, this._then);

  final AudioClip _self;
  final $Res Function(AudioClip) _then;

/// Create a copy of Clip
/// with the given fields replaced by the non-null parameter values.
@override @pragma('vm:prefer-inline') $Res call({Object? slot = null,Object? id = null,Object? name = null,Object? file = null,Object? lengthBeats = null,Object? originalBpm = null,Object? warp = null,Object? gainDb = null,Object? loop = null,Object? tags = null,}) {
  return _then(AudioClip(
slot: null == slot ? _self.slot : slot // ignore: cast_nullable_to_non_nullable
as int,id: null == id ? _self.id : id // ignore: cast_nullable_to_non_nullable
as String,name: null == name ? _self.name : name // ignore: cast_nullable_to_non_nullable
as String,file: null == file ? _self.file : file // ignore: cast_nullable_to_non_nullable
as String,lengthBeats: null == lengthBeats ? _self.lengthBeats : lengthBeats // ignore: cast_nullable_to_non_nullable
as double,originalBpm: null == originalBpm ? _self.originalBpm : originalBpm // ignore: cast_nullable_to_non_nullable
as double,warp: null == warp ? _self.warp : warp // ignore: cast_nullable_to_non_nullable
as WarpMode,gainDb: null == gainDb ? _self.gainDb : gainDb // ignore: cast_nullable_to_non_nullable
as double,loop: null == loop ? _self.loop : loop // ignore: cast_nullable_to_non_nullable
as AudioLoop,tags: null == tags ? _self._tags : tags // ignore: cast_nullable_to_non_nullable
as List<String>,
  ));
}

/// Create a copy of Clip
/// with the given fields replaced by the non-null parameter values.
@override
@pragma('vm:prefer-inline')
$AudioLoopCopyWith<$Res> get loop {
  
  return $AudioLoopCopyWith<$Res>(_self.loop, (value) {
    return _then(_self.copyWith(loop: value));
  });
}
}


/// @nodoc
mixin _$Note {

 int get p; int get v; double get s; double get d;
/// Create a copy of Note
/// with the given fields replaced by the non-null parameter values.
@JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
$NoteCopyWith<Note> get copyWith => _$NoteCopyWithImpl<Note>(this as Note, _$identity);

  /// Serializes this Note to a JSON map.
  Map<String, dynamic> toJson();


@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is Note&&(identical(other.p, p) || other.p == p)&&(identical(other.v, v) || other.v == v)&&(identical(other.s, s) || other.s == s)&&(identical(other.d, d) || other.d == d));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,p,v,s,d);

@override
String toString() {
  return 'Note(p: $p, v: $v, s: $s, d: $d)';
}


}

/// @nodoc
abstract mixin class $NoteCopyWith<$Res>  {
  factory $NoteCopyWith(Note value, $Res Function(Note) _then) = _$NoteCopyWithImpl;
@useResult
$Res call({
 int p, int v, double s, double d
});




}
/// @nodoc
class _$NoteCopyWithImpl<$Res>
    implements $NoteCopyWith<$Res> {
  _$NoteCopyWithImpl(this._self, this._then);

  final Note _self;
  final $Res Function(Note) _then;

/// Create a copy of Note
/// with the given fields replaced by the non-null parameter values.
@pragma('vm:prefer-inline') @override $Res call({Object? p = null,Object? v = null,Object? s = null,Object? d = null,}) {
  return _then(_self.copyWith(
p: null == p ? _self.p : p // ignore: cast_nullable_to_non_nullable
as int,v: null == v ? _self.v : v // ignore: cast_nullable_to_non_nullable
as int,s: null == s ? _self.s : s // ignore: cast_nullable_to_non_nullable
as double,d: null == d ? _self.d : d // ignore: cast_nullable_to_non_nullable
as double,
  ));
}

}


/// Adds pattern-matching-related methods to [Note].
extension NotePatterns on Note {
/// A variant of `map` that fallback to returning `orElse`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeMap<TResult extends Object?>(TResult Function( _Note value)?  $default,{required TResult orElse(),}){
final _that = this;
switch (_that) {
case _Note() when $default != null:
return $default(_that);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// Callbacks receives the raw object, upcasted.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case final Subclass2 value:
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult map<TResult extends Object?>(TResult Function( _Note value)  $default,){
final _that = this;
switch (_that) {
case _Note():
return $default(_that);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `map` that fallback to returning `null`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? mapOrNull<TResult extends Object?>(TResult? Function( _Note value)?  $default,){
final _that = this;
switch (_that) {
case _Note() when $default != null:
return $default(_that);case _:
  return null;

}
}
/// A variant of `when` that fallback to an `orElse` callback.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeWhen<TResult extends Object?>(TResult Function( int p,  int v,  double s,  double d)?  $default,{required TResult orElse(),}) {final _that = this;
switch (_that) {
case _Note() when $default != null:
return $default(_that.p,_that.v,_that.s,_that.d);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// As opposed to `map`, this offers destructuring.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case Subclass2(:final field2):
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult when<TResult extends Object?>(TResult Function( int p,  int v,  double s,  double d)  $default,) {final _that = this;
switch (_that) {
case _Note():
return $default(_that.p,_that.v,_that.s,_that.d);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `when` that fallback to returning `null`
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? whenOrNull<TResult extends Object?>(TResult? Function( int p,  int v,  double s,  double d)?  $default,) {final _that = this;
switch (_that) {
case _Note() when $default != null:
return $default(_that.p,_that.v,_that.s,_that.d);case _:
  return null;

}
}

}

/// @nodoc
@JsonSerializable()

class _Note implements Note {
  const _Note({required this.p, required this.v, required this.s, required this.d});
  factory _Note.fromJson(Map<String, dynamic> json) => _$NoteFromJson(json);

@override final  int p;
@override final  int v;
@override final  double s;
@override final  double d;

/// Create a copy of Note
/// with the given fields replaced by the non-null parameter values.
@override @JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
_$NoteCopyWith<_Note> get copyWith => __$NoteCopyWithImpl<_Note>(this, _$identity);

@override
Map<String, dynamic> toJson() {
  return _$NoteToJson(this, );
}

@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is _Note&&(identical(other.p, p) || other.p == p)&&(identical(other.v, v) || other.v == v)&&(identical(other.s, s) || other.s == s)&&(identical(other.d, d) || other.d == d));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,p,v,s,d);

@override
String toString() {
  return 'Note(p: $p, v: $v, s: $s, d: $d)';
}


}

/// @nodoc
abstract mixin class _$NoteCopyWith<$Res> implements $NoteCopyWith<$Res> {
  factory _$NoteCopyWith(_Note value, $Res Function(_Note) _then) = __$NoteCopyWithImpl;
@override @useResult
$Res call({
 int p, int v, double s, double d
});




}
/// @nodoc
class __$NoteCopyWithImpl<$Res>
    implements _$NoteCopyWith<$Res> {
  __$NoteCopyWithImpl(this._self, this._then);

  final _Note _self;
  final $Res Function(_Note) _then;

/// Create a copy of Note
/// with the given fields replaced by the non-null parameter values.
@override @pragma('vm:prefer-inline') $Res call({Object? p = null,Object? v = null,Object? s = null,Object? d = null,}) {
  return _then(_Note(
p: null == p ? _self.p : p // ignore: cast_nullable_to_non_nullable
as int,v: null == v ? _self.v : v // ignore: cast_nullable_to_non_nullable
as int,s: null == s ? _self.s : s // ignore: cast_nullable_to_non_nullable
as double,d: null == d ? _self.d : d // ignore: cast_nullable_to_non_nullable
as double,
  ));
}


}


/// @nodoc
mixin _$AudioLoop {

 int get startSample;
/// Create a copy of AudioLoop
/// with the given fields replaced by the non-null parameter values.
@JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
$AudioLoopCopyWith<AudioLoop> get copyWith => _$AudioLoopCopyWithImpl<AudioLoop>(this as AudioLoop, _$identity);

  /// Serializes this AudioLoop to a JSON map.
  Map<String, dynamic> toJson();


@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is AudioLoop&&(identical(other.startSample, startSample) || other.startSample == startSample));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,startSample);

@override
String toString() {
  return 'AudioLoop(startSample: $startSample)';
}


}

/// @nodoc
abstract mixin class $AudioLoopCopyWith<$Res>  {
  factory $AudioLoopCopyWith(AudioLoop value, $Res Function(AudioLoop) _then) = _$AudioLoopCopyWithImpl;
@useResult
$Res call({
 int startSample
});




}
/// @nodoc
class _$AudioLoopCopyWithImpl<$Res>
    implements $AudioLoopCopyWith<$Res> {
  _$AudioLoopCopyWithImpl(this._self, this._then);

  final AudioLoop _self;
  final $Res Function(AudioLoop) _then;

/// Create a copy of AudioLoop
/// with the given fields replaced by the non-null parameter values.
@pragma('vm:prefer-inline') @override $Res call({Object? startSample = null,}) {
  return _then(_self.copyWith(
startSample: null == startSample ? _self.startSample : startSample // ignore: cast_nullable_to_non_nullable
as int,
  ));
}

}


/// Adds pattern-matching-related methods to [AudioLoop].
extension AudioLoopPatterns on AudioLoop {
/// A variant of `map` that fallback to returning `orElse`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeMap<TResult extends Object?>(TResult Function( _AudioLoop value)?  $default,{required TResult orElse(),}){
final _that = this;
switch (_that) {
case _AudioLoop() when $default != null:
return $default(_that);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// Callbacks receives the raw object, upcasted.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case final Subclass2 value:
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult map<TResult extends Object?>(TResult Function( _AudioLoop value)  $default,){
final _that = this;
switch (_that) {
case _AudioLoop():
return $default(_that);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `map` that fallback to returning `null`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? mapOrNull<TResult extends Object?>(TResult? Function( _AudioLoop value)?  $default,){
final _that = this;
switch (_that) {
case _AudioLoop() when $default != null:
return $default(_that);case _:
  return null;

}
}
/// A variant of `when` that fallback to an `orElse` callback.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeWhen<TResult extends Object?>(TResult Function( int startSample)?  $default,{required TResult orElse(),}) {final _that = this;
switch (_that) {
case _AudioLoop() when $default != null:
return $default(_that.startSample);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// As opposed to `map`, this offers destructuring.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case Subclass2(:final field2):
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult when<TResult extends Object?>(TResult Function( int startSample)  $default,) {final _that = this;
switch (_that) {
case _AudioLoop():
return $default(_that.startSample);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `when` that fallback to returning `null`
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? whenOrNull<TResult extends Object?>(TResult? Function( int startSample)?  $default,) {final _that = this;
switch (_that) {
case _AudioLoop() when $default != null:
return $default(_that.startSample);case _:
  return null;

}
}

}

/// @nodoc
@JsonSerializable()

class _AudioLoop implements AudioLoop {
  const _AudioLoop({this.startSample = 0});
  factory _AudioLoop.fromJson(Map<String, dynamic> json) => _$AudioLoopFromJson(json);

@override@JsonKey() final  int startSample;

/// Create a copy of AudioLoop
/// with the given fields replaced by the non-null parameter values.
@override @JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
_$AudioLoopCopyWith<_AudioLoop> get copyWith => __$AudioLoopCopyWithImpl<_AudioLoop>(this, _$identity);

@override
Map<String, dynamic> toJson() {
  return _$AudioLoopToJson(this, );
}

@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is _AudioLoop&&(identical(other.startSample, startSample) || other.startSample == startSample));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,startSample);

@override
String toString() {
  return 'AudioLoop(startSample: $startSample)';
}


}

/// @nodoc
abstract mixin class _$AudioLoopCopyWith<$Res> implements $AudioLoopCopyWith<$Res> {
  factory _$AudioLoopCopyWith(_AudioLoop value, $Res Function(_AudioLoop) _then) = __$AudioLoopCopyWithImpl;
@override @useResult
$Res call({
 int startSample
});




}
/// @nodoc
class __$AudioLoopCopyWithImpl<$Res>
    implements _$AudioLoopCopyWith<$Res> {
  __$AudioLoopCopyWithImpl(this._self, this._then);

  final _AudioLoop _self;
  final $Res Function(_AudioLoop) _then;

/// Create a copy of AudioLoop
/// with the given fields replaced by the non-null parameter values.
@override @pragma('vm:prefer-inline') $Res call({Object? startSample = null,}) {
  return _then(_AudioLoop(
startSample: null == startSample ? _self.startSample : startSample // ignore: cast_nullable_to_non_nullable
as int,
  ));
}


}


/// @nodoc
mixin _$UserInstrument {

 String get id; String get name;/// Tương đối so với thư mục project.
 String get source; int get rootNote; double get cents; double get confidence; InstrumentMode get mode; Envelope get envelope;
/// Create a copy of UserInstrument
/// with the given fields replaced by the non-null parameter values.
@JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
$UserInstrumentCopyWith<UserInstrument> get copyWith => _$UserInstrumentCopyWithImpl<UserInstrument>(this as UserInstrument, _$identity);

  /// Serializes this UserInstrument to a JSON map.
  Map<String, dynamic> toJson();


@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is UserInstrument&&(identical(other.id, id) || other.id == id)&&(identical(other.name, name) || other.name == name)&&(identical(other.source, source) || other.source == source)&&(identical(other.rootNote, rootNote) || other.rootNote == rootNote)&&(identical(other.cents, cents) || other.cents == cents)&&(identical(other.confidence, confidence) || other.confidence == confidence)&&(identical(other.mode, mode) || other.mode == mode)&&(identical(other.envelope, envelope) || other.envelope == envelope));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,id,name,source,rootNote,cents,confidence,mode,envelope);

@override
String toString() {
  return 'UserInstrument(id: $id, name: $name, source: $source, rootNote: $rootNote, cents: $cents, confidence: $confidence, mode: $mode, envelope: $envelope)';
}


}

/// @nodoc
abstract mixin class $UserInstrumentCopyWith<$Res>  {
  factory $UserInstrumentCopyWith(UserInstrument value, $Res Function(UserInstrument) _then) = _$UserInstrumentCopyWithImpl;
@useResult
$Res call({
 String id, String name, String source, int rootNote, double cents, double confidence, InstrumentMode mode, Envelope envelope
});


$EnvelopeCopyWith<$Res> get envelope;

}
/// @nodoc
class _$UserInstrumentCopyWithImpl<$Res>
    implements $UserInstrumentCopyWith<$Res> {
  _$UserInstrumentCopyWithImpl(this._self, this._then);

  final UserInstrument _self;
  final $Res Function(UserInstrument) _then;

/// Create a copy of UserInstrument
/// with the given fields replaced by the non-null parameter values.
@pragma('vm:prefer-inline') @override $Res call({Object? id = null,Object? name = null,Object? source = null,Object? rootNote = null,Object? cents = null,Object? confidence = null,Object? mode = null,Object? envelope = null,}) {
  return _then(_self.copyWith(
id: null == id ? _self.id : id // ignore: cast_nullable_to_non_nullable
as String,name: null == name ? _self.name : name // ignore: cast_nullable_to_non_nullable
as String,source: null == source ? _self.source : source // ignore: cast_nullable_to_non_nullable
as String,rootNote: null == rootNote ? _self.rootNote : rootNote // ignore: cast_nullable_to_non_nullable
as int,cents: null == cents ? _self.cents : cents // ignore: cast_nullable_to_non_nullable
as double,confidence: null == confidence ? _self.confidence : confidence // ignore: cast_nullable_to_non_nullable
as double,mode: null == mode ? _self.mode : mode // ignore: cast_nullable_to_non_nullable
as InstrumentMode,envelope: null == envelope ? _self.envelope : envelope // ignore: cast_nullable_to_non_nullable
as Envelope,
  ));
}
/// Create a copy of UserInstrument
/// with the given fields replaced by the non-null parameter values.
@override
@pragma('vm:prefer-inline')
$EnvelopeCopyWith<$Res> get envelope {
  
  return $EnvelopeCopyWith<$Res>(_self.envelope, (value) {
    return _then(_self.copyWith(envelope: value));
  });
}
}


/// Adds pattern-matching-related methods to [UserInstrument].
extension UserInstrumentPatterns on UserInstrument {
/// A variant of `map` that fallback to returning `orElse`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeMap<TResult extends Object?>(TResult Function( _UserInstrument value)?  $default,{required TResult orElse(),}){
final _that = this;
switch (_that) {
case _UserInstrument() when $default != null:
return $default(_that);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// Callbacks receives the raw object, upcasted.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case final Subclass2 value:
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult map<TResult extends Object?>(TResult Function( _UserInstrument value)  $default,){
final _that = this;
switch (_that) {
case _UserInstrument():
return $default(_that);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `map` that fallback to returning `null`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? mapOrNull<TResult extends Object?>(TResult? Function( _UserInstrument value)?  $default,){
final _that = this;
switch (_that) {
case _UserInstrument() when $default != null:
return $default(_that);case _:
  return null;

}
}
/// A variant of `when` that fallback to an `orElse` callback.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeWhen<TResult extends Object?>(TResult Function( String id,  String name,  String source,  int rootNote,  double cents,  double confidence,  InstrumentMode mode,  Envelope envelope)?  $default,{required TResult orElse(),}) {final _that = this;
switch (_that) {
case _UserInstrument() when $default != null:
return $default(_that.id,_that.name,_that.source,_that.rootNote,_that.cents,_that.confidence,_that.mode,_that.envelope);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// As opposed to `map`, this offers destructuring.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case Subclass2(:final field2):
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult when<TResult extends Object?>(TResult Function( String id,  String name,  String source,  int rootNote,  double cents,  double confidence,  InstrumentMode mode,  Envelope envelope)  $default,) {final _that = this;
switch (_that) {
case _UserInstrument():
return $default(_that.id,_that.name,_that.source,_that.rootNote,_that.cents,_that.confidence,_that.mode,_that.envelope);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `when` that fallback to returning `null`
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? whenOrNull<TResult extends Object?>(TResult? Function( String id,  String name,  String source,  int rootNote,  double cents,  double confidence,  InstrumentMode mode,  Envelope envelope)?  $default,) {final _that = this;
switch (_that) {
case _UserInstrument() when $default != null:
return $default(_that.id,_that.name,_that.source,_that.rootNote,_that.cents,_that.confidence,_that.mode,_that.envelope);case _:
  return null;

}
}

}

/// @nodoc
@JsonSerializable()

class _UserInstrument implements UserInstrument {
  const _UserInstrument({required this.id, required this.name, required this.source, required this.rootNote, this.cents = 0.0, this.confidence = 1.0, this.mode = InstrumentMode.natural, this.envelope = const Envelope()});
  factory _UserInstrument.fromJson(Map<String, dynamic> json) => _$UserInstrumentFromJson(json);

@override final  String id;
@override final  String name;
/// Tương đối so với thư mục project.
@override final  String source;
@override final  int rootNote;
@override@JsonKey() final  double cents;
@override@JsonKey() final  double confidence;
@override@JsonKey() final  InstrumentMode mode;
@override@JsonKey() final  Envelope envelope;

/// Create a copy of UserInstrument
/// with the given fields replaced by the non-null parameter values.
@override @JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
_$UserInstrumentCopyWith<_UserInstrument> get copyWith => __$UserInstrumentCopyWithImpl<_UserInstrument>(this, _$identity);

@override
Map<String, dynamic> toJson() {
  return _$UserInstrumentToJson(this, );
}

@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is _UserInstrument&&(identical(other.id, id) || other.id == id)&&(identical(other.name, name) || other.name == name)&&(identical(other.source, source) || other.source == source)&&(identical(other.rootNote, rootNote) || other.rootNote == rootNote)&&(identical(other.cents, cents) || other.cents == cents)&&(identical(other.confidence, confidence) || other.confidence == confidence)&&(identical(other.mode, mode) || other.mode == mode)&&(identical(other.envelope, envelope) || other.envelope == envelope));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,id,name,source,rootNote,cents,confidence,mode,envelope);

@override
String toString() {
  return 'UserInstrument(id: $id, name: $name, source: $source, rootNote: $rootNote, cents: $cents, confidence: $confidence, mode: $mode, envelope: $envelope)';
}


}

/// @nodoc
abstract mixin class _$UserInstrumentCopyWith<$Res> implements $UserInstrumentCopyWith<$Res> {
  factory _$UserInstrumentCopyWith(_UserInstrument value, $Res Function(_UserInstrument) _then) = __$UserInstrumentCopyWithImpl;
@override @useResult
$Res call({
 String id, String name, String source, int rootNote, double cents, double confidence, InstrumentMode mode, Envelope envelope
});


@override $EnvelopeCopyWith<$Res> get envelope;

}
/// @nodoc
class __$UserInstrumentCopyWithImpl<$Res>
    implements _$UserInstrumentCopyWith<$Res> {
  __$UserInstrumentCopyWithImpl(this._self, this._then);

  final _UserInstrument _self;
  final $Res Function(_UserInstrument) _then;

/// Create a copy of UserInstrument
/// with the given fields replaced by the non-null parameter values.
@override @pragma('vm:prefer-inline') $Res call({Object? id = null,Object? name = null,Object? source = null,Object? rootNote = null,Object? cents = null,Object? confidence = null,Object? mode = null,Object? envelope = null,}) {
  return _then(_UserInstrument(
id: null == id ? _self.id : id // ignore: cast_nullable_to_non_nullable
as String,name: null == name ? _self.name : name // ignore: cast_nullable_to_non_nullable
as String,source: null == source ? _self.source : source // ignore: cast_nullable_to_non_nullable
as String,rootNote: null == rootNote ? _self.rootNote : rootNote // ignore: cast_nullable_to_non_nullable
as int,cents: null == cents ? _self.cents : cents // ignore: cast_nullable_to_non_nullable
as double,confidence: null == confidence ? _self.confidence : confidence // ignore: cast_nullable_to_non_nullable
as double,mode: null == mode ? _self.mode : mode // ignore: cast_nullable_to_non_nullable
as InstrumentMode,envelope: null == envelope ? _self.envelope : envelope // ignore: cast_nullable_to_non_nullable
as Envelope,
  ));
}

/// Create a copy of UserInstrument
/// with the given fields replaced by the non-null parameter values.
@override
@pragma('vm:prefer-inline')
$EnvelopeCopyWith<$Res> get envelope {
  
  return $EnvelopeCopyWith<$Res>(_self.envelope, (value) {
    return _then(_self.copyWith(envelope: value));
  });
}
}


/// @nodoc
mixin _$Envelope {

 double get a; double get d; double get s; double get r;
/// Create a copy of Envelope
/// with the given fields replaced by the non-null parameter values.
@JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
$EnvelopeCopyWith<Envelope> get copyWith => _$EnvelopeCopyWithImpl<Envelope>(this as Envelope, _$identity);

  /// Serializes this Envelope to a JSON map.
  Map<String, dynamic> toJson();


@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is Envelope&&(identical(other.a, a) || other.a == a)&&(identical(other.d, d) || other.d == d)&&(identical(other.s, s) || other.s == s)&&(identical(other.r, r) || other.r == r));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,a,d,s,r);

@override
String toString() {
  return 'Envelope(a: $a, d: $d, s: $s, r: $r)';
}


}

/// @nodoc
abstract mixin class $EnvelopeCopyWith<$Res>  {
  factory $EnvelopeCopyWith(Envelope value, $Res Function(Envelope) _then) = _$EnvelopeCopyWithImpl;
@useResult
$Res call({
 double a, double d, double s, double r
});




}
/// @nodoc
class _$EnvelopeCopyWithImpl<$Res>
    implements $EnvelopeCopyWith<$Res> {
  _$EnvelopeCopyWithImpl(this._self, this._then);

  final Envelope _self;
  final $Res Function(Envelope) _then;

/// Create a copy of Envelope
/// with the given fields replaced by the non-null parameter values.
@pragma('vm:prefer-inline') @override $Res call({Object? a = null,Object? d = null,Object? s = null,Object? r = null,}) {
  return _then(_self.copyWith(
a: null == a ? _self.a : a // ignore: cast_nullable_to_non_nullable
as double,d: null == d ? _self.d : d // ignore: cast_nullable_to_non_nullable
as double,s: null == s ? _self.s : s // ignore: cast_nullable_to_non_nullable
as double,r: null == r ? _self.r : r // ignore: cast_nullable_to_non_nullable
as double,
  ));
}

}


/// Adds pattern-matching-related methods to [Envelope].
extension EnvelopePatterns on Envelope {
/// A variant of `map` that fallback to returning `orElse`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeMap<TResult extends Object?>(TResult Function( _Envelope value)?  $default,{required TResult orElse(),}){
final _that = this;
switch (_that) {
case _Envelope() when $default != null:
return $default(_that);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// Callbacks receives the raw object, upcasted.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case final Subclass2 value:
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult map<TResult extends Object?>(TResult Function( _Envelope value)  $default,){
final _that = this;
switch (_that) {
case _Envelope():
return $default(_that);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `map` that fallback to returning `null`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? mapOrNull<TResult extends Object?>(TResult? Function( _Envelope value)?  $default,){
final _that = this;
switch (_that) {
case _Envelope() when $default != null:
return $default(_that);case _:
  return null;

}
}
/// A variant of `when` that fallback to an `orElse` callback.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeWhen<TResult extends Object?>(TResult Function( double a,  double d,  double s,  double r)?  $default,{required TResult orElse(),}) {final _that = this;
switch (_that) {
case _Envelope() when $default != null:
return $default(_that.a,_that.d,_that.s,_that.r);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// As opposed to `map`, this offers destructuring.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case Subclass2(:final field2):
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult when<TResult extends Object?>(TResult Function( double a,  double d,  double s,  double r)  $default,) {final _that = this;
switch (_that) {
case _Envelope():
return $default(_that.a,_that.d,_that.s,_that.r);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `when` that fallback to returning `null`
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? whenOrNull<TResult extends Object?>(TResult? Function( double a,  double d,  double s,  double r)?  $default,) {final _that = this;
switch (_that) {
case _Envelope() when $default != null:
return $default(_that.a,_that.d,_that.s,_that.r);case _:
  return null;

}
}

}

/// @nodoc
@JsonSerializable()

class _Envelope implements Envelope {
  const _Envelope({this.a = 0.005, this.d = 0.2, this.s = 0.8, this.r = 0.3});
  factory _Envelope.fromJson(Map<String, dynamic> json) => _$EnvelopeFromJson(json);

@override@JsonKey() final  double a;
@override@JsonKey() final  double d;
@override@JsonKey() final  double s;
@override@JsonKey() final  double r;

/// Create a copy of Envelope
/// with the given fields replaced by the non-null parameter values.
@override @JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
_$EnvelopeCopyWith<_Envelope> get copyWith => __$EnvelopeCopyWithImpl<_Envelope>(this, _$identity);

@override
Map<String, dynamic> toJson() {
  return _$EnvelopeToJson(this, );
}

@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is _Envelope&&(identical(other.a, a) || other.a == a)&&(identical(other.d, d) || other.d == d)&&(identical(other.s, s) || other.s == s)&&(identical(other.r, r) || other.r == r));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,a,d,s,r);

@override
String toString() {
  return 'Envelope(a: $a, d: $d, s: $s, r: $r)';
}


}

/// @nodoc
abstract mixin class _$EnvelopeCopyWith<$Res> implements $EnvelopeCopyWith<$Res> {
  factory _$EnvelopeCopyWith(_Envelope value, $Res Function(_Envelope) _then) = __$EnvelopeCopyWithImpl;
@override @useResult
$Res call({
 double a, double d, double s, double r
});




}
/// @nodoc
class __$EnvelopeCopyWithImpl<$Res>
    implements _$EnvelopeCopyWith<$Res> {
  __$EnvelopeCopyWithImpl(this._self, this._then);

  final _Envelope _self;
  final $Res Function(_Envelope) _then;

/// Create a copy of Envelope
/// with the given fields replaced by the non-null parameter values.
@override @pragma('vm:prefer-inline') $Res call({Object? a = null,Object? d = null,Object? s = null,Object? r = null,}) {
  return _then(_Envelope(
a: null == a ? _self.a : a // ignore: cast_nullable_to_non_nullable
as double,d: null == d ? _self.d : d // ignore: cast_nullable_to_non_nullable
as double,s: null == s ? _self.s : s // ignore: cast_nullable_to_non_nullable
as double,r: null == r ? _self.r : r // ignore: cast_nullable_to_non_nullable
as double,
  ));
}


}


/// @nodoc
mixin _$Master {

 double get gainDb; List<double> get eq3;/// Bypass EQ3 master (`FX_BYPASS track −1 slot 0`). Limiter không bypass được.
 bool get eq3Bypass; double get limiterCeilingDb;
/// Create a copy of Master
/// with the given fields replaced by the non-null parameter values.
@JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
$MasterCopyWith<Master> get copyWith => _$MasterCopyWithImpl<Master>(this as Master, _$identity);

  /// Serializes this Master to a JSON map.
  Map<String, dynamic> toJson();


@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is Master&&(identical(other.gainDb, gainDb) || other.gainDb == gainDb)&&const DeepCollectionEquality().equals(other.eq3, eq3)&&(identical(other.eq3Bypass, eq3Bypass) || other.eq3Bypass == eq3Bypass)&&(identical(other.limiterCeilingDb, limiterCeilingDb) || other.limiterCeilingDb == limiterCeilingDb));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,gainDb,const DeepCollectionEquality().hash(eq3),eq3Bypass,limiterCeilingDb);

@override
String toString() {
  return 'Master(gainDb: $gainDb, eq3: $eq3, eq3Bypass: $eq3Bypass, limiterCeilingDb: $limiterCeilingDb)';
}


}

/// @nodoc
abstract mixin class $MasterCopyWith<$Res>  {
  factory $MasterCopyWith(Master value, $Res Function(Master) _then) = _$MasterCopyWithImpl;
@useResult
$Res call({
 double gainDb, List<double> eq3, bool eq3Bypass, double limiterCeilingDb
});




}
/// @nodoc
class _$MasterCopyWithImpl<$Res>
    implements $MasterCopyWith<$Res> {
  _$MasterCopyWithImpl(this._self, this._then);

  final Master _self;
  final $Res Function(Master) _then;

/// Create a copy of Master
/// with the given fields replaced by the non-null parameter values.
@pragma('vm:prefer-inline') @override $Res call({Object? gainDb = null,Object? eq3 = null,Object? eq3Bypass = null,Object? limiterCeilingDb = null,}) {
  return _then(_self.copyWith(
gainDb: null == gainDb ? _self.gainDb : gainDb // ignore: cast_nullable_to_non_nullable
as double,eq3: null == eq3 ? _self.eq3 : eq3 // ignore: cast_nullable_to_non_nullable
as List<double>,eq3Bypass: null == eq3Bypass ? _self.eq3Bypass : eq3Bypass // ignore: cast_nullable_to_non_nullable
as bool,limiterCeilingDb: null == limiterCeilingDb ? _self.limiterCeilingDb : limiterCeilingDb // ignore: cast_nullable_to_non_nullable
as double,
  ));
}

}


/// Adds pattern-matching-related methods to [Master].
extension MasterPatterns on Master {
/// A variant of `map` that fallback to returning `orElse`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeMap<TResult extends Object?>(TResult Function( _Master value)?  $default,{required TResult orElse(),}){
final _that = this;
switch (_that) {
case _Master() when $default != null:
return $default(_that);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// Callbacks receives the raw object, upcasted.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case final Subclass2 value:
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult map<TResult extends Object?>(TResult Function( _Master value)  $default,){
final _that = this;
switch (_that) {
case _Master():
return $default(_that);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `map` that fallback to returning `null`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? mapOrNull<TResult extends Object?>(TResult? Function( _Master value)?  $default,){
final _that = this;
switch (_that) {
case _Master() when $default != null:
return $default(_that);case _:
  return null;

}
}
/// A variant of `when` that fallback to an `orElse` callback.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeWhen<TResult extends Object?>(TResult Function( double gainDb,  List<double> eq3,  bool eq3Bypass,  double limiterCeilingDb)?  $default,{required TResult orElse(),}) {final _that = this;
switch (_that) {
case _Master() when $default != null:
return $default(_that.gainDb,_that.eq3,_that.eq3Bypass,_that.limiterCeilingDb);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// As opposed to `map`, this offers destructuring.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case Subclass2(:final field2):
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult when<TResult extends Object?>(TResult Function( double gainDb,  List<double> eq3,  bool eq3Bypass,  double limiterCeilingDb)  $default,) {final _that = this;
switch (_that) {
case _Master():
return $default(_that.gainDb,_that.eq3,_that.eq3Bypass,_that.limiterCeilingDb);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `when` that fallback to returning `null`
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? whenOrNull<TResult extends Object?>(TResult? Function( double gainDb,  List<double> eq3,  bool eq3Bypass,  double limiterCeilingDb)?  $default,) {final _that = this;
switch (_that) {
case _Master() when $default != null:
return $default(_that.gainDb,_that.eq3,_that.eq3Bypass,_that.limiterCeilingDb);case _:
  return null;

}
}

}

/// @nodoc
@JsonSerializable()

class _Master implements Master {
  const _Master({this.gainDb = 0.0, final  List<double> eq3 = const <double>[0, 0, 0], this.eq3Bypass = false, this.limiterCeilingDb = -0.3}): _eq3 = eq3;
  factory _Master.fromJson(Map<String, dynamic> json) => _$MasterFromJson(json);

@override@JsonKey() final  double gainDb;
 final  List<double> _eq3;
@override@JsonKey() List<double> get eq3 {
  if (_eq3 is EqualUnmodifiableListView) return _eq3;
  // ignore: implicit_dynamic_type
  return EqualUnmodifiableListView(_eq3);
}

/// Bypass EQ3 master (`FX_BYPASS track −1 slot 0`). Limiter không bypass được.
@override@JsonKey() final  bool eq3Bypass;
@override@JsonKey() final  double limiterCeilingDb;

/// Create a copy of Master
/// with the given fields replaced by the non-null parameter values.
@override @JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
_$MasterCopyWith<_Master> get copyWith => __$MasterCopyWithImpl<_Master>(this, _$identity);

@override
Map<String, dynamic> toJson() {
  return _$MasterToJson(this, );
}

@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is _Master&&(identical(other.gainDb, gainDb) || other.gainDb == gainDb)&&const DeepCollectionEquality().equals(other._eq3, _eq3)&&(identical(other.eq3Bypass, eq3Bypass) || other.eq3Bypass == eq3Bypass)&&(identical(other.limiterCeilingDb, limiterCeilingDb) || other.limiterCeilingDb == limiterCeilingDb));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,gainDb,const DeepCollectionEquality().hash(_eq3),eq3Bypass,limiterCeilingDb);

@override
String toString() {
  return 'Master(gainDb: $gainDb, eq3: $eq3, eq3Bypass: $eq3Bypass, limiterCeilingDb: $limiterCeilingDb)';
}


}

/// @nodoc
abstract mixin class _$MasterCopyWith<$Res> implements $MasterCopyWith<$Res> {
  factory _$MasterCopyWith(_Master value, $Res Function(_Master) _then) = __$MasterCopyWithImpl;
@override @useResult
$Res call({
 double gainDb, List<double> eq3, bool eq3Bypass, double limiterCeilingDb
});




}
/// @nodoc
class __$MasterCopyWithImpl<$Res>
    implements _$MasterCopyWith<$Res> {
  __$MasterCopyWithImpl(this._self, this._then);

  final _Master _self;
  final $Res Function(_Master) _then;

/// Create a copy of Master
/// with the given fields replaced by the non-null parameter values.
@override @pragma('vm:prefer-inline') $Res call({Object? gainDb = null,Object? eq3 = null,Object? eq3Bypass = null,Object? limiterCeilingDb = null,}) {
  return _then(_Master(
gainDb: null == gainDb ? _self.gainDb : gainDb // ignore: cast_nullable_to_non_nullable
as double,eq3: null == eq3 ? _self._eq3 : eq3 // ignore: cast_nullable_to_non_nullable
as List<double>,eq3Bypass: null == eq3Bypass ? _self.eq3Bypass : eq3Bypass // ignore: cast_nullable_to_non_nullable
as bool,limiterCeilingDb: null == limiterCeilingDb ? _self.limiterCeilingDb : limiterCeilingDb // ignore: cast_nullable_to_non_nullable
as double,
  ));
}


}


/// @nodoc
mixin _$MidiMapping {

 MidiSource get src;/// `{kind:"clip",track,slot}` | `{kind:"fx",track,slot,param}` | … — kiểu cụ thể chốt ở P4,
/// tạm giữ nguyên Map để không mất dữ liệu khi round-trip.
 Map<String, dynamic> get target;
/// Create a copy of MidiMapping
/// with the given fields replaced by the non-null parameter values.
@JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
$MidiMappingCopyWith<MidiMapping> get copyWith => _$MidiMappingCopyWithImpl<MidiMapping>(this as MidiMapping, _$identity);

  /// Serializes this MidiMapping to a JSON map.
  Map<String, dynamic> toJson();


@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is MidiMapping&&(identical(other.src, src) || other.src == src)&&const DeepCollectionEquality().equals(other.target, target));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,src,const DeepCollectionEquality().hash(target));

@override
String toString() {
  return 'MidiMapping(src: $src, target: $target)';
}


}

/// @nodoc
abstract mixin class $MidiMappingCopyWith<$Res>  {
  factory $MidiMappingCopyWith(MidiMapping value, $Res Function(MidiMapping) _then) = _$MidiMappingCopyWithImpl;
@useResult
$Res call({
 MidiSource src, Map<String, dynamic> target
});


$MidiSourceCopyWith<$Res> get src;

}
/// @nodoc
class _$MidiMappingCopyWithImpl<$Res>
    implements $MidiMappingCopyWith<$Res> {
  _$MidiMappingCopyWithImpl(this._self, this._then);

  final MidiMapping _self;
  final $Res Function(MidiMapping) _then;

/// Create a copy of MidiMapping
/// with the given fields replaced by the non-null parameter values.
@pragma('vm:prefer-inline') @override $Res call({Object? src = null,Object? target = null,}) {
  return _then(_self.copyWith(
src: null == src ? _self.src : src // ignore: cast_nullable_to_non_nullable
as MidiSource,target: null == target ? _self.target : target // ignore: cast_nullable_to_non_nullable
as Map<String, dynamic>,
  ));
}
/// Create a copy of MidiMapping
/// with the given fields replaced by the non-null parameter values.
@override
@pragma('vm:prefer-inline')
$MidiSourceCopyWith<$Res> get src {
  
  return $MidiSourceCopyWith<$Res>(_self.src, (value) {
    return _then(_self.copyWith(src: value));
  });
}
}


/// Adds pattern-matching-related methods to [MidiMapping].
extension MidiMappingPatterns on MidiMapping {
/// A variant of `map` that fallback to returning `orElse`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeMap<TResult extends Object?>(TResult Function( _MidiMapping value)?  $default,{required TResult orElse(),}){
final _that = this;
switch (_that) {
case _MidiMapping() when $default != null:
return $default(_that);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// Callbacks receives the raw object, upcasted.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case final Subclass2 value:
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult map<TResult extends Object?>(TResult Function( _MidiMapping value)  $default,){
final _that = this;
switch (_that) {
case _MidiMapping():
return $default(_that);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `map` that fallback to returning `null`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? mapOrNull<TResult extends Object?>(TResult? Function( _MidiMapping value)?  $default,){
final _that = this;
switch (_that) {
case _MidiMapping() when $default != null:
return $default(_that);case _:
  return null;

}
}
/// A variant of `when` that fallback to an `orElse` callback.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeWhen<TResult extends Object?>(TResult Function( MidiSource src,  Map<String, dynamic> target)?  $default,{required TResult orElse(),}) {final _that = this;
switch (_that) {
case _MidiMapping() when $default != null:
return $default(_that.src,_that.target);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// As opposed to `map`, this offers destructuring.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case Subclass2(:final field2):
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult when<TResult extends Object?>(TResult Function( MidiSource src,  Map<String, dynamic> target)  $default,) {final _that = this;
switch (_that) {
case _MidiMapping():
return $default(_that.src,_that.target);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `when` that fallback to returning `null`
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? whenOrNull<TResult extends Object?>(TResult? Function( MidiSource src,  Map<String, dynamic> target)?  $default,) {final _that = this;
switch (_that) {
case _MidiMapping() when $default != null:
return $default(_that.src,_that.target);case _:
  return null;

}
}

}

/// @nodoc
@JsonSerializable()

class _MidiMapping implements MidiMapping {
  const _MidiMapping({required this.src, required final  Map<String, dynamic> target}): _target = target;
  factory _MidiMapping.fromJson(Map<String, dynamic> json) => _$MidiMappingFromJson(json);

@override final  MidiSource src;
/// `{kind:"clip",track,slot}` | `{kind:"fx",track,slot,param}` | … — kiểu cụ thể chốt ở P4,
/// tạm giữ nguyên Map để không mất dữ liệu khi round-trip.
 final  Map<String, dynamic> _target;
/// `{kind:"clip",track,slot}` | `{kind:"fx",track,slot,param}` | … — kiểu cụ thể chốt ở P4,
/// tạm giữ nguyên Map để không mất dữ liệu khi round-trip.
@override Map<String, dynamic> get target {
  if (_target is EqualUnmodifiableMapView) return _target;
  // ignore: implicit_dynamic_type
  return EqualUnmodifiableMapView(_target);
}


/// Create a copy of MidiMapping
/// with the given fields replaced by the non-null parameter values.
@override @JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
_$MidiMappingCopyWith<_MidiMapping> get copyWith => __$MidiMappingCopyWithImpl<_MidiMapping>(this, _$identity);

@override
Map<String, dynamic> toJson() {
  return _$MidiMappingToJson(this, );
}

@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is _MidiMapping&&(identical(other.src, src) || other.src == src)&&const DeepCollectionEquality().equals(other._target, _target));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,src,const DeepCollectionEquality().hash(_target));

@override
String toString() {
  return 'MidiMapping(src: $src, target: $target)';
}


}

/// @nodoc
abstract mixin class _$MidiMappingCopyWith<$Res> implements $MidiMappingCopyWith<$Res> {
  factory _$MidiMappingCopyWith(_MidiMapping value, $Res Function(_MidiMapping) _then) = __$MidiMappingCopyWithImpl;
@override @useResult
$Res call({
 MidiSource src, Map<String, dynamic> target
});


@override $MidiSourceCopyWith<$Res> get src;

}
/// @nodoc
class __$MidiMappingCopyWithImpl<$Res>
    implements _$MidiMappingCopyWith<$Res> {
  __$MidiMappingCopyWithImpl(this._self, this._then);

  final _MidiMapping _self;
  final $Res Function(_MidiMapping) _then;

/// Create a copy of MidiMapping
/// with the given fields replaced by the non-null parameter values.
@override @pragma('vm:prefer-inline') $Res call({Object? src = null,Object? target = null,}) {
  return _then(_MidiMapping(
src: null == src ? _self.src : src // ignore: cast_nullable_to_non_nullable
as MidiSource,target: null == target ? _self._target : target // ignore: cast_nullable_to_non_nullable
as Map<String, dynamic>,
  ));
}

/// Create a copy of MidiMapping
/// with the given fields replaced by the non-null parameter values.
@override
@pragma('vm:prefer-inline')
$MidiSourceCopyWith<$Res> get src {
  
  return $MidiSourceCopyWith<$Res>(_self.src, (value) {
    return _then(_self.copyWith(src: value));
  });
}
}


/// @nodoc
mixin _$MidiSource {

 String get device;/// "note" | "cc"
 String get kind; int get channel; int get number;
/// Create a copy of MidiSource
/// with the given fields replaced by the non-null parameter values.
@JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
$MidiSourceCopyWith<MidiSource> get copyWith => _$MidiSourceCopyWithImpl<MidiSource>(this as MidiSource, _$identity);

  /// Serializes this MidiSource to a JSON map.
  Map<String, dynamic> toJson();


@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is MidiSource&&(identical(other.device, device) || other.device == device)&&(identical(other.kind, kind) || other.kind == kind)&&(identical(other.channel, channel) || other.channel == channel)&&(identical(other.number, number) || other.number == number));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,device,kind,channel,number);

@override
String toString() {
  return 'MidiSource(device: $device, kind: $kind, channel: $channel, number: $number)';
}


}

/// @nodoc
abstract mixin class $MidiSourceCopyWith<$Res>  {
  factory $MidiSourceCopyWith(MidiSource value, $Res Function(MidiSource) _then) = _$MidiSourceCopyWithImpl;
@useResult
$Res call({
 String device, String kind, int channel, int number
});




}
/// @nodoc
class _$MidiSourceCopyWithImpl<$Res>
    implements $MidiSourceCopyWith<$Res> {
  _$MidiSourceCopyWithImpl(this._self, this._then);

  final MidiSource _self;
  final $Res Function(MidiSource) _then;

/// Create a copy of MidiSource
/// with the given fields replaced by the non-null parameter values.
@pragma('vm:prefer-inline') @override $Res call({Object? device = null,Object? kind = null,Object? channel = null,Object? number = null,}) {
  return _then(_self.copyWith(
device: null == device ? _self.device : device // ignore: cast_nullable_to_non_nullable
as String,kind: null == kind ? _self.kind : kind // ignore: cast_nullable_to_non_nullable
as String,channel: null == channel ? _self.channel : channel // ignore: cast_nullable_to_non_nullable
as int,number: null == number ? _self.number : number // ignore: cast_nullable_to_non_nullable
as int,
  ));
}

}


/// Adds pattern-matching-related methods to [MidiSource].
extension MidiSourcePatterns on MidiSource {
/// A variant of `map` that fallback to returning `orElse`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeMap<TResult extends Object?>(TResult Function( _MidiSource value)?  $default,{required TResult orElse(),}){
final _that = this;
switch (_that) {
case _MidiSource() when $default != null:
return $default(_that);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// Callbacks receives the raw object, upcasted.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case final Subclass2 value:
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult map<TResult extends Object?>(TResult Function( _MidiSource value)  $default,){
final _that = this;
switch (_that) {
case _MidiSource():
return $default(_that);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `map` that fallback to returning `null`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? mapOrNull<TResult extends Object?>(TResult? Function( _MidiSource value)?  $default,){
final _that = this;
switch (_that) {
case _MidiSource() when $default != null:
return $default(_that);case _:
  return null;

}
}
/// A variant of `when` that fallback to an `orElse` callback.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeWhen<TResult extends Object?>(TResult Function( String device,  String kind,  int channel,  int number)?  $default,{required TResult orElse(),}) {final _that = this;
switch (_that) {
case _MidiSource() when $default != null:
return $default(_that.device,_that.kind,_that.channel,_that.number);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// As opposed to `map`, this offers destructuring.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case Subclass2(:final field2):
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult when<TResult extends Object?>(TResult Function( String device,  String kind,  int channel,  int number)  $default,) {final _that = this;
switch (_that) {
case _MidiSource():
return $default(_that.device,_that.kind,_that.channel,_that.number);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `when` that fallback to returning `null`
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? whenOrNull<TResult extends Object?>(TResult? Function( String device,  String kind,  int channel,  int number)?  $default,) {final _that = this;
switch (_that) {
case _MidiSource() when $default != null:
return $default(_that.device,_that.kind,_that.channel,_that.number);case _:
  return null;

}
}

}

/// @nodoc
@JsonSerializable()

class _MidiSource implements MidiSource {
  const _MidiSource({required this.device, required this.kind, required this.channel, required this.number});
  factory _MidiSource.fromJson(Map<String, dynamic> json) => _$MidiSourceFromJson(json);

@override final  String device;
/// "note" | "cc"
@override final  String kind;
@override final  int channel;
@override final  int number;

/// Create a copy of MidiSource
/// with the given fields replaced by the non-null parameter values.
@override @JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
_$MidiSourceCopyWith<_MidiSource> get copyWith => __$MidiSourceCopyWithImpl<_MidiSource>(this, _$identity);

@override
Map<String, dynamic> toJson() {
  return _$MidiSourceToJson(this, );
}

@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is _MidiSource&&(identical(other.device, device) || other.device == device)&&(identical(other.kind, kind) || other.kind == kind)&&(identical(other.channel, channel) || other.channel == channel)&&(identical(other.number, number) || other.number == number));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,device,kind,channel,number);

@override
String toString() {
  return 'MidiSource(device: $device, kind: $kind, channel: $channel, number: $number)';
}


}

/// @nodoc
abstract mixin class _$MidiSourceCopyWith<$Res> implements $MidiSourceCopyWith<$Res> {
  factory _$MidiSourceCopyWith(_MidiSource value, $Res Function(_MidiSource) _then) = __$MidiSourceCopyWithImpl;
@override @useResult
$Res call({
 String device, String kind, int channel, int number
});




}
/// @nodoc
class __$MidiSourceCopyWithImpl<$Res>
    implements _$MidiSourceCopyWith<$Res> {
  __$MidiSourceCopyWithImpl(this._self, this._then);

  final _MidiSource _self;
  final $Res Function(_MidiSource) _then;

/// Create a copy of MidiSource
/// with the given fields replaced by the non-null parameter values.
@override @pragma('vm:prefer-inline') $Res call({Object? device = null,Object? kind = null,Object? channel = null,Object? number = null,}) {
  return _then(_MidiSource(
device: null == device ? _self.device : device // ignore: cast_nullable_to_non_nullable
as String,kind: null == kind ? _self.kind : kind // ignore: cast_nullable_to_non_nullable
as String,channel: null == channel ? _self.channel : channel // ignore: cast_nullable_to_non_nullable
as int,number: null == number ? _self.number : number // ignore: cast_nullable_to_non_nullable
as int,
  ));
}


}


/// @nodoc
mixin _$LinkSettings {

 bool get enabled; bool get startStopSync;
/// Create a copy of LinkSettings
/// with the given fields replaced by the non-null parameter values.
@JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
$LinkSettingsCopyWith<LinkSettings> get copyWith => _$LinkSettingsCopyWithImpl<LinkSettings>(this as LinkSettings, _$identity);

  /// Serializes this LinkSettings to a JSON map.
  Map<String, dynamic> toJson();


@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is LinkSettings&&(identical(other.enabled, enabled) || other.enabled == enabled)&&(identical(other.startStopSync, startStopSync) || other.startStopSync == startStopSync));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,enabled,startStopSync);

@override
String toString() {
  return 'LinkSettings(enabled: $enabled, startStopSync: $startStopSync)';
}


}

/// @nodoc
abstract mixin class $LinkSettingsCopyWith<$Res>  {
  factory $LinkSettingsCopyWith(LinkSettings value, $Res Function(LinkSettings) _then) = _$LinkSettingsCopyWithImpl;
@useResult
$Res call({
 bool enabled, bool startStopSync
});




}
/// @nodoc
class _$LinkSettingsCopyWithImpl<$Res>
    implements $LinkSettingsCopyWith<$Res> {
  _$LinkSettingsCopyWithImpl(this._self, this._then);

  final LinkSettings _self;
  final $Res Function(LinkSettings) _then;

/// Create a copy of LinkSettings
/// with the given fields replaced by the non-null parameter values.
@pragma('vm:prefer-inline') @override $Res call({Object? enabled = null,Object? startStopSync = null,}) {
  return _then(_self.copyWith(
enabled: null == enabled ? _self.enabled : enabled // ignore: cast_nullable_to_non_nullable
as bool,startStopSync: null == startStopSync ? _self.startStopSync : startStopSync // ignore: cast_nullable_to_non_nullable
as bool,
  ));
}

}


/// Adds pattern-matching-related methods to [LinkSettings].
extension LinkSettingsPatterns on LinkSettings {
/// A variant of `map` that fallback to returning `orElse`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeMap<TResult extends Object?>(TResult Function( _LinkSettings value)?  $default,{required TResult orElse(),}){
final _that = this;
switch (_that) {
case _LinkSettings() when $default != null:
return $default(_that);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// Callbacks receives the raw object, upcasted.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case final Subclass2 value:
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult map<TResult extends Object?>(TResult Function( _LinkSettings value)  $default,){
final _that = this;
switch (_that) {
case _LinkSettings():
return $default(_that);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `map` that fallback to returning `null`.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case final Subclass value:
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? mapOrNull<TResult extends Object?>(TResult? Function( _LinkSettings value)?  $default,){
final _that = this;
switch (_that) {
case _LinkSettings() when $default != null:
return $default(_that);case _:
  return null;

}
}
/// A variant of `when` that fallback to an `orElse` callback.
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return orElse();
/// }
/// ```

@optionalTypeArgs TResult maybeWhen<TResult extends Object?>(TResult Function( bool enabled,  bool startStopSync)?  $default,{required TResult orElse(),}) {final _that = this;
switch (_that) {
case _LinkSettings() when $default != null:
return $default(_that.enabled,_that.startStopSync);case _:
  return orElse();

}
}
/// A `switch`-like method, using callbacks.
///
/// As opposed to `map`, this offers destructuring.
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case Subclass2(:final field2):
///     return ...;
/// }
/// ```

@optionalTypeArgs TResult when<TResult extends Object?>(TResult Function( bool enabled,  bool startStopSync)  $default,) {final _that = this;
switch (_that) {
case _LinkSettings():
return $default(_that.enabled,_that.startStopSync);case _:
  throw StateError('Unexpected subclass');

}
}
/// A variant of `when` that fallback to returning `null`
///
/// It is equivalent to doing:
/// ```dart
/// switch (sealedClass) {
///   case Subclass(:final field):
///     return ...;
///   case _:
///     return null;
/// }
/// ```

@optionalTypeArgs TResult? whenOrNull<TResult extends Object?>(TResult? Function( bool enabled,  bool startStopSync)?  $default,) {final _that = this;
switch (_that) {
case _LinkSettings() when $default != null:
return $default(_that.enabled,_that.startStopSync);case _:
  return null;

}
}

}

/// @nodoc
@JsonSerializable()

class _LinkSettings implements LinkSettings {
  const _LinkSettings({this.enabled = false, this.startStopSync = true});
  factory _LinkSettings.fromJson(Map<String, dynamic> json) => _$LinkSettingsFromJson(json);

@override@JsonKey() final  bool enabled;
@override@JsonKey() final  bool startStopSync;

/// Create a copy of LinkSettings
/// with the given fields replaced by the non-null parameter values.
@override @JsonKey(includeFromJson: false, includeToJson: false)
@pragma('vm:prefer-inline')
_$LinkSettingsCopyWith<_LinkSettings> get copyWith => __$LinkSettingsCopyWithImpl<_LinkSettings>(this, _$identity);

@override
Map<String, dynamic> toJson() {
  return _$LinkSettingsToJson(this, );
}

@override
bool operator ==(Object other) {
  return identical(this, other) || (other.runtimeType == runtimeType&&other is _LinkSettings&&(identical(other.enabled, enabled) || other.enabled == enabled)&&(identical(other.startStopSync, startStopSync) || other.startStopSync == startStopSync));
}

@JsonKey(includeFromJson: false, includeToJson: false)
@override
int get hashCode => Object.hash(runtimeType,enabled,startStopSync);

@override
String toString() {
  return 'LinkSettings(enabled: $enabled, startStopSync: $startStopSync)';
}


}

/// @nodoc
abstract mixin class _$LinkSettingsCopyWith<$Res> implements $LinkSettingsCopyWith<$Res> {
  factory _$LinkSettingsCopyWith(_LinkSettings value, $Res Function(_LinkSettings) _then) = __$LinkSettingsCopyWithImpl;
@override @useResult
$Res call({
 bool enabled, bool startStopSync
});




}
/// @nodoc
class __$LinkSettingsCopyWithImpl<$Res>
    implements _$LinkSettingsCopyWith<$Res> {
  __$LinkSettingsCopyWithImpl(this._self, this._then);

  final _LinkSettings _self;
  final $Res Function(_LinkSettings) _then;

/// Create a copy of LinkSettings
/// with the given fields replaced by the non-null parameter values.
@override @pragma('vm:prefer-inline') $Res call({Object? enabled = null,Object? startStopSync = null,}) {
  return _then(_LinkSettings(
enabled: null == enabled ? _self.enabled : enabled // ignore: cast_nullable_to_non_nullable
as bool,startStopSync: null == startStopSync ? _self.startStopSync : startStopSync // ignore: cast_nullable_to_non_nullable
as bool,
  ));
}


}

// dart format on
