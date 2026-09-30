// ignore: unused_import
import 'package:intl/intl.dart' as intl;
import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for English (`en`).
class AppLocalizationsEn extends AppLocalizations {
  AppLocalizationsEn([String locale = 'en']) : super(locale);

  @override
  String get aboutApi => 'API';

  @override
  String get aboutBuffer => 'Buffer';

  @override
  String get aboutDungBangFlutterEngineC => 'Built with Flutter + a C++ engine (JUCE). iPad only, landscape.';

  @override
  String get aboutEngine => 'Engine';

  @override
  String get aboutEngineGiaLoopcoreFake => 'Fake engine (LOOPCORE_FAKE)';

  @override
  String aboutFrames(Object frames) {
    return '$frames frames';
  }

  @override
  String get aboutLoai => 'Type';

  @override
  String get aboutLoopCore => 'LoopCore (JUCE)';

  @override
  String aboutPhienBan(Object p0, Object p1) {
    return 'Version $p0 ($p1)';
  }

  @override
  String get aboutSampleRate => 'Sample rate';

  @override
  String get aboutThietBi => 'Device';

  @override
  String get audioAudioChuaBat => 'Audio is off';

  @override
  String audioBars(int count) {
    String _temp0 = intl.Intl.pluralLogic(count, locale: localeName, other: '$count bars', one: '1 bar');
    return '$_temp0';
  }

  @override
  String get audioBuffer => 'Audio buffer';

  @override
  String get audioChoi => 'Playing';

  @override
  String get audioDangBatThuDuoc => 'On — recording available';

  @override
  String audioDangChay(Object frames, Object ms, Object hz) {
    return 'Running: $frames frames ($ms) @ $hz Hz';
  }

  @override
  String audioKhongDoiDuocBuffer(Object err) {
    return 'Couldn\'t change the buffer: $err';
  }

  @override
  String get audioLuonBat => 'On';

  @override
  String get audioMic => 'Microphone';

  @override
  String get audioMonitorMacDinhChoTrack => 'Default monitor for new audio tracks';

  @override
  String audioNhoHonTreItHon(Object p0) {
    return 'Smaller = lower latency but more CPU. $p0';
  }

  @override
  String get audioQuantizeNotKhiThuMidi => 'Quantize notes when recording MIDI';

  @override
  String get audioRungNheKhiLaunchThu => 'Haptics on launch / record';

  @override
  String get audioSoBarKhiThuVao => 'Recording length';

  @override
  String get audioThuAm => 'Recording';

  @override
  String get audioTuDo => 'Free';

  @override
  String get audioTuDoGhiChu => 'Free: tap the cell again (or LOOP) to finish the take';

  @override
  String get audioTuDong => 'Auto';

  @override
  String get audioTuDongNgheMicroKhi =>
      'Auto: hear the mic while the track is armed. Use headphones to avoid feedback.';

  @override
  String get bannerAudioBiNgatCuocGoi => 'Audio interrupted (call, Siri…)';

  @override
  String bannerDaGiaiPhongBoNho(Object mb) {
    return 'Memory freed ($mb MB in use)';
  }

  @override
  String get bannerDangDungTaiNgheBluetooth =>
      'Bluetooth headphones: high latency, not suited for recording or playing in time.';

  @override
  String bannerKhongOverdubDuoc(Object track, Object slot) {
    return 'Can\'t overdub cell $track·$slot: the audio clip is in Re-Pitch at a different tempo or not ready yet. The clip keeps playing.';
  }

  @override
  String bannerLoiEngine(Object p0) {
    return 'Engine error: $p0';
  }

  @override
  String get browserBanThuCuaToi => 'My recordings';

  @override
  String get browserChuaCoBanThu => 'No recordings yet. Record a new instrument in the Instrument tab.';

  @override
  String get browserChuaCoYeuThich => 'No favorites yet. Long-press an item to add it.';

  @override
  String browserDaBoYeuThich(String name) {
    return 'Removed from Favorites: $name';
  }

  @override
  String browserDaThemVaoO(Object p0, Object p1) {
    return 'Added \"$p0\" to cell $p1';
  }

  @override
  String browserDaThemYeuThich(String name) {
    return 'Added to Favorites: $name';
  }

  @override
  String browserGan(Object trackName) {
    return 'assign → $trackName';
  }

  @override
  String get browserKeoVaoTrack => 'Drag onto a track header or clip cell';

  @override
  String get browserKhongCoKetQua => 'No results';

  @override
  String browserKhongDocDuocThuVien(Object e) {
    return 'Couldn\'t read the library: $e';
  }

  @override
  String browserLoopInfo(Object bpm, int beats) {
    String _temp0 = intl.Intl.pluralLogic(beats, locale: localeName, other: '$beats beats', one: '1 beat');
    return '$bpm BPM · $_temp0';
  }

  @override
  String browserODaCoClip(int cell) {
    return 'Cell $cell already has a clip';
  }

  @override
  String browserThemVao(Object trackName) {
    return 'add to $trackName';
  }

  @override
  String get browserThuMucTrong => 'This folder is empty';

  @override
  String get browserTimKiem => 'Search library';

  @override
  String get browserTrackAudioCoClip => 'This audio track has clips. Pick an empty track to assign an instrument.';

  @override
  String get browserTrackNayKhongConO => 'No empty cells left on this track';

  @override
  String get browserTrackNhacCuCoClip => 'This instrument track has MIDI clips. Pick an empty track for the loop.';

  @override
  String get browserTuNgheThu => 'Auto preview';

  @override
  String get browserYeuThich => 'Favorites';

  @override
  String get clipChoClipChay => 'Starting…';

