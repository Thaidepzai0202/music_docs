import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_localizations/flutter_localizations.dart';
import 'package:intl/intl.dart' as intl;

import 'app_localizations_en.dart';
import 'app_localizations_vi.dart';

// ignore_for_file: type=lint

/// Callers can lookup localized strings with an instance of AppLocalizations
/// returned by `AppLocalizations.of(context)`.
///
/// Applications need to include `AppLocalizations.delegate()` in their app's
/// `localizationDelegates` list, and the locales they support in the app's
/// `supportedLocales` list. For example:
///
/// ```dart
/// import 'l10n/app_localizations.dart';
///
/// return MaterialApp(
///   localizationsDelegates: AppLocalizations.localizationsDelegates,
///   supportedLocales: AppLocalizations.supportedLocales,
///   home: MyApplicationHome(),
/// );
/// ```
///
/// ## Update pubspec.yaml
///
/// Please make sure to update your pubspec.yaml to include the following
/// packages:
///
/// ```yaml
/// dependencies:
///   # Internationalization support.
///   flutter_localizations:
///     sdk: flutter
///   intl: any # Use the pinned version from flutter_localizations
///
///   # Rest of dependencies
/// ```
///
/// ## iOS Applications
///
/// iOS applications define key application metadata, including supported
/// locales, in an Info.plist file that is built into the application bundle.
/// To configure the locales supported by your app, you’ll need to edit this
/// file.
///
/// First, open your project’s ios/Runner.xcworkspace Xcode workspace file.
/// Then, in the Project Navigator, open the Info.plist file under the Runner
/// project’s Runner folder.
///
/// Next, select the Information Property List item, select Add Item from the
/// Editor menu, then select Localizations from the pop-up menu.
///
/// Select and expand the newly-created Localizations item then, for each
/// locale your application supports, add a new item and select the locale
/// you wish to add from the pop-up menu in the Value field. This list should
/// be consistent with the languages listed in the AppLocalizations.supportedLocales
/// property.
abstract class AppLocalizations {
  AppLocalizations(String locale) : localeName = intl.Intl.canonicalizedLocale(locale.toString());

  final String localeName;

  static AppLocalizations of(BuildContext context) {
    return Localizations.of<AppLocalizations>(context, AppLocalizations)!;
  }

  static const LocalizationsDelegate<AppLocalizations> delegate = _AppLocalizationsDelegate();

  /// A list of this localizations delegate along with the default localizations
  /// delegates.
  ///
  /// Returns a list of localizations delegates containing this delegate along with
  /// GlobalMaterialLocalizations.delegate, GlobalCupertinoLocalizations.delegate,
  /// and GlobalWidgetsLocalizations.delegate.
  ///
  /// Additional delegates can be added by appending to this list in
  /// MaterialApp. This list does not have to be used at all if a custom list
  /// of delegates is preferred or required.
  static const List<LocalizationsDelegate<dynamic>> localizationsDelegates = <LocalizationsDelegate<dynamic>>[
    delegate,
    GlobalMaterialLocalizations.delegate,
    GlobalCupertinoLocalizations.delegate,
    GlobalWidgetsLocalizations.delegate,
  ];

  /// A list of this localizations delegate's supported locales.
  static const List<Locale> supportedLocales = <Locale>[Locale('en'), Locale('vi')];

  /// No description provided for @aboutApi.
  ///
  /// In en, this message translates to:
  /// **'API'**
  String get aboutApi;

  /// No description provided for @aboutBuffer.
  ///
  /// In en, this message translates to:
  /// **'Buffer'**
  String get aboutBuffer;

  /// No description provided for @aboutDungBangFlutterEngineC.
  ///
  /// In en, this message translates to:
  /// **'Built with Flutter + a C++ engine (JUCE). iPad only, landscape.'**
  String get aboutDungBangFlutterEngineC;

  /// No description provided for @aboutEngine.
  ///
  /// In en, this message translates to:
  /// **'Engine'**
  String get aboutEngine;

  /// No description provided for @aboutEngineGiaLoopcoreFake.
  ///
  /// In en, this message translates to:
  /// **'Fake engine (LOOPCORE_FAKE)'**
  String get aboutEngineGiaLoopcoreFake;

  /// No description provided for @aboutFrames.
  ///
  /// In en, this message translates to:
  /// **'{frames} frames'**
  String aboutFrames(Object frames);

  /// No description provided for @aboutLoai.
  ///
  /// In en, this message translates to:
  /// **'Type'**
  String get aboutLoai;

  /// No description provided for @aboutLoopCore.
  ///
  /// In en, this message translates to:
  /// **'LoopCore (JUCE)'**
  String get aboutLoopCore;

  /// No description provided for @aboutPhienBan.
  ///
  /// In en, this message translates to:
  /// **'Version {p0} ({p1})'**
  String aboutPhienBan(Object p0, Object p1);

  /// No description provided for @aboutSampleRate.
  ///
  /// In en, this message translates to:
  /// **'Sample rate'**
  String get aboutSampleRate;

  /// No description provided for @aboutThietBi.
  ///
  /// In en, this message translates to:
  /// **'Device'**
  String get aboutThietBi;

  /// No description provided for @audioAudioChuaBat.
  ///
  /// In en, this message translates to:
  /// **'Audio is off'**
  String get audioAudioChuaBat;

  /// No description provided for @audioBars.
  ///
  /// In en, this message translates to:
  /// **'{count, plural, =1{1 bar} other{{count} bars}}'**
  String audioBars(int count);

  /// No description provided for @audioBuffer.
  ///
  /// In en, this message translates to:
  /// **'Audio buffer'**
  String get audioBuffer;

  /// No description provided for @audioChoi.
  ///
  /// In en, this message translates to:
  /// **'Playing'**
  String get audioChoi;

  /// No description provided for @audioDangBatThuDuoc.
  ///
  /// In en, this message translates to:
  /// **'On — recording available'**
  String get audioDangBatThuDuoc;

  /// No description provided for @audioDangChay.
  ///
  /// In en, this message translates to:
  /// **'Running: {frames} frames ({ms}) @ {hz} Hz'**
  String audioDangChay(Object frames, Object ms, Object hz);

  /// No description provided for @audioKhongDoiDuocBuffer.
  ///
  /// In en, this message translates to:
  /// **'Couldn\'t change the buffer: {err}'**
  String audioKhongDoiDuocBuffer(Object err);

  /// No description provided for @audioLuonBat.
  ///
  /// In en, this message translates to:
  /// **'On'**
  String get audioLuonBat;

  /// No description provided for @audioMic.
  ///
  /// In en, this message translates to:
  /// **'Microphone'**
  String get audioMic;

  /// No description provided for @audioMonitorMacDinhChoTrack.
  ///
  /// In en, this message translates to:
  /// **'Default monitor for new audio tracks'**
  String get audioMonitorMacDinhChoTrack;

  /// No description provided for @audioNhoHonTreItHon.
  ///
  /// In en, this message translates to:
  /// **'Smaller = lower latency but more CPU. {p0}'**
  String audioNhoHonTreItHon(Object p0);

  /// No description provided for @audioQuantizeNotKhiThuMidi.
  ///
  /// In en, this message translates to:
  /// **'Quantize notes when recording MIDI'**
  String get audioQuantizeNotKhiThuMidi;

  /// No description provided for @audioRungNheKhiLaunchThu.
  ///
  /// In en, this message translates to:
  /// **'Haptics on launch / record'**
  String get audioRungNheKhiLaunchThu;

  /// No description provided for @audioSoBarKhiThuVao.
  ///
  /// In en, this message translates to:
  /// **'Recording length'**
  String get audioSoBarKhiThuVao;

  /// No description provided for @audioThuAm.
  ///
  /// In en, this message translates to:
  /// **'Recording'**
  String get audioThuAm;

  /// No description provided for @audioTuDo.
  ///
  /// In en, this message translates to:
  /// **'Free'**
  String get audioTuDo;

  /// No description provided for @audioTuDoGhiChu.
  ///
  /// In en, this message translates to:
  /// **'Free: tap the cell again (or LOOP) to finish the take'**
  String get audioTuDoGhiChu;

  /// No description provided for @audioTuDong.
  ///
  /// In en, this message translates to:
  /// **'Auto'**
  String get audioTuDong;

  /// No description provided for @audioTuDongNgheMicroKhi.
  ///
  /// In en, this message translates to:
  /// **'Auto: hear the mic while the track is armed. Use headphones to avoid feedback.'**
  String get audioTuDongNgheMicroKhi;

  /// No description provided for @bannerAudioBiNgatCuocGoi.
  ///
  /// In en, this message translates to:
  /// **'Audio interrupted (call, Siri…)'**
  String get bannerAudioBiNgatCuocGoi;

  /// No description provided for @bannerDaGiaiPhongBoNho.
  ///
  /// In en, this message translates to:
  /// **'Memory freed ({mb} MB in use)'**
  String bannerDaGiaiPhongBoNho(Object mb);

  /// No description provided for @bannerDangDungTaiNgheBluetooth.
  ///
  /// In en, this message translates to:
  /// **'Bluetooth headphones: high latency, not suited for recording or playing in time.'**
  String get bannerDangDungTaiNgheBluetooth;