  @override
  String get clipChonTatCa => 'Select all';

  @override
  String get clipClear => 'Clear';

  @override
  String get clipCuonHaiNgon => 'Scroll with two fingers';

  @override
  String get clipDangGhi => 'Recording';

  @override
  String get clipDoDaiClip => 'Clip length';

  @override
  String get clipDrumsHint => 'Drum loops sound more natural with Re-Pitch';

  @override
  String clipGain(Object db) {
    return 'Clip gain: $db dB';
  }

  @override
  String get clipGhi => 'Record';

  @override
  String get clipHoanTacOverdub => 'Undo overdub';

  @override
  String get clipHoanTacSua => 'Undo';

  @override
  String get clipKhongDuCho => 'Not enough room in the clip';

  @override
  String clipKhongDuChoToiDa(int bars) {
    String _temp0 = intl.Intl.pluralLogic(
      bars,
      locale: localeName,
      other: 'Not enough room: a clip can be at most $bars bars.',
      one: 'Not enough room: a clip can be at most 1 bar.',
    );
    return '$_temp0';
  }

  @override
  String get clipLamLai => 'Redo';

  @override
  String clipLoopTuSBeatGoc(Object p0, Object p1, Object p2) {
    return 'Loop from $p0 s · $p1 beats · original $p2 BPM';
  }

  @override
  String get clipLuiVaXoa => 'Back and delete';

  @override
  String get clipLuoi => 'Grid';

  @override
  String get clipModeChon => 'Select';

  @override
  String get clipModeVe => 'Draw';

  @override
  String get clipNghi => 'Rest';

  @override
  String get clipNhanBan => 'Duplicate selected notes';

  @override
  String clipNotBeat(int p0, Object p1) {
    String _temp0 = intl.Intl.pluralLogic(p0, locale: localeName, other: '$p0 notes', one: '1 note');
    return '$_temp0 · $p1 beats';
  }

  @override
  String get clipOTrongBanPhim => 'Empty cell. Open the keyboard to play, record live or enter notes step by step.';

  @override
  String get clipQuangTamLen => 'Octave up';

  @override
  String get clipQuangTamXuong => 'Octave down';

  @override
  String get clipQuantize => 'Quantize';

  @override
  String clipQuantizeGrid(Object grid) {
    return 'Quantize $grid';
  }

  @override
  String get clipStep => 'Step';

  @override
  String get clipTangDoDai => 'Extend clip';

  @override
  String clipTangDoDaiHoi(int bars) {
    String _temp0 = intl.Intl.pluralLogic(
      bars,
      locale: localeName,
      other: 'Extend the clip to $bars bars to fit the copy?',
      one: 'Extend the clip to 1 bar to fit the copy?',
    );
    return '$_temp0';
  }

  @override
  String get clipVelocity => 'Velocity';

  @override
  String get clipWarp => 'Warp';

  @override
  String clipXoaNotDaChon(Object p0) {
    return 'Delete selected ($p0)';
  }

  @override
  String get clipZoom => 'Zoom';

  @override
  String get commonCancel => 'Cancel';

  @override
  String get commonOk => 'OK';

  @override
  String demoName(String token) {
    String _temp0 = intl.Intl.selectLogic(token, {
      'demo': 'Demo',
      'drums': 'Drums',
      'bass': 'Bass',
      'keys': 'Keys',
      'lead': 'Lead',
      'beatA': 'Beat A',
      'beatB': 'Beat B',
      'fill': 'Fill',
      'halfTime': 'Half-time',
      'bassA': 'Bass A',
      'bassB': 'Bass B',
      'walk': 'Walk',
      'chords': 'Chords',
      'stab': 'Stab',
      'melody': 'Melody',
      'hook': 'Hook',
      'other': '$token',
    });
    return '$_temp0';
  }

  @override
  String errorText(String code) {
    String _temp0 = intl.Intl.selectLogic(code, {
      'INVALID_ARG': 'Invalid request',
      'NOT_CREATED': 'Engine not started',
      'ALREADY_CREATED': 'Engine already started',
      'NOT_IMPLEMENTED': 'Not supported by the engine yet',
      'AUDIO_DEVICE': 'Audio device error',
      'MIC_PERMISSION': 'No microphone access',
      'FILE_NOT_FOUND': 'File not found',
      'FILE_FORMAT': 'Unsupported file format',
      'DISK_FULL': 'Storage is full',
      'FILE_WRITE': 'Couldn\'t write the file',
      'OUT_OF_MEMORY': 'Out of memory',
      'QUEUE_FULL': 'Engine is busy, try again',
      'JOB_CANCELLED': 'Cancelled',
      'JOB_NOT_FOUND': 'Task not found',
      'PITCH_NOT_DETECTED': 'Couldn\'t detect the pitch',
      'OVERDUB_UNSUPPORTED': 'This clip can\'t be overdubbed',
      'CELL_EMPTY': 'The cell is still empty after recording',
      'other': 'Engine error ($code)',
    });
    return '$_temp0';
  }

  @override
  String get exportChamDeBatDauGhi => 'Tap to start recording';

  @override
  String get exportChiaSe => 'Share';

  @override
  String exportClippedSamples(int count) {
    String _temp0 = intl.Intl.pluralLogic(
      count,
      locale: localeName,
      other: '$count samples clipped: lower the master gain',
      one: '1 sample clipped: lower the master gain',
    );
    return '$_temp0';
  }

  @override
  String get exportDangGhi => 'Recording  ';

  @override
  String get exportDinhDang => 'Format';

  @override
  String get exportDungGhiJam => 'Stop jam recording';

  @override
  String exportExportLoi(Object p0) {
    return 'Export failed: $p0';
  }

  @override
  String get exportGhiBuoiJam => 'Record jam';

  @override
  String get exportGhiToanBoDauRa =>
      'Records the whole master output while you play (WAV, saved in Exports/). Toggle it quickly with the ● button in the top bar.';

  @override
  String get exportHuy => 'Cancel';

  @override
  String exportKhongGhiDuoc(Object err) {
    return 'Couldn\'t record: $err';
  }

  @override
  String exportKhongMoDuocShareSheet(Object e) {
    return 'Couldn\'t open the share sheet: $e';
  }

  @override
  String get exportMenu => 'Export…';

  @override
  String get exportMoiTrackMotFile => 'One file per track';

  @override
  String exportResult(Object file, Object time) {
    return '$file · $time';
  }

  @override
  String get exportScene => 'Scene';

  @override
  String exportSceneItem(Object number, Object name) {
    return '$number. $name';
  }

  @override
  String get exportSceneNayChuaCoClip => 'This scene has no clips';

  @override
  String get exportSceneTab => 'Export scene';

  @override
  String get exportSoBar => 'Bars';

  @override
  String exportStemCount(int count) {
    String _temp0 = intl.Intl.pluralLogic(count, locale: localeName, other: '$count stems', one: '1 stem');
    return ' + $_temp0';
  }

  @override
  String get exportStemsLabel => 'Stems';

  @override
  String get exportTitle => 'Export';

  @override
  String get fxBat => 'On';

  @override
  String get fxBatLai => 'Turn on';

  @override
  String get fxBypass => 'Bypass';

  @override
  String get fxDoiLoaiFx => 'Change FX type';

  @override
  String get fxKeoDocDeChinhCham => 'Drag up/down to adjust · double-tap to reset';

  @override
  String get fxMasterCard => 'Master · EQ3 · Limiter';

  @override
  String fxParam(String name) {
    String _temp0 = intl.Intl.selectLogic(name, {
      'mode': 'Mode',
      'cutoff': 'Cutoff',
      'reso': 'Reso',
      'rate': 'Rate',
      'feedback': 'Feedback',
      'mix': 'Mix',
      'pingpong': 'Ping-pong',
      'size': 'Size',
      'damping': 'Damping',
      'width': 'Width',
      'low': 'Low',
      'mid': 'Mid',
      'high': 'High',
      'thresh': 'Thresh',
      'ratio': 'Ratio',
      'attack': 'Attack',
      'release': 'Release',
      'makeup': 'Makeup',
      'ceiling': 'Ceiling',
      'other': '$name',
    });
    return '$_temp0';
  }

  @override
  String fxSlotTrong(Object p0) {
    return 'Slot $p0 empty';
  }

  @override
  String get fxTat => 'Off';

  @override
  String get fxThemFx => 'Add FX';

  @override
  String get fxToggleOff => 'OFF';

  @override
  String get fxToggleOn => 'ON';

  @override
  String fxType(String type) {
    String _temp0 = intl.Intl.selectLogic(type, {
      'filter': 'Filter',
      'delay': 'Delay',
      'reverb': 'Reverb',
      'eq3': 'EQ3',
      'comp': 'Compressor',
      'other': '$type',
    });
    return '$_temp0';
  }

  @override
  String get fxXoaFx => 'Remove FX';

  @override
  String get instrumentBanPhim => 'Keyboard';

  @override
  String get instrumentKhongTimThayNhacCu => 'Instrument not found';

  @override
  String instrumentMode(String mode) {
    String _temp0 = intl.Intl.selectLogic(mode, {'natural': 'Natural', 'classic': 'Classic', 'other': '$mode'});
    return '$_temp0';
  }

  @override
  String get instrumentPad => 'Pads';

  @override
  String get instrumentThuAmMoi => 'Record new';

  @override
  String jamDroppedMs(Object ms) {
    return 'Lost $ms ms: the disk could not keep up';
  }

  @override
  String get latencyAppPhatTiengClickRa =>
      'The app plays clicks through the speaker and listens through the mic to measure the round trip. Keep the iPad somewhere quiet at a moderate volume — or connect a loopback cable from output to input. Takes about 5 seconds. The offset below is set from the result.';

  @override
  String get latencyBanThuNgheBiTre =>
      'If recordings sound late against the beat, increase it; if early, decrease it. Saved and applied every time the app opens.';

  @override
  String get latencyBuThem => 'Extra offset';

  @override
  String get latencyCanQuyenMicroDeDo => 'Microphone access is needed to measure';

  @override
  String get latencyChinhTay => 'Manual';

  @override
  String get latencyChuaCo => 'not set';

  @override
  String latencyDaLuuEngineChuaAp(Object err) {
    return 'Saved, but the engine couldn\'t apply it yet: $err';
  }

  @override
  String latencyDeviceBaoBuSampleLech(Object p0, Object p1, Object p2, Object p3) {
    return 'Device reports $p0 · offset $p1 samples · spread $p2 · confidence $p3%';
  }

  @override
  String get latencyDoDoTre => 'Measure latency';

  @override
  String latencyDoLoi(Object p0) {
    return 'Measurement failed: $p0';
  }

  @override
  String get latencyDoTreVongThuPhat => 'Round-trip latency (record ↔ play)';

  @override
  String get latencyDoTuDong => 'Automatic measurement';

  @override
  String get latencyEngineDangDung => 'Engine is using';