  /// No description provided for @bannerKhongOverdubDuoc.
  ///
  /// In en, this message translates to:
  /// **'Can\'t overdub cell {track}·{slot}: the audio clip is in Re-Pitch at a different tempo or not ready yet. The clip keeps playing.'**
  String bannerKhongOverdubDuoc(Object track, Object slot);

  /// No description provided for @bannerLoiEngine.
  ///
  /// In en, this message translates to:
  /// **'Engine error: {p0}'**
  String bannerLoiEngine(Object p0);

  /// No description provided for @browserAssignHint.
  ///
  /// In en, this message translates to:
  /// **'+ = {action}'**
  String browserAssignHint(Object action);

  /// No description provided for @browserDaThemVaoO.
  ///
  /// In en, this message translates to:
  /// **'Added \"{p0}\" to cell {p1}'**
  String browserDaThemVaoO(Object p0, Object p1);

  /// No description provided for @browserGan.
  ///
  /// In en, this message translates to:
  /// **'assign → {trackName}'**
  String browserGan(Object trackName);

  /// No description provided for @browserKhongDocDuocThuVien.
  ///
  /// In en, this message translates to:
  /// **'Couldn\'t read the library: {e}'**
  String browserKhongDocDuocThuVien(Object e);

  /// No description provided for @browserKit.
  ///
  /// In en, this message translates to:
  /// **'Kit'**
  String get browserKit;

  /// No description provided for @browserLoop.
  ///
  /// In en, this message translates to:
  /// **'Loop'**
  String get browserLoop;

  /// No description provided for @browserLoopInfo.
  ///
  /// In en, this message translates to:
  /// **'{bpm} BPM · {beats, plural, =1{1 beat} other{{beats} beats}}'**
  String browserLoopInfo(Object bpm, int beats);

  /// No description provided for @browserNhacCu.
  ///
  /// In en, this message translates to:
  /// **'Instruments'**
  String get browserNhacCu;

  /// No description provided for @browserSection.
  ///
  /// In en, this message translates to:
  /// **'{title} ({count})'**
  String browserSection(Object title, Object count);

  /// No description provided for @browserThemVao.
  ///
  /// In en, this message translates to:
  /// **'add to {trackName}'**
  String browserThemVao(Object trackName);

  /// No description provided for @browserTrackNayKhongConO.
  ///
  /// In en, this message translates to:
  /// **'No empty cells left on this track'**
  String get browserTrackNayKhongConO;

  /// No description provided for @clipClear.
  ///
  /// In en, this message translates to:
  /// **'Clear'**
  String get clipClear;

  /// No description provided for @clipDoDaiClip.
  ///
  /// In en, this message translates to:
  /// **'Clip length'**
  String get clipDoDaiClip;

  /// No description provided for @clipDrumsHint.
  ///
  /// In en, this message translates to:
  /// **'Drum loops sound more natural with Re-Pitch'**
  String get clipDrumsHint;

  /// No description provided for @clipGain.
  ///
  /// In en, this message translates to:
  /// **'Clip gain: {db} dB'**
  String clipGain(Object db);

  /// No description provided for @clipHoanTacOverdub.
  ///
  /// In en, this message translates to:
  /// **'Undo overdub'**
  String get clipHoanTacOverdub;

  /// No description provided for @clipHoanTacSua.
  ///
  /// In en, this message translates to:
  /// **'Undo'**
  String get clipHoanTacSua;

  /// No description provided for @clipLamLai.
  ///
  /// In en, this message translates to:
  /// **'Redo'**
  String get clipLamLai;

  /// No description provided for @clipLoopTuSBeatGoc.
  ///
  /// In en, this message translates to:
  /// **'Loop from {p0} s · {p1} beats · original {p2} BPM'**
  String clipLoopTuSBeatGoc(Object p0, Object p1, Object p2);

  /// No description provided for @clipLuoi.
  ///
  /// In en, this message translates to:
  /// **'Grid'**
  String get clipLuoi;

  /// No description provided for @clipModeChon.
  ///
  /// In en, this message translates to:
  /// **'Select'**
  String get clipModeChon;

  /// No description provided for @clipModeVe.
  ///
  /// In en, this message translates to:
  /// **'Draw'**
  String get clipModeVe;

  /// No description provided for @clipNotBeat.
  ///
  /// In en, this message translates to:
  /// **'{p0, plural, =1{1 note} other{{p0} notes}} · {p1} beats'**
  String clipNotBeat(int p0, Object p1);

  /// No description provided for @clipQuangTamLen.
  ///
  /// In en, this message translates to:
  /// **'Octave up'**
  String get clipQuangTamLen;

  /// No description provided for @clipQuangTamXuong.
  ///
  /// In en, this message translates to:
  /// **'Octave down'**
  String get clipQuangTamXuong;

  /// No description provided for @clipQuantize.
  ///
  /// In en, this message translates to:
  /// **'Quantize'**
  String get clipQuantize;

  /// No description provided for @clipQuantizeGrid.
  ///
  /// In en, this message translates to:
  /// **'Quantize {grid}'**
  String clipQuantizeGrid(Object grid);

  /// No description provided for @clipWarp.
  ///
  /// In en, this message translates to:
  /// **'Warp'**
  String get clipWarp;

  /// No description provided for @clipXoaNotDaChon.
  ///
  /// In en, this message translates to:
  /// **'Delete selected ({p0})'**
  String clipXoaNotDaChon(Object p0);

  /// No description provided for @clipZoom.
  ///
  /// In en, this message translates to:
  /// **'Zoom'**
  String get clipZoom;

  /// No description provided for @commonOk.
  ///
  /// In en, this message translates to:
  /// **'OK'**
  String get commonOk;

  /// No description provided for @demoName.
  ///
  /// In en, this message translates to:
  /// **'{token, select, demo{Demo} drums{Drums} bass{Bass} keys{Keys} lead{Lead} beatA{Beat A} beatB{Beat B} fill{Fill} halfTime{Half-time} bassA{Bass A} bassB{Bass B} walk{Walk} chords{Chords} stab{Stab} melody{Melody} hook{Hook} other{{token}}}'**
  String demoName(String token);

  /// No description provided for @errorText.
  ///
  /// In en, this message translates to:
  /// **'{code, select, INVALID_ARG{Invalid request} NOT_CREATED{Engine not started} ALREADY_CREATED{Engine already started} NOT_IMPLEMENTED{Not supported by the engine yet} AUDIO_DEVICE{Audio device error} MIC_PERMISSION{No microphone access} FILE_NOT_FOUND{File not found} FILE_FORMAT{Unsupported file format} DISK_FULL{Storage is full} FILE_WRITE{Couldn\'t write the file} OUT_OF_MEMORY{Out of memory} QUEUE_FULL{Engine is busy, try again} JOB_CANCELLED{Cancelled} JOB_NOT_FOUND{Task not found} PITCH_NOT_DETECTED{Couldn\'t detect the pitch} OVERDUB_UNSUPPORTED{This clip can\'t be overdubbed} CELL_EMPTY{The cell is still empty after recording} other{Engine error ({code})}}'**
  String errorText(String code);

  /// No description provided for @exportChamDeBatDauGhi.
  ///
  /// In en, this message translates to:
  /// **'Tap to start recording'**
  String get exportChamDeBatDauGhi;

  /// No description provided for @exportChiaSe.
  ///
  /// In en, this message translates to:
  /// **'Share'**
  String get exportChiaSe;

  /// No description provided for @exportClippedSamples.
  ///
  /// In en, this message translates to:
  /// **'{count, plural, =1{1 sample clipped: lower the master gain} other{{count} samples clipped: lower the master gain}}'**
  String exportClippedSamples(int count);

  /// No description provided for @exportDangGhi.
  ///
  /// In en, this message translates to:
  /// **'Recording  '**
  String get exportDangGhi;

  /// No description provided for @exportDinhDang.
  ///
  /// In en, this message translates to:
  /// **'Format'**
  String get exportDinhDang;

  /// No description provided for @exportDungGhiJam.
  ///
  /// In en, this message translates to:
  /// **'Stop jam recording'**
  String get exportDungGhiJam;

  /// No description provided for @exportExportLoi.
  ///
  /// In en, this message translates to:
  /// **'Export failed: {p0}'**
  String exportExportLoi(Object p0);

  /// No description provided for @exportGhiBuoiJam.
  ///
  /// In en, this message translates to:
  /// **'Record jam'**
  String get exportGhiBuoiJam;

  /// No description provided for @exportGhiToanBoDauRa.
  ///
  /// In en, this message translates to:
  /// **'Records the whole master output while you play (WAV, saved in Exports/). Toggle it quickly with the ● button in the top bar.'**
  String get exportGhiToanBoDauRa;

  /// No description provided for @exportHuy.
  ///
  /// In en, this message translates to:
  /// **'Cancel'**
  String get exportHuy;

  /// No description provided for @exportKhongGhiDuoc.
  ///
  /// In en, this message translates to:
  /// **'Couldn\'t record: {err}'**
  String exportKhongGhiDuoc(Object err);

  /// No description provided for @exportKhongMoDuocShareSheet.
  ///
  /// In en, this message translates to:
  /// **'Couldn\'t open the share sheet: {e}'**
  String exportKhongMoDuocShareSheet(Object e);

  /// No description provided for @exportMenu.
  ///
  /// In en, this message translates to:
  /// **'Export…'**
  String get exportMenu;

  /// No description provided for @exportMoiTrackMotFile.
  ///
  /// In en, this message translates to:
  /// **'One file per track'**
  String get exportMoiTrackMotFile;

  /// No description provided for @exportResult.
  ///
  /// In en, this message translates to:
  /// **'{file} · {time}'**
  String exportResult(Object file, Object time);

  /// No description provided for @exportScene.
  ///
  /// In en, this message translates to:
  /// **'Scene'**
  String get exportScene;

  /// No description provided for @exportSceneItem.
  ///
  /// In en, this message translates to:
  /// **'{number}. {name}'**
  String exportSceneItem(Object number, Object name);

  /// No description provided for @exportSceneNayChuaCoClip.
  ///
  /// In en, this message translates to:
  /// **'This scene has no clips'**
  String get exportSceneNayChuaCoClip;

  /// No description provided for @exportSceneTab.
  ///
  /// In en, this message translates to:
  /// **'Export scene'**
  String get exportSceneTab;

  /// No description provided for @exportSoBar.
  ///
  /// In en, this message translates to:
  /// **'Bars'**
  String get exportSoBar;

  /// No description provided for @exportStemCount.
  ///
  /// In en, this message translates to:
  /// **' + {count, plural, =1{1 stem} other{{count} stems}}'**
  String exportStemCount(int count);

  /// No description provided for @exportStemsLabel.
  ///
  /// In en, this message translates to:
  /// **'Stems'**
  String get exportStemsLabel;

  /// No description provided for @exportTitle.
  ///
  /// In en, this message translates to:
  /// **'Export'**
  String get exportTitle;

  /// No description provided for @fxBat.
  ///
  /// In en, this message translates to:
  /// **'On'**
  String get fxBat;

  /// No description provided for @fxBatLai.
  ///
  /// In en, this message translates to:
  /// **'Turn on'**
  String get fxBatLai;

  /// No description provided for @fxBypass.
  ///
  /// In en, this message translates to:
  /// **'Bypass'**
  String get fxBypass;

  /// No description provided for @fxDoiLoaiFx.
  ///
  /// In en, this message translates to:
  /// **'Change FX type'**
  String get fxDoiLoaiFx;

  /// No description provided for @fxKeoDocDeChinhCham.
  ///
  /// In en, this message translates to:
  /// **'Drag up/down to adjust · double-tap to reset'**
  String get fxKeoDocDeChinhCham;

  /// No description provided for @fxMasterCard.
  ///
  /// In en, this message translates to:
  /// **'Master · EQ3 · Limiter'**
  String get fxMasterCard;

  /// No description provided for @fxParam.
  ///
  /// In en, this message translates to:
  /// **'{name, select, mode{Mode} cutoff{Cutoff} reso{Reso} rate{Rate} feedback{Feedback} mix{Mix} pingpong{Ping-pong} size{Size} damping{Damping} width{Width} low{Low} mid{Mid} high{High} thresh{Thresh} ratio{Ratio} attack{Attack} release{Release} makeup{Makeup} ceiling{Ceiling} other{{name}}}'**
  String fxParam(String name);

  /// No description provided for @fxSlotTrong.
  ///
  /// In en, this message translates to:
  /// **'Slot {p0} empty'**
  String fxSlotTrong(Object p0);

  /// No description provided for @fxTat.
  ///
  /// In en, this message translates to:
  /// **'Off'**
  String get fxTat;

  /// No description provided for @fxThemFx.
  ///
  /// In en, this message translates to:
  /// **'Add FX'**
  String get fxThemFx;

  /// No description provided for @fxToggleOff.
  ///
  /// In en, this message translates to:
  /// **'OFF'**
  String get fxToggleOff;

  /// No description provided for @fxToggleOn.
  ///
  /// In en, this message translates to:
  /// **'ON'**
  String get fxToggleOn;

  /// No description provided for @fxType.
  ///
  /// In en, this message translates to:
  /// **'{type, select, filter{Filter} delay{Delay} reverb{Reverb} eq3{EQ3} comp{Compressor} other{{type}}}'**
  String fxType(String type);

  /// No description provided for @fxXoaFx.
  ///
  /// In en, this message translates to:
  /// **'Remove FX'**
  String get fxXoaFx;

  /// No description provided for @instrumentBanPhim.
  ///
  /// In en, this message translates to:
  /// **'Keyboard'**
  String get instrumentBanPhim;

  /// No description provided for @instrumentKhongTimThayNhacCu.
  ///
  /// In en, this message translates to:
  /// **'Instrument not found'**
  String get instrumentKhongTimThayNhacCu;

  /// No description provided for @instrumentMode.
  ///
  /// In en, this message translates to:
  /// **'{mode, select, natural{Natural} classic{Classic} other{{mode}}}'**
  String instrumentMode(String mode);

  /// No description provided for @instrumentPad.
  ///
  /// In en, this message translates to:
  /// **'Pads'**
  String get instrumentPad;

  /// No description provided for @instrumentThuAmMoi.
  ///
  /// In en, this message translates to:
  /// **'Record new'**
  String get instrumentThuAmMoi;

  /// No description provided for @jamDroppedMs.
  ///
  /// In en, this message translates to:
  /// **'Lost {ms} ms: the disk could not keep up'**
  String jamDroppedMs(Object ms);

  /// No description provided for @latencyAppPhatTiengClickRa.
  ///
  /// In en, this message translates to:
  /// **'The app plays clicks through the speaker and listens through the mic to measure the round trip. Keep the iPad somewhere quiet at a moderate volume — or connect a loopback cable from output to input. Takes about 5 seconds. The offset below is set from the result.'**
  String get latencyAppPhatTiengClickRa;

  /// No description provided for @latencyBanThuNgheBiTre.
  ///
  /// In en, this message translates to:
  /// **'If recordings sound late against the beat, increase it; if early, decrease it. Saved and applied every time the app opens.'**
  String get latencyBanThuNgheBiTre;

  /// No description provided for @latencyBuThem.
  ///
  /// In en, this message translates to:
  /// **'Extra offset'**
  String get latencyBuThem;

  /// No description provided for @latencyCanQuyenMicroDeDo.
  ///
  /// In en, this message translates to:
  /// **'Microphone access is needed to measure'**
  String get latencyCanQuyenMicroDeDo;

  /// No description provided for @latencyChinhTay.
  ///
  /// In en, this message translates to:
  /// **'Manual'**
  String get latencyChinhTay;

  /// No description provided for @latencyChuaCo.
  ///
  /// In en, this message translates to:
  /// **'not set'**
  String get latencyChuaCo;

  /// No description provided for @latencyDaLuuEngineChuaAp.
  ///
  /// In en, this message translates to:
  /// **'Saved, but the engine couldn\'t apply it yet: {err}'**
  String latencyDaLuuEngineChuaAp(Object err);

  /// No description provided for @latencyDeviceBaoBuSampleLech.
  ///
  /// In en, this message translates to:
  /// **'Device reports {p0} · offset {p1} samples · spread {p2} · confidence {p3}%'**
  String latencyDeviceBaoBuSampleLech(Object p0, Object p1, Object p2, Object p3);

  /// No description provided for @latencyDoDoTre.
  ///
  /// In en, this message translates to:
  /// **'Measure latency'**
  String get latencyDoDoTre;

  /// No description provided for @latencyDoLoi.
  ///
  /// In en, this message translates to:
  /// **'Measurement failed: {p0}'**
  String latencyDoLoi(Object p0);

  /// No description provided for @latencyDoTreVongThuPhat.
  ///
  /// In en, this message translates to:
  /// **'Round-trip latency (record ↔ play)'**
  String get latencyDoTreVongThuPhat;

  /// No description provided for @latencyDoTuDong.
  ///
  /// In en, this message translates to:
  /// **'Automatic measurement'**
  String get latencyDoTuDong;

  /// No description provided for @latencyEngineDangDung.
  ///
  /// In en, this message translates to:
  /// **'Engine is using'**
  String get latencyEngineDangDung;

  /// No description provided for @latencyFailReason.
  ///
  /// In en, this message translates to:
  /// **'{reason, select, NO_SIGNAL{Didn\'t hear the test clicks — turn the volume up or connect a loopback cable} TOO_NOISY{Too much background noise — try somewhere quieter} INCONSISTENT{The measurements didn\'t agree — keep the iPad still and try again} DEVICE_CHANGED{The audio device changed during the test — try again} TIMEOUT{The test took too long — try again} other{{reason}}}'**
  String latencyFailReason(String reason);

  /// No description provided for @latencyKhongBatDuocAudio.
  ///
  /// In en, this message translates to:
  /// **'Couldn\'t start audio'**
  String get latencyKhongBatDuocAudio;

  /// No description provided for @latencyLanDoGanNhat.
  ///
  /// In en, this message translates to:
  /// **'Last measurement'**
  String get latencyLanDoGanNhat;

  /// No description provided for @latencyOffsetValue.
  ///
  /// In en, this message translates to:
  /// **'{ms} ms ({samples, plural, =1{1 sample} other{{samples} samples}})'**
  String latencyOffsetValue(Object ms, int samples);

  /// No description provided for @latencySamplesMs.
  ///
  /// In en, this message translates to:
  /// **'{samples, plural, =1{1 sample} other{{samples} samples}} · {ms} ms'**
  String latencySamplesMs(int samples, Object ms);

  /// No description provided for @latencyVe0.
  ///
  /// In en, this message translates to:
  /// **'Reset to 0'**
  String get latencyVe0;