  @override
  String latencyFailReason(String reason) {
    String _temp0 = intl.Intl.selectLogic(reason, {
      'NO_SIGNAL': 'Didn\'t hear the test clicks — turn the volume up or connect a loopback cable',
      'TOO_NOISY': 'Too much background noise — try somewhere quieter',
      'INCONSISTENT': 'The measurements didn\'t agree — keep the iPad still and try again',
      'DEVICE_CHANGED': 'The audio device changed during the test — try again',
      'TIMEOUT': 'The test took too long — try again',
      'other': '$reason',
    });
    return '$_temp0';
  }

  @override
  String get latencyKhongBatDuocAudio => 'Couldn\'t start audio';

  @override
  String get latencyLanDoGanNhat => 'Last measurement';

  @override
  String latencyOffsetValue(Object ms, int samples) {
    String _temp0 = intl.Intl.pluralLogic(samples, locale: localeName, other: '$samples samples', one: '1 sample');
    return '$ms ms ($_temp0)';
  }

  @override
  String latencySamplesMs(int samples, Object ms) {
    String _temp0 = intl.Intl.pluralLogic(samples, locale: localeName, other: '$samples samples', one: '1 sample');
    return '$_temp0 · $ms ms';
  }

  @override
  String get latencyVe0 => 'Reset to 0';

  @override
  String learnKind(String kind) {
    String _temp0 = intl.Intl.selectLogic(kind, {
      'clip': 'Clip',
      'scene': 'Scene',
      'transport': 'Transport',
      'stopAll': 'Stop all',
      'trackGain': 'Track gain',
      'trackMute': 'Track mute',
      'fx': 'FX parameter',
      'loopButton': 'LOOP button',
      'trackStop': 'Stop track',
      'undoOverdub': 'Undo overdub',
      'other': '$kind',
    });
    return '$_temp0';
  }

  @override
  String libraryFolder(String name) {
    String _temp0 = intl.Intl.selectLogic(name, {
      'drums': 'Drums',
      'instruments': 'Instruments',
      'keys': 'Keys',
      'strings': 'Strings',
      'windsBrass': 'Winds & Brass',
      'synth': 'Synth',
      'loops': 'Loops',
      'other': '$name',
    });
    return '$_temp0';
  }

  @override
  String libraryTag(String tag) {
    String _temp0 = intl.Intl.selectLogic(tag, {
      'drums': 'Drums',
      'bass': 'Bass',
      'keys': 'Keys',
      'synth': 'Synth',
      'pad': 'Pad',
      'click': 'Click',
      'tone': 'Tone',
      'test': 'Test',
      'hiphop': 'Hip-hop',
      'funk': 'Funk',
      'melody': 'Melody',
      'vocal': 'Vocal',
      'fx': 'FX',
      'strings': 'Strings',
      'winds': 'Winds',
      'brass': 'Brass',
      'percussion': 'Percussion',
      'other': '$tag',
    });
    return '$_temp0';
  }

  @override
  String get licensesChuaCoFileGiayPhep => 'No content license files yet.';

  @override
  String get licensesChuaTichHopP406 => 'Not integrated yet (P4-06)';

  @override
  String get licensesGhiCong => 'Credits';

  @override
  String get licensesGiayPhepCacGoiFlutter => 'Licenses of Flutter/Dart packages';

  @override
  String get licensesGoiFlutterDart => 'Flutter / Dart packages';

  @override
  String get licensesJuce9DungTheoGoi => 'JUCE 9 — used under the Starter plan (JUCE EULA)';

  @override
  String licensesKhongDocDuoc(Object asset) {
    return 'Couldn\'t read $asset';
  }

  @override
  String get licensesNoiDungAmThanh => 'Sound content';

  @override
  String get licensesThuVienBenThuBa => 'Third-party libraries';

  @override
  String get linkBamOMotMayThi => 'Pressing ▶/■ on one device starts/stops the others too.';

  @override
  String get linkBatLink2 => 'Enable Link';

  @override
  String get linkCanQuyenMangCucBo => 'Needs local network access (iOS asks the first time you turn it on).';

  @override
  String linkDangNoiThietBi(int peers) {
    String _temp0 = intl.Intl.pluralLogic(
      peers,
      locale: localeName,
      other: '$peers peers',
      one: '1 peer',
      zero: 'no peers',
    );
    return 'Connected: $_temp0';
  }

  @override
  String get linkDongBoStartStop => 'Sync Start/Stop';

  @override
  String get linkDongBoTempoVaNhip =>
      'Sync tempo and beat with other apps/devices on the same Wi-Fi. Saved per project.';

  @override
  String get linkEngineChuaCoLinkP4 =>
      'The engine doesn\'t have Link yet (P4-06) — your choice is saved in the project';

  @override
  String get linkTitle => 'Ableton Link';

  @override
  String loopButtonLabel(String state) {
    String _temp0 = intl.Intl.selectLogic(state, {
      'idle': 'LOOP',
      'rec': 'REC',
      'play': 'PLAY',
      'dub': 'DUB',
      'other': 'LOOP',
    });
    return '$_temp0';
  }

  @override
  String get loopButtonTooltip => 'LOOP: tap = record → play → overdub · double-tap = stop · hold = undo';

  @override
  String get loopDungTrack => 'Stop track';

  @override
  String get loopHoanTac => 'Undo overdub / delete clip';

  @override
  String get loopXoa => 'Delete';

  @override
  String get loopXoaClipBody => 'There is no overdub layer to undo.';

  @override
  String loopXoaClipTitle(Object name) {
    return 'Delete clip \"$name\"?';
  }

  @override
  String menuClipMidiTrong(int bars) {
    String _temp0 = intl.Intl.pluralLogic(
      bars,
      locale: localeName,
      other: 'Empty MIDI clip · $bars bars',
      one: 'Empty MIDI clip · 1 bar',
    );
    return '$_temp0';
  }