  /// No description provided for @learnKind.
  ///
  /// In en, this message translates to:
  /// **'{kind, select, clip{Clip} scene{Scene} transport{Transport} stopAll{Stop all} trackGain{Track gain} trackMute{Track mute} fx{FX parameter} loopButton{LOOP button} trackStop{Stop track} undoOverdub{Undo overdub} other{{kind}}}'**
  String learnKind(String kind);

  /// No description provided for @libraryTag.
  ///
  /// In en, this message translates to:
  /// **'{tag, select, drums{Drums} bass{Bass} keys{Keys} synth{Synth} pad{Pad} click{Click} tone{Tone} test{Test} hiphop{Hip-hop} funk{Funk} melody{Melody} vocal{Vocal} fx{FX} other{{tag}}}'**
  String libraryTag(String tag);

  /// No description provided for @licensesChuaCoFileGiayPhep.
  ///
  /// In en, this message translates to:
  /// **'No content license files yet.'**
  String get licensesChuaCoFileGiayPhep;

  /// No description provided for @licensesChuaTichHopP406.
  ///
  /// In en, this message translates to:
  /// **'Not integrated yet (P4-06)'**
  String get licensesChuaTichHopP406;

  /// No description provided for @licensesGiayPhepCacGoiFlutter.
  ///
  /// In en, this message translates to:
  /// **'Licenses of Flutter/Dart packages'**
  String get licensesGiayPhepCacGoiFlutter;

  /// No description provided for @licensesGoiFlutterDart.
  ///
  /// In en, this message translates to:
  /// **'Flutter / Dart packages'**
  String get licensesGoiFlutterDart;

  /// No description provided for @licensesJuce9DungTheoGoi.
  ///
  /// In en, this message translates to:
  /// **'JUCE 9 — used under the Starter plan (JUCE EULA)'**
  String get licensesJuce9DungTheoGoi;

  /// No description provided for @licensesKhongDocDuoc.
  ///
  /// In en, this message translates to:
  /// **'Couldn\'t read {asset}'**
  String licensesKhongDocDuoc(Object asset);

  /// No description provided for @licensesNoiDungAmThanh.
  ///
  /// In en, this message translates to:
  /// **'Sound content'**
  String get licensesNoiDungAmThanh;

  /// No description provided for @licensesThuVienBenThuBa.
  ///
  /// In en, this message translates to:
  /// **'Third-party libraries'**
  String get licensesThuVienBenThuBa;

  /// No description provided for @linkBamOMotMayThi.
  ///
  /// In en, this message translates to:
  /// **'Pressing ▶/■ on one device starts/stops the others too.'**
  String get linkBamOMotMayThi;

  /// No description provided for @linkBatLink2.
  ///
  /// In en, this message translates to:
  /// **'Enable Link'**
  String get linkBatLink2;

  /// No description provided for @linkCanQuyenMangCucBo.
  ///
  /// In en, this message translates to:
  /// **'Needs local network access (iOS asks the first time you turn it on).'**
  String get linkCanQuyenMangCucBo;

  /// No description provided for @linkDangNoiThietBi.
  ///
  /// In en, this message translates to:
  /// **'Connected: {peers, plural, =0{no peers} =1{1 peer} other{{peers} peers}}'**
  String linkDangNoiThietBi(int peers);

  /// No description provided for @linkDongBoStartStop.
  ///
  /// In en, this message translates to:
  /// **'Sync Start/Stop'**
  String get linkDongBoStartStop;

  /// No description provided for @linkDongBoTempoVaNhip.
  ///
  /// In en, this message translates to:
  /// **'Sync tempo and beat with other apps/devices on the same Wi-Fi. Saved per project.'**
  String get linkDongBoTempoVaNhip;

  /// No description provided for @linkEngineChuaCoLinkP4.
  ///
  /// In en, this message translates to:
  /// **'The engine doesn\'t have Link yet (P4-06) — your choice is saved in the project'**
  String get linkEngineChuaCoLinkP4;

  /// No description provided for @linkTitle.
  ///
  /// In en, this message translates to:
  /// **'Ableton Link'**
  String get linkTitle;

  /// No description provided for @loopButtonLabel.
  ///
  /// In en, this message translates to:
  /// **'{state, select, idle{LOOP} rec{REC} play{PLAY} dub{DUB} other{LOOP}}'**
  String loopButtonLabel(String state);

  /// No description provided for @loopButtonTooltip.
  ///
  /// In en, this message translates to:
  /// **'LOOP: tap = record → play → overdub · double-tap = stop · hold = undo'**
  String get loopButtonTooltip;

  /// No description provided for @loopDungTrack.
  ///
  /// In en, this message translates to:
  /// **'Stop track'**
  String get loopDungTrack;

  /// No description provided for @loopHoanTac.
  ///
  /// In en, this message translates to:
  /// **'Undo overdub / delete clip'**
  String get loopHoanTac;

  /// No description provided for @loopXoa.
  ///
  /// In en, this message translates to:
  /// **'Delete'**
  String get loopXoa;

  /// No description provided for @loopXoaClipBody.
  ///
  /// In en, this message translates to:
  /// **'There is no overdub layer to undo.'**
  String get loopXoaClipBody;

  /// No description provided for @loopXoaClipTitle.
  ///
  /// In en, this message translates to:
  /// **'Delete clip \"{name}\"?'**
  String loopXoaClipTitle(Object name);

  /// No description provided for @menuClipMidiTrong.
  ///
  /// In en, this message translates to:
  /// **'{bars, plural, =1{Empty MIDI clip · 1 bar} other{Empty MIDI clip · {bars} bars}}'**
  String menuClipMidiTrong(int bars);

  /// No description provided for @menuCopy.
  ///
  /// In en, this message translates to:
  /// **'Copy'**
  String get menuCopy;

  /// No description provided for @menuCopyToiDay.
  ///
  /// In en, this message translates to:
  /// **'Copy here'**
  String get menuCopyToiDay;

  /// No description provided for @menuDan.
  ///
  /// In en, this message translates to:
  /// **'Paste \"{p0}\"'**
  String menuDan(Object p0);

  /// No description provided for @menuDanChuaCopyClipNao.
  ///
  /// In en, this message translates to:
  /// **'Paste (nothing copied)'**
  String get menuDanChuaCopyClipNao;

  /// No description provided for @menuDiChuyenToiDay.
  ///
  /// In en, this message translates to:
  /// **'Move here'**
  String get menuDiChuyenToiDay;

  /// No description provided for @menuDoiTenClip.
  ///
  /// In en, this message translates to:
  /// **'Rename clip'**
  String get menuDoiTenClip;

  /// No description provided for @menuKhongConOTrongBen.
  ///
  /// In en, this message translates to:
  /// **'No empty cell below'**
  String get menuKhongConOTrongBen;

  /// No description provided for @menuMauTrack.
  ///
  /// In en, this message translates to:
  /// **'Track color'**
  String get menuMauTrack;

  /// No description provided for @menuODichDaCoClip.
  ///
  /// In en, this message translates to:
  /// **'The target cell already has a clip'**
  String get menuODichDaCoClip;

  /// No description provided for @metronomeMode.
  ///
  /// In en, this message translates to:
  /// **'{mode, select, off{Off} always{On} recordOnly{Rec only} other{{mode}}}'**
  String metronomeMode(String mode);

  /// No description provided for @midiAnyChannel.
  ///
  /// In en, this message translates to:
  /// **'any channel'**
  String get midiAnyChannel;

  /// No description provided for @midiAnyDevice.
  ///
  /// In en, this message translates to:
  /// **'any device'**
  String get midiAnyDevice;

  /// No description provided for @midiBatDauLearn.
  ///
  /// In en, this message translates to:
  /// **'Start learn'**
  String get midiBatDauLearn;

  /// No description provided for @midiBoGan.
  ///
  /// In en, this message translates to:
  /// **'Remove mapping'**
  String get midiBoGan;

  /// No description provided for @midiChannel.
  ///
  /// In en, this message translates to:
  /// **'channel {number}'**
  String midiChannel(Object number);

  /// No description provided for @midiChuaGanGi.
  ///
  /// In en, this message translates to:
  /// **'Nothing mapped yet.'**
  String get midiChuaGanGi;

  /// No description provided for @midiChuaThayThietBiMidi.
  ///
  /// In en, this message translates to:
  /// **'No MIDI devices found. Connect via USB or pair Bluetooth MIDI.'**
  String get midiChuaThayThietBiMidi;

  /// No description provided for @midiClipO.
  ///
  /// In en, this message translates to:
  /// **'Clip · {p0} · cell {p1}'**
  String midiClipO(Object p0, Object p1);

  /// No description provided for @midiDenLedPhanHoiTrang.
  ///
  /// In en, this message translates to:
  /// **'Clip status LEDs (Launchpad) arrive in P4-05.'**
  String get midiDenLedPhanHoiTrang;

  /// No description provided for @midiDungMoiClipTheoQuantize.
  ///
  /// In en, this message translates to:
  /// **'Stops every clip on the quantize grid.'**
  String get midiDungMoiClipTheoQuantize;

  /// No description provided for @midiDungTatCa.
  ///
  /// In en, this message translates to:
  /// **'Stop all'**
  String get midiDungTatCa;