  @override
  String get menuCopy => 'Copy';

  @override
  String get menuCopyToiDay => 'Copy here';

  @override
  String menuDan(Object p0) {
    return 'Paste \"$p0\"';
  }

  @override
  String get menuDanChuaCopyClipNao => 'Paste (nothing copied)';

  @override
  String get menuDiChuyenToiDay => 'Move here';

  @override
  String get menuDoiTenClip => 'Rename clip';

  @override
  String get menuKhongConOTrongBen => 'No empty cell below';

  @override
  String get menuMauTrack => 'Track color';

  @override
  String get menuODichDaCoClip => 'The target cell already has a clip';

  @override
  String metronomeMode(String mode) {
    String _temp0 = intl.Intl.selectLogic(mode, {
      'off': 'Off',
      'always': 'On',
      'recordOnly': 'Rec only',
      'other': '$mode',
    });
    return '$_temp0';
  }

  @override
  String get midiAnyChannel => 'any channel';

  @override
  String get midiAnyDevice => 'any device';

  @override
  String get midiBatDauLearn => 'Start learn';

  @override
  String get midiBoGan => 'Remove mapping';

  @override
  String midiChannel(Object number) {
    return 'channel $number';
  }

  @override
  String get midiChuaGanGi => 'Nothing mapped yet.';

  @override
  String get midiChuaThayThietBiMidi => 'No MIDI devices found. Connect via USB or pair Bluetooth MIDI.';

  @override
  String midiClipO(Object p0, Object p1) {
    return 'Clip · $p0 · cell $p1';
  }

  @override
  String get midiDenLedPhanHoiTrang => 'Clip status LEDs (Launchpad) arrive in P4-05.';

  @override
  String get midiDungMoiClipTheoQuantize => 'Stops every clip on the quantize grid.';

  @override
  String get midiDungTatCa => 'Stop all';

  @override
  String midiEnableFailed(Object error) {
    return 'Couldn\'t change the device: $error';
  }

  @override
  String get midiEngineChuaHoTroMidi => 'The engine doesn\'t support MIDI yet (P4-01)';

  @override
  String midiFootswitchGhiChu(String kind) {
    String _temp0 = intl.Intl.selectLogic(kind, {
      'loopButton':
          'The footswitch acts like the on-screen LOOP button on the selected track: record → play → overdub.',
      'trackStop': 'The footswitch stops the selected track (at the quantize boundary).',
      'undoOverdub': 'The footswitch undoes the last overdub of the playing clip on the selected track.',
      'other': '$kind',
    });
    return '$_temp0';
  }

  @override
  String midiFxSlotItem(Object number, Object type) {
    return 'Slot $number · $type';
  }

  @override
  String get midiGanDieuKhienMidi2 => 'Map MIDI control';

  @override
  String get midiGanMoi => 'New mapping';

  @override
  String get midiGanMotNutNumTren =>
      'Map a button/knob on your controller to a clip or FX parameter. Saved in the project.';

  @override
  String get midiGhepThietBiBluetoothMidi => 'Pair Bluetooth MIDI device';

  @override
  String get midiLamMoi => 'Refresh';

  @override
  String midiLearnFailed(Object error) {
    return 'Couldn\'t start learn: $error';
  }

  @override
  String get midiMidiLearnProjectDangMo => 'MIDI learn (current project)';

  @override
  String midiO(Object p0) {
    return 'Cell $p0';
  }

  @override
  String midiParamFallback(Object id) {
    return 'parameter $id';
  }

  @override
  String midiSlot(Object number) {
    return 'slot $number';
  }

  @override
  String midiSourceCc(Object number) {
    return 'CC $number';
  }

  @override
  String midiSourceNote(Object number) {
    return 'Note $number';
  }

  @override
  String get midiThietBiRa => 'Outputs';

  @override
  String get midiThietBiVao => 'Inputs';

  @override
  String get midiTrackNayChuaCoFx => 'This track has no FX.';

  @override
  String get midiVanHoacBamMotNut => 'Turn a knob or press a button on your controller…';

  @override
  String get mixerMaster => 'Master';

  @override
  String get mixerMonitor => 'Monitor';

  @override
  String get mixerMonitorLuonBat => 'Monitor: On';

  @override
  String get mixerMonitorTat => 'Monitor: Off';

  @override
  String get mixerMonitorTuDongKhiArm => 'Monitor: Auto (when armed)';

  @override
  String monitorBadge(String mode) {
    String _temp0 = intl.Intl.selectLogic(mode, {'off': 'Off', 'auto': 'Auto', 'always': 'On', 'other': '$mode'});
    return '$_temp0';
  }

  @override
  String get onboardingAmThanhChiLuuTren => 'Audio stays on your iPad and is never sent anywhere.';

  @override
  String get onboardingBienMotTiengBanThu => 'Turn a sound you record into an instrument you can play on the keyboard.';

  @override
  String get onboardingBoQua => 'Skip';

  @override
  String get onboardingChamMotODePhat => 'Tap a cell to launch a clip — it starts on the next bar.';

  @override
  String get onboardingChamSoSceneBenPhai => 'Tap a scene on the right to launch a whole row at once.';

  @override
  String get onboardingChoPhepMicro => 'Allow microphone';

  @override
  String get onboardingChuaCoQuyenMicroThu => 'No microphone access yet: we\'ll ask again when you record.';

  @override
  String get onboardingDeSau => 'Later';

  @override
  String get onboardingDoDoTreDeBan => 'Measure latency so your recordings land on the beat.';