  /// No description provided for @midiEnableFailed.
  ///
  /// In en, this message translates to:
  /// **'Couldn\'t change the device: {error}'**
  String midiEnableFailed(Object error);

  /// No description provided for @midiEngineChuaHoTroMidi.
  ///
  /// In en, this message translates to:
  /// **'The engine doesn\'t support MIDI yet (P4-01)'**
  String get midiEngineChuaHoTroMidi;

  /// No description provided for @midiFootswitchGhiChu.
  ///
  /// In en, this message translates to:
  /// **'{kind, select, loopButton{The footswitch acts like the on-screen LOOP button on the selected track: record → play → overdub.} trackStop{The footswitch stops the selected track (at the quantize boundary).} undoOverdub{The footswitch undoes the last overdub of the playing clip on the selected track.} other{{kind}}}'**
  String midiFootswitchGhiChu(String kind);

  /// No description provided for @midiFxSlotItem.
  ///
  /// In en, this message translates to:
  /// **'Slot {number} · {type}'**
  String midiFxSlotItem(Object number, Object type);

  /// No description provided for @midiGanDieuKhienMidi2.
  ///
  /// In en, this message translates to:
  /// **'Map MIDI control'**
  String get midiGanDieuKhienMidi2;

  /// No description provided for @midiGanMoi.
  ///
  /// In en, this message translates to:
  /// **'New mapping'**
  String get midiGanMoi;

  /// No description provided for @midiGanMotNutNumTren.
  ///
  /// In en, this message translates to:
  /// **'Map a button/knob on your controller to a clip or FX parameter. Saved in the project.'**
  String get midiGanMotNutNumTren;

  /// No description provided for @midiGhepThietBiBluetoothMidi.
  ///
  /// In en, this message translates to:
  /// **'Pair Bluetooth MIDI device'**
  String get midiGhepThietBiBluetoothMidi;

  /// No description provided for @midiLamMoi.
  ///
  /// In en, this message translates to:
  /// **'Refresh'**
  String get midiLamMoi;

  /// No description provided for @midiLearnFailed.
  ///
  /// In en, this message translates to:
  /// **'Couldn\'t start learn: {error}'**
  String midiLearnFailed(Object error);

  /// No description provided for @midiMidiLearnProjectDangMo.
  ///
  /// In en, this message translates to:
  /// **'MIDI learn (current project)'**
  String get midiMidiLearnProjectDangMo;

  /// No description provided for @midiO.
  ///
  /// In en, this message translates to:
  /// **'Cell {p0}'**
  String midiO(Object p0);

  /// No description provided for @midiParamFallback.
  ///
  /// In en, this message translates to:
  /// **'parameter {id}'**
  String midiParamFallback(Object id);

  /// No description provided for @midiSlot.
  ///
  /// In en, this message translates to:
  /// **'slot {number}'**
  String midiSlot(Object number);

  /// No description provided for @midiSourceCc.
  ///
  /// In en, this message translates to:
  /// **'CC {number}'**
  String midiSourceCc(Object number);

  /// No description provided for @midiSourceNote.
  ///
  /// In en, this message translates to:
  /// **'Note {number}'**
  String midiSourceNote(Object number);

  /// No description provided for @midiThietBiRa.
  ///
  /// In en, this message translates to:
  /// **'Outputs'**
  String get midiThietBiRa;

  /// No description provided for @midiThietBiVao.
  ///
  /// In en, this message translates to:
  /// **'Inputs'**
  String get midiThietBiVao;

  /// No description provided for @midiTrackNayChuaCoFx.
  ///
  /// In en, this message translates to:
  /// **'This track has no FX.'**
  String get midiTrackNayChuaCoFx;

  /// No description provided for @midiVanHoacBamMotNut.
  ///
  /// In en, this message translates to:
  /// **'Turn a knob or press a button on your controller…'**
  String get midiVanHoacBamMotNut;

  /// No description provided for @mixerMaster.
  ///
  /// In en, this message translates to:
  /// **'Master'**
  String get mixerMaster;

  /// No description provided for @mixerMonitor.
  ///
  /// In en, this message translates to:
  /// **'Monitor'**
  String get mixerMonitor;

  /// No description provided for @mixerMonitorLuonBat.
  ///
  /// In en, this message translates to:
  /// **'Monitor: On'**
  String get mixerMonitorLuonBat;

  /// No description provided for @mixerMonitorTat.
  ///
  /// In en, this message translates to:
  /// **'Monitor: Off'**
  String get mixerMonitorTat;

  /// No description provided for @mixerMonitorTuDongKhiArm.
  ///
  /// In en, this message translates to:
  /// **'Monitor: Auto (when armed)'**
  String get mixerMonitorTuDongKhiArm;

  /// No description provided for @monitorShort.
  ///
  /// In en, this message translates to:
  /// **'{mode, select, off{Off} auto{Auto} always{On} other{{mode}}}'**
  String monitorShort(String mode);

  /// No description provided for @onboardingAmThanhChiLuuTren.
  ///
  /// In en, this message translates to:
  /// **'Audio stays on your iPad and is never sent anywhere.'**
  String get onboardingAmThanhChiLuuTren;

  /// No description provided for @onboardingBienMotTiengBanThu.
  ///
  /// In en, this message translates to:
  /// **'Turn a sound you record into an instrument you can play on the keyboard.'**
  String get onboardingBienMotTiengBanThu;

  /// No description provided for @onboardingBoQua.
  ///
  /// In en, this message translates to:
  /// **'Skip'**
  String get onboardingBoQua;

  /// No description provided for @onboardingChamMotODePhat.
  ///
  /// In en, this message translates to:
  /// **'Tap a cell to launch a clip — it starts on the next bar.'**
  String get onboardingChamMotODePhat;

  /// No description provided for @onboardingChamSoSceneBenPhai.
  ///
  /// In en, this message translates to:
  /// **'Tap a scene on the right to launch a whole row at once.'**
  String get onboardingChamSoSceneBenPhai;

  /// No description provided for @onboardingChoPhepMicro.
  ///
  /// In en, this message translates to:
  /// **'Allow microphone'**
  String get onboardingChoPhepMicro;

  /// No description provided for @onboardingChuaCoQuyenMicroThu.
  ///
  /// In en, this message translates to:
  /// **'No microphone access yet: we\'ll ask again when you record.'**
  String get onboardingChuaCoQuyenMicroThu;

  /// No description provided for @onboardingDeSau.
  ///
  /// In en, this message translates to:
  /// **'Later'**
  String get onboardingDeSau;

  /// No description provided for @onboardingDoDoTreDeBan.
  ///
  /// In en, this message translates to:
  /// **'Measure latency so your recordings land on the beat.'**
  String get onboardingDoDoTreDeBan;

  /// No description provided for @onboardingKhongTaoDuocProjectDemo.
  ///
  /// In en, this message translates to:
  /// **'Couldn\'t create the demo project: {e}'**
  String onboardingKhongTaoDuocProjectDemo(Object e);

  /// No description provided for @onboardingMicroDangBiTatBan.
  ///
  /// In en, this message translates to:
  /// **'The microphone is off. You can still play loops and built-in instruments; turn it back on in Settings to record.'**
  String get onboardingMicroDangBiTatBan;

  /// No description provided for @onboardingMoCaiDat.
  ///
  /// In en, this message translates to:
  /// **'Open Settings'**
  String get onboardingMoCaiDat;

  /// No description provided for @onboardingMoProjectDemo.
  ///
  /// In en, this message translates to:
  /// **'Open demo project'**
  String get onboardingMoProjectDemo;

  /// No description provided for @onboardingMusicLooperCanMicro.
  ///
  /// In en, this message translates to:
  /// **'Music Looper needs the microphone'**
  String get onboardingMusicLooperCanMicro;

  /// No description provided for @onboardingTabInstrumentPhiaDuoiDe.
  ///
  /// In en, this message translates to:
  /// **'Use the Instrument tab below to play pads and keys.'**
  String get onboardingTabInstrumentPhiaDuoiDe;

  /// No description provided for @onboardingThuGiongHatGuitarThanh.
  ///
  /// In en, this message translates to:
  /// **'Record vocals, guitar… into loops that stay in time.'**
  String get onboardingThuGiongHatGuitarThanh;

  /// No description provided for @onboardingThuNgayVoiProjectDemo.
  ///
  /// In en, this message translates to:
  /// **'Try the demo project'**
  String get onboardingThuNgayVoiProjectDemo;

  /// No description provided for @onboardingTiepTuc.
  ///
  /// In en, this message translates to:
  /// **'Continue'**
  String get onboardingTiepTuc;

  /// No description provided for @onboardingTuTaoProject.
  ///
  /// In en, this message translates to:
  /// **'Start my own project'**
  String get onboardingTuTaoProject;

  /// No description provided for @panelTab.
  ///
  /// In en, this message translates to:
  /// **'{tab, select, clip{Clip} instrument{Instrument} mixer{Mixer} fx{FX} browser{Browser} other{{tab}}}'**
  String panelTab(String tab);

  /// No description provided for @projectsBanSao.
  ///
  /// In en, this message translates to:
  /// **'{name} (copy)'**
  String projectsBanSao(Object name);

  /// No description provided for @projectsCaiDat.
  ///
  /// In en, this message translates to:
  /// **'Settings'**
  String get projectsCaiDat;

  /// No description provided for @projectsChuaCoProjectBamProject.
  ///
  /// In en, this message translates to:
  /// **'No projects yet. Tap \"New project\" or \"Create demo project\".'**
  String get projectsChuaCoProjectBamProject;

  /// No description provided for @projectsDoiTen.
  ///
  /// In en, this message translates to:
  /// **'Rename'**
  String get projectsDoiTen;

  /// No description provided for @projectsKhongDocDuocProject.
  ///
  /// In en, this message translates to:
  /// **'Couldn\'t read projects'**
  String get projectsKhongDocDuocProject;

  /// No description provided for @projectsLoi.
  ///
  /// In en, this message translates to:
  /// **'Error: {e}'**
  String projectsLoi(Object e);

  /// No description provided for @projectsNhanBan.
  ///
  /// In en, this message translates to:
  /// **'Duplicate'**
  String get projectsNhanBan;

  /// No description provided for @projectsProjectJsonBiHongDa.
  ///
  /// In en, this message translates to:
  /// **'project.json was damaged — opened the previous save (.bak)'**
  String get projectsProjectJsonBiHongDa;

  /// No description provided for @projectsProjectMoi.
  ///
  /// In en, this message translates to:
  /// **'New project'**
  String get projectsProjectMoi;

  /// No description provided for @projectsSuaLanCuoi.
  ///
  /// In en, this message translates to:
  /// **'Last edited: {p0}'**
  String projectsSuaLanCuoi(Object p0);

  /// No description provided for @projectsTaoProjectDemo.
  ///
  /// In en, this message translates to:
  /// **'Create demo project'**
  String get projectsTaoProjectDemo;

  /// No description provided for @projectsTitle.
  ///
  /// In en, this message translates to:
  /// **'Projects'**
  String get projectsTitle;

  /// No description provided for @projectsXoa.
  ///
  /// In en, this message translates to:
  /// **'Delete \"{p0}\"?'**
  String projectsXoa(Object p0);

  /// No description provided for @projectsXoa2.
  ///
  /// In en, this message translates to:
  /// **'Delete'**
  String get projectsXoa2;

  /// No description provided for @projectsXoaCaBanThuAm.
  ///
  /// In en, this message translates to:
  /// **'This also deletes the recordings in the project. This can\'t be undone.'**
  String get projectsXoaCaBanThuAm;

  /// No description provided for @quantizeGrid.
  ///
  /// In en, this message translates to:
  /// **'{grid, select, none{None} sixteenth{1/16} eighth{1/8} quarter{1/4} half{1/2} bar1{1 bar} bar2{2 bars} bar4{4 bars} other{{grid}}}'**
  String quantizeGrid(String grid);

  /// No description provided for @recordQuantize.
  ///
  /// In en, this message translates to:
  /// **'{grid, select, off{Off} sixteenth{1/16} eighth{1/8} other{{grid}}}'**
  String recordQuantize(String grid);

  /// No description provided for @samplerCanQuyenMicroDeThu.
  ///
  /// In en, this message translates to:
  /// **'Microphone access is needed to record (Settings → Music Looper → Microphone)'**
  String get samplerCanQuyenMicroDeThu;

  /// No description provided for @samplerChamDeThuMotNot.
  ///
  /// In en, this message translates to:
  /// **'Tap to record one note (up to {p0} s)'**
  String samplerChamDeThuMotNot(Object p0);

  /// No description provided for @samplerDangCatKhoangLangVa.
  ///
  /// In en, this message translates to:
  /// **'Trimming silence and detecting the note…'**
  String get samplerDangCatKhoangLangVa;

  /// No description provided for @samplerDangTaoNhacCu13.
  ///
  /// In en, this message translates to:
  /// **'Creating instrument (13 zones)… {p0}%'**
  String samplerDangTaoNhacCu13(Object p0);

  /// No description provided for @samplerDoTinCay.
  ///
  /// In en, this message translates to:
  /// **'confidence {p0}%'**
  String samplerDoTinCay(Object p0);

  /// No description provided for @samplerHatHoacThoiMotNot.
  ///
  /// In en, this message translates to:
  /// **'Sing or play one steady note… stops after {p0} s'**
  String samplerHatHoacThoiMotNot(Object p0);

  /// No description provided for @samplerKhongBatDuocAudio.
  ///
  /// In en, this message translates to:
  /// **'Couldn\'t start audio: {p0}'**
  String samplerKhongBatDuocAudio(Object p0);

  /// No description provided for @samplerKhongChacVeCaoDo.
  ///
  /// In en, this message translates to:
  /// **'Pitch is uncertain — set the root note with − / + before creating'**
  String get samplerKhongChacVeCaoDo;

  /// No description provided for @samplerKhongDoDuocCaoDo.
  ///
  /// In en, this message translates to:
  /// **'Couldn\'t detect the pitch — pick the root note and create again'**
  String get samplerKhongDoDuocCaoDo;

  /// No description provided for @samplerKhongNgheThayTieng.
  ///
  /// In en, this message translates to:
  /// **'Didn\'t hear any sound — check the mic and record again'**
  String get samplerKhongNgheThayTieng;

  /// No description provided for @samplerNotGoc.
  ///
  /// In en, this message translates to:
  /// **'Root note: '**
  String get samplerNotGoc;

  /// No description provided for @samplerPhanTichLoi.
  ///
  /// In en, this message translates to:
  /// **'Analysis failed: {p0}'**
  String samplerPhanTichLoi(Object p0);

  /// No description provided for @samplerTaoNhacCu.
  ///
  /// In en, this message translates to:
  /// **'Create instrument'**
  String get samplerTaoNhacCu;

  /// No description provided for @samplerTaoNhacCuLoi.
  ///
  /// In en, this message translates to:
  /// **'Couldn\'t create the instrument: {p0}'**
  String samplerTaoNhacCuLoi(Object p0);

  /// No description provided for @samplerThuAmNhacCu.
  ///
  /// In en, this message translates to:
  /// **'Record → instrument'**
  String get samplerThuAmNhacCu;

  /// No description provided for @samplerThuLai.
  ///
  /// In en, this message translates to:
  /// **'Retake'**
  String get samplerThuLai;

  /// No description provided for @samplerTiengThu.
  ///
  /// In en, this message translates to:
  /// **'Recording {p0}'**
  String samplerTiengThu(Object p0);

  /// No description provided for @samplerTrim.
  ///
  /// In en, this message translates to:
  /// **'Trim {start} → {end} s'**
  String samplerTrim(Object start, Object end);

  /// No description provided for @sessionBanThu.
  ///
  /// In en, this message translates to:
  /// **'Take {p0}'**
  String sessionBanThu(Object p0);

  /// No description provided for @sessionBatEditRoiChamMot.
  ///
  /// In en, this message translates to:
  /// **'Turn on ✎ Edit, then tap a cell to select a clip'**
  String get sessionBatEditRoiChamMot;

  /// No description provided for @sessionChoPhepMicro.
  ///
  /// In en, this message translates to:
  /// **'Allow microphone'**
  String get sessionChoPhepMicro;

  /// No description provided for @sessionChuaCoQuyenMicroChi.
  ///
  /// In en, this message translates to:
  /// **'No microphone access: playback only, recording is off'**
  String get sessionChuaCoQuyenMicroChi;

  /// No description provided for @sessionChuaMoProject.
  ///
  /// In en, this message translates to:
  /// **'No project open'**
  String get sessionChuaMoProject;

  /// No description provided for @sessionDangMoProject.
  ///
  /// In en, this message translates to:
  /// **'Opening project… {p0}/{p1}'**
  String sessionDangMoProject(Object p0, Object p1);

  /// No description provided for @sessionKhongLuuDuocProject.
  ///
  /// In en, this message translates to:
  /// **'Couldn\'t save the project: {reason}'**
  String sessionKhongLuuDuocProject(Object reason);

  /// No description provided for @sessionLoiKhiMoProject.
  ///
  /// In en, this message translates to:
  /// **'{p0, plural, =1{1 error} other{{p0} errors}} while opening the project: {p1}'**
  String sessionLoiKhiMoProject(int p0, Object p1);

  /// No description provided for @sessionMidiClipName.
  ///
  /// In en, this message translates to:
  /// **'MIDI {number}'**
  String sessionMidiClipName(Object number);

  /// No description provided for @sessionMoPanel.
  ///
  /// In en, this message translates to:
  /// **'Open panel'**
  String get sessionMoPanel;

  /// No description provided for @sessionMoRongPanel.
  ///
  /// In en, this message translates to:
  /// **'Expand panel'**
  String get sessionMoRongPanel;

  /// No description provided for @sessionMode.
  ///
  /// In en, this message translates to:
  /// **'{mode, select, perform{Perform} edit{Edit} other{{mode}}}'**
  String sessionMode(String mode);

  /// No description provided for @sessionOTrong.
  ///
  /// In en, this message translates to:
  /// **'Cell {p0}·{p1} is empty'**
  String sessionOTrong(Object p0, Object p1);

  /// No description provided for @sessionSceneName.
  ///
  /// In en, this message translates to:
  /// **'Scene {number}'**
  String sessionSceneName(Object number);

  /// No description provided for @sessionThuGon.
  ///
  /// In en, this message translates to:
  /// **'Collapse'**
  String get sessionThuGon;

  /// No description provided for @sessionThuNhoPanel.
  ///
  /// In en, this message translates to:
  /// **'Restore panel'**
  String get sessionThuNhoPanel;