  @override
  String onboardingKhongTaoDuocProjectDemo(Object e) {
    return 'Couldn\'t create the demo project: $e';
  }

  @override
  String get onboardingMicroDangBiTatBan =>
      'The microphone is off. You can still play loops and built-in instruments; turn it back on in Settings to record.';

  @override
  String get onboardingMoCaiDat => 'Open Settings';

  @override
  String get onboardingMoProjectDemo => 'Open demo project';

  @override
  String get onboardingMusicLooperCanMicro => 'Music Looper needs the microphone';

  @override
  String get onboardingTabInstrumentPhiaDuoiDe => 'Use the Instrument tab below to play pads and keys.';

  @override
  String get onboardingThuGiongHatGuitarThanh => 'Record vocals, guitar… into loops that stay in time.';

  @override
  String get onboardingThuNgayVoiProjectDemo => 'Try the demo project';

  @override
  String get onboardingTiepTuc => 'Continue';

  @override
  String get onboardingTuTaoProject => 'Start my own project';

  @override
  String panelTab(String tab) {
    String _temp0 = intl.Intl.selectLogic(tab, {
      'clip': 'Clip',
      'instrument': 'Instrument',
      'mixer': 'Mixer',
      'fx': 'FX',
      'browser': 'Browser',
      'other': '$tab',
    });
    return '$_temp0';
  }

  @override
  String projectsBanSao(Object name) {
    return '$name (copy)';
  }

  @override
  String get projectsCaiDat => 'Settings';

  @override
  String get projectsChuaCoProjectBamProject => 'No projects yet. Tap \"New project\" or \"Create demo project\".';

  @override
  String get projectsDoiTen => 'Rename';

  @override
  String get projectsKhongDocDuocProject => 'Couldn\'t read projects';

  @override
  String projectsLoi(Object e) {
    return 'Error: $e';
  }

  @override
  String get projectsNhanBan => 'Duplicate';

  @override
  String get projectsProjectJsonBiHongDa => 'project.json was damaged — opened the previous save (.bak)';

  @override
  String get projectsProjectMoi => 'New project';

  @override
  String projectsSuaLanCuoi(Object p0) {
    return 'Last edited: $p0';
  }

  @override
  String get projectsTaoProjectDemo => 'Create demo project';

  @override
  String get projectsTitle => 'Projects';

  @override
  String projectsXoa(Object p0) {
    return 'Delete \"$p0\"?';
  }

  @override
  String get projectsXoa2 => 'Delete';

  @override
  String get projectsXoaCaBanThuAm => 'This also deletes the recordings in the project. This can\'t be undone.';

  @override
  String quantizeGrid(String grid) {
    String _temp0 = intl.Intl.selectLogic(grid, {
      'none': 'None',
      'sixteenth': '1/16',
      'eighth': '1/8',
      'quarter': '1/4',
      'half': '1/2',
      'bar1': '1 bar',
      'bar2': '2 bars',
      'bar4': '4 bars',
      'other': '$grid',
    });
    return '$_temp0';
  }

  @override
  String recordQuantize(String grid) {
    String _temp0 = intl.Intl.selectLogic(grid, {'off': 'Off', 'sixteenth': '1/16', 'eighth': '1/8', 'other': '$grid'});
    return '$_temp0';
  }

  @override
  String get samplerCanQuyenMicroDeThu =>
      'Microphone access is needed to record (Settings → Music Looper → Microphone)';

  @override
  String samplerChamDeThuMotNot(Object p0) {
    return 'Tap to record one note (up to $p0 s)';
  }

  @override
  String get samplerDangCatKhoangLangVa => 'Trimming silence and detecting the note…';

  @override
  String samplerDangTaoNhacCu13(Object p0) {
    return 'Creating instrument (13 zones)… $p0%';
  }

  @override
  String samplerDoTinCay(Object p0) {
    return 'confidence $p0%';
  }

  @override
  String samplerHatHoacThoiMotNot(Object p0) {
    return 'Sing or play one steady note… stops after $p0 s';
  }

  @override
  String samplerKhongBatDuocAudio(Object p0) {
    return 'Couldn\'t start audio: $p0';
  }

  @override
  String get samplerKhongChacVeCaoDo => 'Pitch is uncertain — set the root note with − / + before creating';

  @override
  String get samplerKhongDoDuocCaoDo => 'Couldn\'t detect the pitch — pick the root note and create again';

  @override
  String get samplerKhongNgheThayTieng => 'Didn\'t hear any sound — check the mic and record again';

  @override
  String get samplerNotGoc => 'Root note: ';

  @override
  String samplerPhanTichLoi(Object p0) {
    return 'Analysis failed: $p0';
  }

  @override
  String get samplerTaoNhacCu => 'Create instrument';

  @override
  String samplerTaoNhacCuLoi(Object p0) {
    return 'Couldn\'t create the instrument: $p0';
  }

  @override
  String get samplerThuAmNhacCu => 'Record → instrument';

  @override
  String get samplerThuLai => 'Retake';

  @override
  String samplerTiengThu(Object p0) {
    return 'Recording $p0';
  }

  @override
  String samplerTrim(Object start, Object end) {
    return 'Trim $start → $end s';
  }

  @override
  String sessionBanThu(Object p0) {
    return 'Take $p0';
  }

  @override
  String get sessionBatEditRoiChamMot => 'Turn on ✎ Edit, then tap a cell to select a clip';

  @override
  String get sessionChoPhepMicro => 'Allow microphone';

  @override
  String get sessionChuaCoQuyenMicroChi => 'No microphone access: playback only, recording is off';

  @override
  String get sessionChuaMoProject => 'No project open';