  /// No description provided for @sessionTitle.
  ///
  /// In en, this message translates to:
  /// **'Session'**
  String get sessionTitle;

  /// No description provided for @sessionTrackName.
  ///
  /// In en, this message translates to:
  /// **'Track {number}'**
  String sessionTrackName(Object number);

  /// No description provided for @settingsNeedsProjectLink.
  ///
  /// In en, this message translates to:
  /// **'Open a project to turn on Link.'**
  String get settingsNeedsProjectLink;

  /// No description provided for @settingsNeedsProjectMidi.
  ///
  /// In en, this message translates to:
  /// **'Open a project to map MIDI controls.'**
  String get settingsNeedsProjectMidi;

  /// No description provided for @settingsSection.
  ///
  /// In en, this message translates to:
  /// **'{section, select, audio{Audio} latency{Latency} midi{MIDI} link{Link} licenses{Licenses} about{About} other{{section}}}'**
  String settingsSection(String section);

  /// No description provided for @spike1MsDoLai.
  ///
  /// In en, this message translates to:
  /// **'✗ (> 1 ms, measure again)'**
  String get spike1MsDoLai;

  /// No description provided for @spikeApiversion.
  ///
  /// In en, this message translates to:
  /// **'apiVersion = {p0}'**
  String spikeApiversion(Object p0);

  /// No description provided for @spikeAppCanMicroDeThu.
  ///
  /// In en, this message translates to:
  /// **'The app needs the microphone to record and measure latency.\nOpen Settings → Music Looper → turn on Microphone, then come back and tap Start.'**
  String get spikeAppCanMicroDeThu;

  /// No description provided for @spikeAudioBiNgatSiriCuoc.
  ///
  /// In en, this message translates to:
  /// **'Audio interrupted (Siri/call…)'**
  String get spikeAudioBiNgatSiriCuoc;

  /// No description provided for @spikeBat.
  ///
  /// In en, this message translates to:
  /// **'on'**
  String get spikeBat;

  /// No description provided for @spikeBlock20050Ms.
  ///
  /// In en, this message translates to:
  /// **'Block 200 / 50 ms'**
  String get spikeBlock20050Ms;

  /// No description provided for @spikeCamTaiNgheTruoc.
  ///
  /// In en, this message translates to:
  /// **'Plug in headphones first'**
  String get spikeCamTaiNgheTruoc;

  /// No description provided for @spikeCanQuyenMicro.
  ///
  /// In en, this message translates to:
  /// **'Microphone access needed'**
  String get spikeCanQuyenMicro;

  /// No description provided for @spikeCanhBaoBoNhoMb.
  ///
  /// In en, this message translates to:
  /// **'Memory warning: {p0} MB'**
  String spikeCanhBaoBoNhoMb(Object p0);

  /// No description provided for @spikeCheaper.
  ///
  /// In en, this message translates to:
  /// **'Cheaper'**
  String get spikeCheaper;

  /// No description provided for @spikeCo.
  ///
  /// In en, this message translates to:
  /// **'yes'**
  String get spikeCo;

  /// No description provided for @spikeCoLatencyCao.
  ///
  /// In en, this message translates to:
  /// **'yes ⚠ high latency'**
  String get spikeCoLatencyCao;

  /// No description provided for @spikeCpuDinh.
  ///
  /// In en, this message translates to:
  /// **'CPU  {cpuAvg}% · peak {cpuPeak}%'**
  String spikeCpuDinh(Object cpuAvg, Object cpuPeak);

  /// No description provided for @spikeDaCamBat.
  ///
  /// In en, this message translates to:
  /// **'Plugged in, turn on'**
  String get spikeDaCamBat;

  /// No description provided for @spikeDangChay.
  ///
  /// In en, this message translates to:
  /// **'Running…'**
  String get spikeDangChay;

  /// No description provided for @spikeDangDo.
  ///
  /// In en, this message translates to:
  /// **'Measuring…'**
  String get spikeDangDo;

  /// No description provided for @spikeDangThu4Giay.
  ///
  /// In en, this message translates to:
  /// **'Recording 4 seconds…'**
  String get spikeDangThu4Giay;

  /// No description provided for @spikeDeSau.
  ///
  /// In en, this message translates to:
  /// **'Later'**
  String get spikeDeSau;

  /// No description provided for @spikeDeviceInKenh.
  ///
  /// In en, this message translates to:
  /// **'device: {p0} · in {p1} ch'**
  String spikeDeviceInKenh(Object p0, Object p1);

  /// No description provided for @spikeDo.
  ///
  /// In en, this message translates to:
  /// **'Measure'**
  String get spikeDo;

  /// No description provided for @spikeDoLatency.
  ///
  /// In en, this message translates to:
  /// **'Measure latency'**
  String get spikeDoLatency;

  /// No description provided for @spikeDoLatencyBoTaiNghe.
  ///
  /// In en, this message translates to:
  /// **'Measure latency: no headphones, external speaker, quiet room (~3 s, sine/loop paused). Stretch bench uses the 4-second recording.'**
  String get spikeDoLatencyBoTaiNghe;

  /// No description provided for @spikeEventRouteNgatXrun.
  ///
  /// In en, this message translates to:
  /// **'Events (route, interruptions, xruns)'**
  String get spikeEventRouteNgatXrun;

  /// No description provided for @spikeFakeengineChuaCoLoopcoreXcframework.
  ///
  /// In en, this message translates to:
  /// **'FakeEngine (no LoopCore.xcframework yet)'**
  String get spikeFakeengineChuaCoLoopcoreXcframework;

  /// No description provided for @spikeFormant.
  ///
  /// In en, this message translates to:
  /// **'Formant'**
  String get spikeFormant;

  /// No description provided for @spikeGain.
  ///
  /// In en, this message translates to:
  /// **'Gain'**
  String get spikeGain;

  /// No description provided for @spikeHetNgatAudio.
  ///
  /// In en, this message translates to:
  /// **'Interruption ended'**
  String get spikeHetNgatAudio;

  /// No description provided for @spikeHuy.
  ///
  /// In en, this message translates to:
  /// **'Cancel'**
  String get spikeHuy;

  /// No description provided for @spikeJobDangChay.
  ///
  /// In en, this message translates to:
  /// **'{op}: job {jobId} running…'**
  String spikeJobDangChay(Object op, Object jobId);

  /// No description provided for @spikeJobDangChay2.
  ///
  /// In en, this message translates to:
  /// **'{op}: job {jobId} running… {p2}%'**
  String spikeJobDangChay2(Object op, Object jobId, Object p2);

  /// No description provided for @spikeJobThatBai.
  ///
  /// In en, this message translates to:
  /// **'{op} (job {jobId}) failed: {p2} {p3}'**
  String spikeJobThatBai(Object op, Object jobId, Object p2, Object p3);

  /// No description provided for @spikeKetQua.
  ///
  /// In en, this message translates to:
  /// **'Results'**
  String get spikeKetQua;

  /// No description provided for @spikeKhong.
  ///
  /// In en, this message translates to:
  /// **'no'**
  String get spikeKhong;

  /// No description provided for @spikeKhongDoDuocMicKhong.
  ///
  /// In en, this message translates to:
  /// **'Couldn\'t measure: the mic didn\'t hear the chirp clearly (inputPeak = {p0}).\nTurn the speaker up, unplug headphones, measure in a quiet room and try again.'**
  String spikeKhongDoDuocMicKhong(Object p0);

  /// No description provided for @spikeLeAudioStartLoi.
  ///
  /// In en, this message translates to:
  /// **'le_audio_start error: {p0}'**
  String spikeLeAudioStartLoi(Object p0);

  /// No description provided for @spikeLeCreateLoi.
  ///
  /// In en, this message translates to:
  /// **'le_create error: {p0}'**
  String spikeLeCreateLoi(Object p0);

  /// No description provided for @spikeLoi.
  ///
  /// In en, this message translates to:
  /// **'error'**
  String get spikeLoi;

  /// No description provided for @spikeLoiEngine.
  ///
  /// In en, this message translates to:
  /// **'Engine error: {p0}'**
  String spikeLoiEngine(Object p0);

  /// No description provided for @spikeLoopcoreSpike.
  ///
  /// In en, this message translates to:
  /// **'LoopCore spike'**
  String get spikeLoopcoreSpike;

  /// No description provided for @spikeMoCaiDat.
  ///
  /// In en, this message translates to:
  /// **'Open Settings'**
  String get spikeMoCaiDat;

  /// No description provided for @spikeModeDefault.
  ///
  /// In en, this message translates to:
  /// **'Mode default'**
  String get spikeModeDefault;

  /// No description provided for @spikeModeMeasurement.
  ///
  /// In en, this message translates to:
  /// **'Mode measurement'**
  String get spikeModeMeasurement;

  /// No description provided for @spikeNaudioChuaChayBamStart.
  ///
  /// In en, this message translates to:
  /// **'\nAudio is not running → tap Start audio first.'**
  String get spikeNaudioChuaChayBamStart;

  /// No description provided for @spikePassthroughCanTaiNghe.
  ///
  /// In en, this message translates to:
  /// **'Passthrough (headphones needed)'**
  String get spikePassthroughCanTaiNghe;

  /// No description provided for @spikePassthroughDuaMicRaLoa.
  ///
  /// In en, this message translates to:
  /// **'Passthrough sends the mic to the speaker. Without headphones you will get feedback.'**
  String get spikePassthroughDuaMicRaLoa;