  @override
  String sessionDangMoProject(Object p0, Object p1) {
    return 'Opening project… $p0/$p1';
  }

  @override
  String sessionKhongLuuDuocProject(Object reason) {
    return 'Couldn\'t save the project: $reason';
  }

  @override
  String sessionLoiKhiMoProject(int p0, Object p1) {
    String _temp0 = intl.Intl.pluralLogic(p0, locale: localeName, other: '$p0 errors', one: '1 error');
    return '$_temp0 while opening the project: $p1';
  }

  @override
  String sessionMidiClipName(Object number) {
    return 'MIDI $number';
  }

  @override
  String get sessionMoPanel => 'Open panel';

  @override
  String get sessionMoRongPanel => 'Expand panel';

  @override
  String sessionMode(String mode) {
    String _temp0 = intl.Intl.selectLogic(mode, {'perform': 'Perform', 'edit': 'Edit', 'other': '$mode'});
    return '$_temp0';
  }

  @override
  String sessionOTrong(Object p0, Object p1) {
    return 'Cell $p0·$p1 is empty';
  }

  @override
  String sessionSceneName(Object number) {
    return 'Scene $number';
  }

  @override
  String get sessionThuGon => 'Collapse';

  @override
  String get sessionThuNhoPanel => 'Restore panel';

  @override
  String get sessionTitle => 'Session';

  @override
  String sessionTrackName(Object number) {
    return 'Track $number';
  }

  @override
  String get settingsNeedsProjectLink => 'Open a project to turn on Link.';

  @override
  String get settingsNeedsProjectMidi => 'Open a project to map MIDI controls.';

  @override
  String settingsSection(String section) {
    String _temp0 = intl.Intl.selectLogic(section, {
      'audio': 'Audio',
      'latency': 'Latency',
      'midi': 'MIDI',
      'link': 'Link',
      'licenses': 'Licenses',
      'about': 'About',
      'other': '$section',
    });
    return '$_temp0';
  }

  @override
  String get spike1MsDoLai => '✗ (> 1 ms, measure again)';

  @override
  String spikeApiversion(Object p0) {
    return 'apiVersion = $p0';
  }

  @override
  String get spikeAppCanMicroDeThu =>
      'The app needs the microphone to record and measure latency.\nOpen Settings → Music Looper → turn on Microphone, then come back and tap Start.';

  @override
  String get spikeAudioBiNgatSiriCuoc => 'Audio interrupted (Siri/call…)';

  @override
  String get spikeBat => 'on';

  @override
  String get spikeBlock20050Ms => 'Block 200 / 50 ms';

  @override
  String get spikeCamTaiNgheTruoc => 'Plug in headphones first';

  @override
  String get spikeCanQuyenMicro => 'Microphone access needed';

  @override
  String spikeCanhBaoBoNhoMb(Object p0) {
    return 'Memory warning: $p0 MB';
  }

  @override
  String get spikeCheaper => 'Cheaper';

  @override
  String get spikeCo => 'yes';

  @override
  String get spikeCoLatencyCao => 'yes ⚠ high latency';

  @override
  String spikeCpuDinh(Object cpuAvg, Object cpuPeak) {
    return 'CPU  $cpuAvg% · peak $cpuPeak%';
  }

  @override
  String get spikeDaCamBat => 'Plugged in, turn on';

  @override
  String get spikeDangChay => 'Running…';

  @override
  String get spikeDangDo => 'Measuring…';

  @override
  String get spikeDangThu4Giay => 'Recording 4 seconds…';

  @override
  String get spikeDeSau => 'Later';

  @override
  String spikeDeviceInKenh(Object p0, Object p1) {
    return 'device: $p0 · in $p1 ch';
  }

  @override
  String get spikeDo => 'Measure';

  @override
  String get spikeDoLatency => 'Measure latency';

  @override
  String get spikeDoLatencyBoTaiNghe =>
      'Measure latency: no headphones, external speaker, quiet room (~3 s, sine/loop paused). Stretch bench uses the 4-second recording.';

  @override
  String get spikeEventRouteNgatXrun => 'Events (route, interruptions, xruns)';

  @override
  String get spikeFakeengineChuaCoLoopcoreXcframework => 'FakeEngine (no LoopCore.xcframework yet)';

  @override
  String get spikeFormant => 'Formant';

  @override
  String get spikeGain => 'Gain';

  @override
  String get spikeHetNgatAudio => 'Interruption ended';

  @override
  String get spikeHuy => 'Cancel';

  @override
  String spikeJobDangChay(Object op, Object jobId) {
    return '$op: job $jobId running…';
  }

  @override
  String spikeJobDangChay2(Object op, Object jobId, Object p2) {
    return '$op: job $jobId running… $p2%';
  }

  @override
  String spikeJobThatBai(Object op, Object jobId, Object p2, Object p3) {
    return '$op (job $jobId) failed: $p2 $p3';
  }

  @override
  String get spikeKetQua => 'Results';

  @override
  String get spikeKhong => 'no';

  @override
  String spikeKhongDoDuocMicKhong(Object p0) {
    return 'Couldn\'t measure: the mic didn\'t hear the chirp clearly (inputPeak = $p0).\nTurn the speaker up, unplug headphones, measure in a quiet room and try again.';
  }

  @override
  String spikeLeAudioStartLoi(Object p0) {
    return 'le_audio_start error: $p0';
  }

  @override
  String spikeLeCreateLoi(Object p0) {
    return 'le_create error: $p0';
  }

  @override
  String get spikeLoi => 'error';

  @override
  String spikeLoiEngine(Object p0) {
    return 'Engine error: $p0';
  }

  @override
  String get spikeLoopcoreSpike => 'LoopCore spike';

  @override
  String get spikeMoCaiDat => 'Open Settings';

  @override
  String get spikeModeDefault => 'Mode default';

  @override
  String get spikeModeMeasurement => 'Mode measurement';

  @override
  String get spikeNaudioChuaChayBamStart => '\nAudio is not running → tap Start audio first.';

  @override
  String get spikePassthroughCanTaiNghe => 'Passthrough (headphones needed)';

  @override
  String get spikePassthroughDuaMicRaLoa =>
      'Passthrough sends the mic to the speaker. Without headphones you will get feedback.';

  @override
  String get spikePhatLoop => 'Play loop';

  @override
  String get spikeProjectsDev => 'Projects (dev)';

  @override
  String spikeRoundTripDoDuocMs(
    Object p0,
    Object p1,
    Object p2,
    Object p3,
    Object p4,
    Object p5,
    Object p6,
    Object runs,
  ) {
    return 'Measured round-trip: $p0 ms ($p1 smp)\niOS reports: $p2 ms ($p3 smp)\nSpread between runs: $p4 ms $p5 · valid $p6/$runs';
  }

  @override
  String spikeRouteDoiTaiNgheDay(Object p0, Object p1) {
    return 'Route changed: wired/interface=$p0, Bluetooth=$p1';
  }

  @override
  String get spikeSessionInfo => 'Session info';

  @override
  String spikeSineHzGain(Object p0, Object p1) {
    return 'Sine $p0 Hz · gain $p1';
  }

  @override
  String spikeSpikeSetbuffersizeLoi(Object p0) {
    return 'spike.setBufferSize error: $p0';
  }

  @override
  String spikeSpikeSetsessionmodeLoi(Object p0) {
    return 'spike.setSessionMode error: $p0';
  }

  @override
  String get spikeStartAudio => 'Start audio';

  @override
  String get spikeStopAudio => 'Stop audio';

  @override
  String get spikeStretchBench => 'Stretch bench';

  @override
  String spikeTaiGiaLapVoice(Object voices) {
    return 'Simulated load: $voices voices';
  }

  @override
  String get spikeTanSo => 'Frequency';

  @override
  String get spikeTat => 'off';

  @override
  String get spikeThu4Giay => 'Record 4 seconds';

  @override
  String spikeThuXong(Object frames, Object seconds) {
    return 'Recorded: $frames frames ($seconds s)';
  }

  @override
  String spikeTongZoneMsFormantNsetup(
    Object p0,
    Object p1,
    Object p2,
    Object p3,
    Object p4,
    Object p5,
    Object zones,
    Object p7,
  ) {
    return 'Total $p0 zones: $p1 ms $p2 · formant $p3\nSetup $p4 ms · file write $p5 ms\nms/zone  $zones\n$p7 WAV files: Files app → On My iPad → Music Looper → spike';
  }

  @override
  String spikeXrunTong(Object totalXruns) {
    return 'Xrun (total $totalXruns)';
  }

  @override
  String tempoMode(String mode) {
    String _temp0 = intl.Intl.selectLogic(mode, {
      'fixed': 'Fixed BPM',
      'firstLoop': 'First loop sets BPM',
      'other': '$mode',
    });
    return '$_temp0';
  }

  @override
  String get trackMenuDoiTenScene => 'Rename scene';

  @override
  String get trackMenuDoiTenTrack => 'Rename track';

  @override
  String get trackMenuMustBeEmpty => ' (track must be empty)';

  @override
  String get trackMenuToAudio => 'Convert to audio track';

  @override
  String get trackMenuToInstrument => 'Convert to instrument track';

  @override
  String transportAction(String action) {
    String _temp0 = intl.Intl.selectLogic(action, {
      'play': 'Play',
      'stop': 'Stop',
      'toggle': 'Play/Stop',
      'other': '$action',
    });
    return '$_temp0';
  }

  @override
  String transportBpm(Object bpm) {
    return '$bpm BPM';
  }

  @override
  String get transportBpmTrong => '— BPM';

  @override
  String get transportChoVongDau => 'waiting for first loop';

  @override
  String transportCpu(Object cpu, Object xruns) {
    return 'CPU $cpu% · xrun $xruns';
  }

  @override
  String transportDaGhi(Object p0, Object p1) {
    return 'Saved $p0 ($p1)';
  }

  @override
  String transportDemVaoBar(int v) {
    String _temp0 = intl.Intl.pluralLogic(v, locale: localeName, other: '$v bars', one: '1 bar');
    return 'Count-in $_temp0';
  }

  @override
  String get transportGhiBuoiJamMaster => 'Record jam (master)';

  @override
  String get transportKhongDemVao => 'No count-in';

  @override
  String transportKhongGhiDuocJam(Object err) {
    return 'Couldn\'t record the jam: $err';
  }

  @override
  String get transportMetronomeCountInNhip => 'Metronome · count-in · time signature';

  @override
  String transportMetronomeItem(Object mode) {
    return 'Metronome: $mode';
  }

  @override
  String transportNhip(Object n, Object d) {
    return 'Time signature $n/$d';
  }

  @override
  String get transportQuantize => 'Quantize';

  @override
  String transportQuantizeChip(Object grid) {
    return 'Q: $grid';
  }

  @override
  String get transportTap => 'TAP';

  @override
  String get transportThem => 'More';

  @override
  String warpMode(String mode) {
    String _temp0 = intl.Intl.selectLogic(mode, {'stretch': 'Stretch', 'repitch': 'Re-Pitch', 'other': '$mode'});
    return '$_temp0';
  }
}