  /// No description provided for @spikePhatLoop.
  ///
  /// In en, this message translates to:
  /// **'Play loop'**
  String get spikePhatLoop;

  /// No description provided for @spikeProjectsDev.
  ///
  /// In en, this message translates to:
  /// **'Projects (dev)'**
  String get spikeProjectsDev;

  /// No description provided for @spikeRoundTripDoDuocMs.
  ///
  /// In en, this message translates to:
  /// **'Measured round-trip: {p0} ms ({p1} smp)\niOS reports: {p2} ms ({p3} smp)\nSpread between runs: {p4} ms {p5} · valid {p6}/{runs}'**
  String spikeRoundTripDoDuocMs(
    Object p0,
    Object p1,
    Object p2,
    Object p3,
    Object p4,
    Object p5,
    Object p6,
    Object runs,
  );

  /// No description provided for @spikeRouteDoiTaiNgheDay.
  ///
  /// In en, this message translates to:
  /// **'Route changed: wired/interface={p0}, Bluetooth={p1}'**
  String spikeRouteDoiTaiNgheDay(Object p0, Object p1);

  /// No description provided for @spikeSessionInfo.
  ///
  /// In en, this message translates to:
  /// **'Session info'**
  String get spikeSessionInfo;

  /// No description provided for @spikeSineHzGain.
  ///
  /// In en, this message translates to:
  /// **'Sine {p0} Hz · gain {p1}'**
  String spikeSineHzGain(Object p0, Object p1);

  /// No description provided for @spikeSpikeSetbuffersizeLoi.
  ///
  /// In en, this message translates to:
  /// **'spike.setBufferSize error: {p0}'**
  String spikeSpikeSetbuffersizeLoi(Object p0);

  /// No description provided for @spikeSpikeSetsessionmodeLoi.
  ///
  /// In en, this message translates to:
  /// **'spike.setSessionMode error: {p0}'**
  String spikeSpikeSetsessionmodeLoi(Object p0);

  /// No description provided for @spikeStartAudio.
  ///
  /// In en, this message translates to:
  /// **'Start audio'**
  String get spikeStartAudio;

  /// No description provided for @spikeStopAudio.
  ///
  /// In en, this message translates to:
  /// **'Stop audio'**
  String get spikeStopAudio;

  /// No description provided for @spikeStretchBench.
  ///
  /// In en, this message translates to:
  /// **'Stretch bench'**
  String get spikeStretchBench;

  /// No description provided for @spikeTaiGiaLapVoice.
  ///
  /// In en, this message translates to:
  /// **'Simulated load: {voices} voices'**
  String spikeTaiGiaLapVoice(Object voices);

  /// No description provided for @spikeTanSo.
  ///
  /// In en, this message translates to:
  /// **'Frequency'**
  String get spikeTanSo;

  /// No description provided for @spikeTat.
  ///
  /// In en, this message translates to:
  /// **'off'**
  String get spikeTat;

  /// No description provided for @spikeThu4Giay.
  ///
  /// In en, this message translates to:
  /// **'Record 4 seconds'**
  String get spikeThu4Giay;

  /// No description provided for @spikeThuXong.
  ///
  /// In en, this message translates to:
  /// **'Recorded: {frames} frames ({seconds} s)'**
  String spikeThuXong(Object frames, Object seconds);

  /// No description provided for @spikeTongZoneMsFormantNsetup.
  ///
  /// In en, this message translates to:
  /// **'Total {p0} zones: {p1} ms {p2} · formant {p3}\nSetup {p4} ms · file write {p5} ms\nms/zone  {zones}\n{p7} WAV files: Files app → On My iPad → Music Looper → spike'**
  String spikeTongZoneMsFormantNsetup(
    Object p0,
    Object p1,
    Object p2,
    Object p3,
    Object p4,
    Object p5,
    Object zones,
    Object p7,
  );

  /// No description provided for @spikeXrunTong.
  ///
  /// In en, this message translates to:
  /// **'Xrun (total {totalXruns})'**
  String spikeXrunTong(Object totalXruns);

  /// No description provided for @tempoMode.
  ///
  /// In en, this message translates to:
  /// **'{mode, select, fixed{Fixed BPM} firstLoop{First loop sets BPM} other{{mode}}}'**
  String tempoMode(String mode);

  /// No description provided for @trackMenuDoiTenScene.
  ///
  /// In en, this message translates to:
  /// **'Rename scene'**
  String get trackMenuDoiTenScene;

  /// No description provided for @trackMenuDoiTenTrack.
  ///
  /// In en, this message translates to:
  /// **'Rename track'**
  String get trackMenuDoiTenTrack;

  /// No description provided for @trackMenuMustBeEmpty.
  ///
  /// In en, this message translates to:
  /// **' (track must be empty)'**
  String get trackMenuMustBeEmpty;

  /// No description provided for @trackMenuToAudio.
  ///
  /// In en, this message translates to:
  /// **'Convert to audio track'**
  String get trackMenuToAudio;

  /// No description provided for @trackMenuToInstrument.
  ///
  /// In en, this message translates to:
  /// **'Convert to instrument track'**
  String get trackMenuToInstrument;

  /// No description provided for @transportAction.
  ///
  /// In en, this message translates to:
  /// **'{action, select, play{Play} stop{Stop} toggle{Play/Stop} other{{action}}}'**
  String transportAction(String action);

  /// No description provided for @transportBpm.
  ///
  /// In en, this message translates to:
  /// **'{bpm} BPM'**
  String transportBpm(Object bpm);

  /// No description provided for @transportBpmTrong.
  ///
  /// In en, this message translates to:
  /// **'— BPM'**
  String get transportBpmTrong;

  /// No description provided for @transportChoVongDau.
  ///
  /// In en, this message translates to:
  /// **'waiting for first loop'**
  String get transportChoVongDau;

  /// No description provided for @transportCpu.
  ///
  /// In en, this message translates to:
  /// **'CPU {cpu}% · xrun {xruns}'**
  String transportCpu(Object cpu, Object xruns);

  /// No description provided for @transportDaGhi.
  ///
  /// In en, this message translates to:
  /// **'Saved {p0} ({p1})'**
  String transportDaGhi(Object p0, Object p1);

  /// No description provided for @transportDemVaoBar.
  ///
  /// In en, this message translates to:
  /// **'Count-in {v, plural, =1{1 bar} other{{v} bars}}'**
  String transportDemVaoBar(int v);

  /// No description provided for @transportGhiBuoiJamMaster.
  ///
  /// In en, this message translates to:
  /// **'Record jam (master)'**
  String get transportGhiBuoiJamMaster;

  /// No description provided for @transportKhongDemVao.
  ///
  /// In en, this message translates to:
  /// **'No count-in'**
  String get transportKhongDemVao;

  /// No description provided for @transportKhongGhiDuocJam.
  ///
  /// In en, this message translates to:
  /// **'Couldn\'t record the jam: {err}'**
  String transportKhongGhiDuocJam(Object err);

  /// No description provided for @transportMetronomeCountInNhip.
  ///
  /// In en, this message translates to:
  /// **'Metronome · count-in · time signature'**
  String get transportMetronomeCountInNhip;

  /// No description provided for @transportMetronomeItem.
  ///
  /// In en, this message translates to:
  /// **'Metronome: {mode}'**
  String transportMetronomeItem(Object mode);

  /// No description provided for @transportNhip.
  ///
  /// In en, this message translates to:
  /// **'Time signature {n}/{d}'**
  String transportNhip(Object n, Object d);

  /// No description provided for @transportQuantize.
  ///
  /// In en, this message translates to:
  /// **'Quantize'**
  String get transportQuantize;

  /// No description provided for @transportQuantizeChip.
  ///
  /// In en, this message translates to:
  /// **'Q: {grid}'**
  String transportQuantizeChip(Object grid);

  /// No description provided for @transportTap.
  ///
  /// In en, this message translates to:
  /// **'TAP'**
  String get transportTap;

  /// No description provided for @transportThem.
  ///
  /// In en, this message translates to:
  /// **'More'**
  String get transportThem;

  /// No description provided for @warpMode.
  ///
  /// In en, this message translates to:
  /// **'{mode, select, stretch{Stretch} repitch{Re-Pitch} other{{mode}}}'**
  String warpMode(String mode);
}

class _AppLocalizationsDelegate extends LocalizationsDelegate<AppLocalizations> {
  const _AppLocalizationsDelegate();

  @override
  Future<AppLocalizations> load(Locale locale) {
    return SynchronousFuture<AppLocalizations>(lookupAppLocalizations(locale));
  }

  @override
  bool isSupported(Locale locale) => <String>['en', 'vi'].contains(locale.languageCode);

  @override
  bool shouldReload(_AppLocalizationsDelegate old) => false;
}

AppLocalizations lookupAppLocalizations(Locale locale) {
  // Lookup logic when only language code is specified.
  switch (locale.languageCode) {
    case 'en':
      return AppLocalizationsEn();
    case 'vi':
      return AppLocalizationsVi();
  }

  throw FlutterError(
    'AppLocalizations.delegate failed to load unsupported locale "$locale". This is likely '
    'an issue with the localizations generation tool. Please file an issue '
    'on GitHub with a reproducible sample app and the gen-l10n configuration '
    'that was used.',
  );
}
