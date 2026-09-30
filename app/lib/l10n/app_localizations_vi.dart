// ignore: unused_import
import 'package:intl/intl.dart' as intl;
import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for Vietnamese (`vi`).
class AppLocalizationsVi extends AppLocalizations {
  AppLocalizationsVi([String locale = 'vi']) : super(locale);

  @override
  String get aboutApi => 'API';

  @override
  String get aboutBuffer => 'Buffer';

  @override
  String get aboutDungBangFlutterEngineC => 'Dựng bằng Flutter + engine C++ (JUCE). Chỉ cho iPad, dùng ngang.';

  @override
  String get aboutEngine => 'Engine';

  @override
  String get aboutEngineGiaLoopcoreFake => 'Engine giả (LOOPCORE_FAKE)';

  @override
  String aboutFrames(Object frames) {
    return '$frames frame';
  }

  @override
  String get aboutLoai => 'Loại';

  @override
  String get aboutLoopCore => 'LoopCore (JUCE)';

  @override
  String aboutPhienBan(Object p0, Object p1) {
    return 'Phiên bản $p0 ($p1)';
  }

  @override
  String get aboutSampleRate => 'Sample rate';

  @override
  String get aboutThietBi => 'Thiết bị';

  @override
  String get audioAudioChuaBat => 'Audio chưa bật';

  @override
  String audioBars(int count) {
    return '$count bar';
  }

  @override
  String get audioBuffer => 'Buffer audio';

  @override
  String get audioChoi => 'Chơi';

  @override
  String get audioDangBatThuDuoc => 'Đang bật — thu được';

  @override
  String audioDangChay(Object frames, Object ms, Object hz) {
    return 'Đang chạy: $frames frame ($ms) @ $hz Hz';
  }

  @override
  String audioKhongDoiDuocBuffer(Object err) {
    return 'Không đổi được buffer: $err';
  }

  @override
  String get audioLuonBat => 'Luôn bật';

  @override
  String get audioMic => 'Micro';

  @override
  String get audioMonitorMacDinhChoTrack => 'Monitor mặc định cho track audio mới';

  @override
  String audioNhoHonTreItHon(Object p0) {
    return 'Nhỏ hơn = trễ ít hơn nhưng tốn CPU hơn. $p0';
  }

  @override
  String get audioQuantizeNotKhiThuMidi => 'Quantize nốt khi thu MIDI';

  @override
  String get audioRungNheKhiLaunchThu => 'Rung nhẹ khi launch / thu';

  @override
  String get audioSoBarKhiThuVao => 'Độ dài thu';

  @override
  String get audioThuAm => 'Thu âm';

  @override
  String get audioTuDo => 'Tự do';

  @override
  String get audioTuDoGhiChu => 'Tự do: chạm ô lần nữa (hoặc nút LOOP) để chốt bản thu';

  @override
  String get audioTuDong => 'Tự động';

  @override
  String get audioTuDongNgheMicroKhi => 'Tự động: nghe micro khi track đang arm. Dùng tai nghe để tránh hú.';

  @override
  String get bannerAudioBiNgatCuocGoi => 'Audio bị ngắt (cuộc gọi, Siri…)';

  @override
  String bannerDaGiaiPhongBoNho(Object mb) {
    return 'Đã giải phóng bộ nhớ (còn dùng $mb MB)';
  }

  @override
  String get bannerDangDungTaiNgheBluetooth =>
      'Đang dùng tai nghe Bluetooth: độ trễ cao, không hợp để thu hoặc chơi nhịp.';

  @override
  String bannerKhongOverdubDuoc(Object track, Object slot) {
    return 'Không overdub được ô $track·$slot: clip audio đang Re-Pitch khác tempo hoặc chưa sẵn sàng. Clip vẫn phát.';
  }

  @override
  String bannerLoiEngine(Object p0) {
    return 'Lỗi engine: $p0';
  }

  @override
  String get browserBanThuCuaToi => 'Bản thu của tôi';

  @override
  String get browserChuaCoBanThu => 'Chưa có bản thu. Thu nhạc cụ mới ở tab Nhạc cụ.';

  @override
  String get browserChuaCoYeuThich => 'Chưa có mục yêu thích. Nhấn giữ một mục để thêm.';

  @override
  String browserDaBoYeuThich(String name) {
    return 'Đã bỏ khỏi Yêu thích: $name';
  }

  @override
  String browserDaThemVaoO(Object p0, Object p1) {
    return 'Đã thêm \"$p0\" vào ô $p1';
  }

  @override
  String browserDaThemYeuThich(String name) {
    return 'Đã thêm vào Yêu thích: $name';
  }

  @override
  String browserGan(Object trackName) {
    return 'gán → $trackName';
  }

  @override
  String get browserKeoVaoTrack => 'Kéo vào header track hoặc ô clip';

  @override
  String get browserKhongCoKetQua => 'Không có kết quả';

  @override
  String browserKhongDocDuocThuVien(Object e) {
    return 'Không đọc được thư viện: $e';
  }

  @override
  String browserLoopInfo(Object bpm, int beats) {
    return '$bpm BPM · $beats beat';
  }

  @override
  String browserODaCoClip(int cell) {
    return 'Ô $cell đã có clip';
  }

  @override
  String browserThemVao(Object trackName) {
    return 'thêm vào $trackName';
  }

  @override
  String get browserThuMucTrong => 'Thư mục trống';

  @override
  String get browserTimKiem => 'Tìm trong thư viện';

  @override
  String get browserTrackAudioCoClip => 'Track audio này đang có clip. Chọn track trống để gán nhạc cụ.';

  @override
  String get browserTrackNayKhongConO => 'Track này không còn ô trống';

  @override
  String get browserTrackNhacCuCoClip => 'Track nhạc cụ này đang có clip MIDI. Chọn track trống để thêm loop.';

  @override
  String get browserTuNgheThu => 'Tự nghe thử';

  @override
  String get browserYeuThich => 'Yêu thích';

  @override
  String get clipChoClipChay => 'Chờ clip chạy…';

  @override
  String get clipChonTatCa => 'Chọn tất cả';

  @override
  String get clipClear => 'Xoá hết';

  @override
  String get clipCuonHaiNgon => 'Cuộn bằng hai ngón';

  @override
  String get clipDangGhi => 'Đang ghi';

  @override
  String get clipDoDaiClip => 'Độ dài clip';

  @override
  String get clipDrumsHint => 'Loop trống nghe tự nhiên hơn khi Re-Pitch';

  @override
  String clipGain(Object db) {
    return 'Gain clip: $db dB';
  }

  @override
  String get clipGhi => 'Ghi';

  @override
  String get clipHoanTacOverdub => 'Hoàn tác overdub';

  @override
  String get clipHoanTacSua => 'Hoàn tác';

  @override
  String get clipKhongDuCho => 'Clip không đủ chỗ';

  @override
  String clipKhongDuChoToiDa(int bars) {
    return 'Không đủ chỗ: clip dài tối đa $bars bar.';
  }

  @override
  String get clipLamLai => 'Làm lại';

  @override
  String clipLoopTuSBeatGoc(Object p0, Object p1, Object p2) {
    return 'Loop từ $p0 s · $p1 beat · gốc $p2 BPM';
  }

  @override
  String get clipLuiVaXoa => 'Lùi và xoá';

  @override
  String get clipLuoi => 'Lưới';

  @override
  String get clipModeChon => 'Chọn';

  @override
  String get clipModeVe => 'Vẽ';

  @override
  String get clipNghi => 'Nghỉ';

  @override
  String get clipNhanBan => 'Nhân bản nốt đã chọn';

  @override
  String clipNotBeat(int p0, Object p1) {
    return '$p0 nốt · $p1 beat';
  }

  @override
  String get clipOTrongBanPhim => 'Ô trống. Mở bàn phím để chơi, ghi trực tiếp hoặc nhập từng nốt.';

  @override
  String get clipQuangTamLen => 'Lên một quãng tám';

  @override
  String get clipQuangTamXuong => 'Xuống một quãng tám';

  @override
  String get clipQuantize => 'Quantize';

  @override
  String clipQuantizeGrid(Object grid) {
    return 'Quantize $grid';
  }

  @override
  String get clipStep => 'Từng nốt';

  @override
  String get clipTangDoDai => 'Tăng độ dài';

  @override
  String clipTangDoDaiHoi(int bars) {
    return 'Tăng độ dài clip lên $bars bar để chứa bản chép?';
  }

  @override
  String get clipVelocity => 'Velocity';

  @override
  String get clipWarp => 'Warp';

  @override
  String clipXoaNotDaChon(Object p0) {
    return 'Xoá nốt đã chọn ($p0)';
  }

  @override
  String get clipZoom => 'Thu phóng';

  @override
  String get commonCancel => 'Huỷ';

  @override
  String get commonOk => 'OK';

  @override
  String demoName(String token) {
    String _temp0 = intl.Intl.selectLogic(token, {
      'demo': 'Demo',
      'drums': 'Trống',
      'bass': 'Bass',
      'keys': 'Đàn phím',
      'lead': 'Lead',
      'beatA': 'Beat A',
      'beatB': 'Beat B',
      'fill': 'Fill',
      'halfTime': 'Half-time',
      'bassA': 'Bass A',
      'bassB': 'Bass B',
      'walk': 'Walk',
      'chords': 'Hợp âm',
      'stab': 'Stab',
      'melody': 'Giai điệu',
      'hook': 'Hook',
      'other': '$token',
    });
    return '$_temp0';
  }

  @override
  String errorText(String code) {
    String _temp0 = intl.Intl.selectLogic(code, {
      'INVALID_ARG': 'Yêu cầu không hợp lệ',
      'NOT_CREATED': 'Engine chưa khởi động',
      'ALREADY_CREATED': 'Engine đã khởi động',
      'NOT_IMPLEMENTED': 'Engine chưa hỗ trợ',
      'AUDIO_DEVICE': 'Lỗi thiết bị âm thanh',
      'MIC_PERMISSION': 'Chưa có quyền micro',
      'FILE_NOT_FOUND': 'Không thấy file',
      'FILE_FORMAT': 'Định dạng file không hỗ trợ',
      'DISK_FULL': 'Bộ nhớ máy đã đầy',
      'FILE_WRITE': 'Không ghi được file',
      'OUT_OF_MEMORY': 'Hết bộ nhớ',
      'QUEUE_FULL': 'Engine đang bận, thử lại',
      'JOB_CANCELLED': 'Đã huỷ',
      'JOB_NOT_FOUND': 'Không thấy tác vụ',
      'PITCH_NOT_DETECTED': 'Không dò được cao độ',
      'OVERDUB_UNSUPPORTED': 'Clip này không overdub được',
      'CELL_EMPTY': 'Ô vẫn trống sau khi thu',
      'other': 'Lỗi engine ($code)',
    });
    return '$_temp0';
  }

  @override
  String get exportChamDeBatDauGhi => 'Chạm để bắt đầu ghi';

  @override
  String get exportChiaSe => 'Chia sẻ';

  @override
  String exportClippedSamples(int count) {
    return '$count mẫu bị clip: hãy giảm gain master';
  }

  @override
  String get exportDangGhi => 'Đang ghi  ';

  @override
  String get exportDinhDang => 'Định dạng';

  @override
  String get exportDungGhiJam => 'Dừng ghi jam';

  @override
  String exportExportLoi(Object p0) {
    return 'Export lỗi: $p0';
  }

  @override
  String get exportGhiBuoiJam => 'Ghi buổi jam';

  @override
  String get exportGhiToanBoDauRa =>
      'Ghi toàn bộ đầu ra master trong lúc bạn chơi (WAV, lưu ở Exports/). Có thể bật/tắt nhanh bằng nút ● ở thanh trên.';

  @override
  String get exportHuy => 'Huỷ';

  @override
  String exportKhongGhiDuoc(Object err) {
    return 'Không ghi được: $err';
  }

  @override
  String exportKhongMoDuocShareSheet(Object e) {
    return 'Không mở được share sheet: $e';
  }

  @override
  String get exportMenu => 'Export…';

  @override
  String get exportMoiTrackMotFile => 'Mỗi track một file';

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
  String get exportSceneNayChuaCoClip => 'Scene này chưa có clip';

  @override
  String get exportSceneTab => 'Export scene';

  @override
  String get exportSoBar => 'Số bar';

  @override
  String exportStemCount(int count) {
    return ' + $count stem';
  }

  @override
  String get exportStemsLabel => 'Stems';

  @override
  String get exportTitle => 'Export';

  @override
  String get fxBat => 'Bật';

  @override
  String get fxBatLai => 'Bật lại';

  @override
  String get fxBypass => 'Bypass';

  @override
  String get fxDoiLoaiFx => 'Đổi loại FX';

  @override
  String get fxKeoDocDeChinhCham => 'Kéo dọc để chỉnh · chạm đúp về mặc định';

  @override
  String get fxMasterCard => 'Master · EQ3 · Limiter';

  @override
  String fxParam(String name) {
    String _temp0 = intl.Intl.selectLogic(name, {
      'mode': 'Kiểu',
      'cutoff': 'Cutoff',
      'reso': 'Reso',
      'rate': 'Nhịp',
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
    return 'Slot $p0 trống';
  }

  @override
  String get fxTat => 'Tắt';

  @override
  String get fxThemFx => 'Thêm FX';

  @override
  String get fxToggleOff => 'TẮT';

  @override
  String get fxToggleOn => 'BẬT';

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
  String get fxXoaFx => 'Xoá FX';

  @override
  String get instrumentBanPhim => 'Bàn phím';

  @override
  String get instrumentKhongTimThayNhacCu => 'Không tìm thấy nhạc cụ';

  @override
  String instrumentMode(String mode) {
    String _temp0 = intl.Intl.selectLogic(mode, {'natural': 'Natural', 'classic': 'Classic', 'other': '$mode'});
    return '$_temp0';
  }

  @override
  String get instrumentPad => 'Pad';

  @override
  String get instrumentThuAmMoi => 'Thu âm mới';

  @override
  String jamDroppedMs(Object ms) {
    return 'Mất $ms ms: bộ nhớ ghi không kịp';
  }

  @override
  String get latencyAppPhatTiengClickRa =>
      'App phát tiếng click ra loa rồi nghe lại qua micro để đo thời gian đi-về. Để iPad ở chỗ yên tĩnh, âm lượng vừa phải — hoặc cắm cáp loopback từ đầu ra vào đầu vào. Mất khoảng 5 giây. Đo xong thì mức bù bên dưới được đặt theo kết quả.';

  @override
  String get latencyBanThuNgheBiTre =>
      'Bản thu nghe bị trễ so với nhịp thì tăng, bị sớm thì giảm. Lưu lại và áp dụng mỗi lần mở app.';

  @override
  String get latencyBuThem => 'Bù thêm';

  @override
  String get latencyCanQuyenMicroDeDo => 'Cần quyền micro để đo';

  @override
  String get latencyChinhTay => 'Chỉnh tay';

  @override
  String get latencyChuaCo => 'chưa có';

  @override
  String latencyDaLuuEngineChuaAp(Object err) {
    return 'Đã lưu, engine chưa áp dụng được: $err';
  }

  @override
  String latencyDeviceBaoBuSampleLech(Object p0, Object p1, Object p2, Object p3) {
    return 'Device báo $p0 · bù $p1 sample · lệch giữa các lần $p2 · tin cậy $p3%';
  }

  @override
  String get latencyDoDoTre => 'Đo độ trễ';

  @override
  String latencyDoLoi(Object p0) {
    return 'Đo lỗi: $p0';
  }

  @override
  String get latencyDoTreVongThuPhat => 'Độ trễ vòng (thu ↔ phát)';

  @override
  String get latencyDoTuDong => 'Đo tự động';

  @override
  String get latencyEngineDangDung => 'Engine đang dùng';

  @override
  String latencyFailReason(String reason) {
    String _temp0 = intl.Intl.selectLogic(reason, {
      'NO_SIGNAL': 'Không nghe thấy tiếng click đo — tăng âm lượng hoặc cắm cáp loopback',
      'TOO_NOISY': 'Xung quanh ồn quá — thử ở chỗ yên tĩnh hơn',
      'INCONSISTENT': 'Các lần đo lệch nhau — để iPad yên rồi đo lại',
      'DEVICE_CHANGED': 'Thiết bị âm thanh đổi trong lúc đo — đo lại',
      'TIMEOUT': 'Đo quá lâu — thử lại',
      'other': '$reason',
    });
    return '$_temp0';
  }

  @override
  String get latencyKhongBatDuocAudio => 'Không bật được audio';

  @override
  String get latencyLanDoGanNhat => 'Lần đo gần nhất';

  @override
  String latencyOffsetValue(Object ms, int samples) {
    return '$ms ms ($samples sample)';
  }

  @override
  String latencySamplesMs(int samples, Object ms) {
    return '$samples sample · $ms ms';
  }

  @override
  String get latencyVe0 => 'Về 0';

  @override
  String learnKind(String kind) {
    String _temp0 = intl.Intl.selectLogic(kind, {
      'clip': 'Clip',
      'scene': 'Scene',
      'transport': 'Transport',
      'stopAll': 'Dừng tất cả',
      'trackGain': 'Gain track',
      'trackMute': 'Mute track',
      'fx': 'Tham số FX',
      'loopButton': 'Nút LOOP',
      'trackStop': 'Dừng track',
      'undoOverdub': 'Hoàn tác overdub',
      'other': '$kind',
    });
    return '$_temp0';
  }

  @override
  String libraryFolder(String name) {
    String _temp0 = intl.Intl.selectLogic(name, {
      'drums': 'Trống',
      'instruments': 'Nhạc cụ',
      'keys': 'Phím',
      'strings': 'Dây',
      'windsBrass': 'Kèn & sáo',
      'synth': 'Synth',
      'loops': 'Loop',
      'other': '$name',
    });
    return '$_temp0';
  }

  @override
  String libraryTag(String tag) {
    String _temp0 = intl.Intl.selectLogic(tag, {
      'drums': 'Trống',
      'bass': 'Bass',
      'keys': 'Phím',
      'synth': 'Synth',
      'pad': 'Pad',
      'click': 'Click',
      'tone': 'Âm đơn',
      'test': 'Thử',
      'hiphop': 'Hip-hop',
      'funk': 'Funk',
      'melody': 'Giai điệu',
      'vocal': 'Giọng',
      'fx': 'FX',
      'strings': 'Bộ dây',
      'winds': 'Bộ gỗ',
      'brass': 'Bộ đồng',
      'percussion': 'Bộ gõ',
      'other': '$tag',
    });
    return '$_temp0';
  }

  @override
  String get licensesChuaCoFileGiayPhep => 'Chưa có file giấy phép nội dung.';

  @override
  String get licensesChuaTichHopP406 => 'Chưa tích hợp (P4-06)';

  @override
  String get licensesGhiCong => 'Ghi công';

  @override
  String get licensesGiayPhepCacGoiFlutter => 'Giấy phép các gói Flutter/Dart';

  @override
  String get licensesGoiFlutterDart => 'Gói Flutter / Dart';

  @override
  String get licensesJuce9DungTheoGoi => 'JUCE 9 — dùng theo gói Starter (EULA JUCE)';

  @override
  String licensesKhongDocDuoc(Object asset) {
    return 'Không đọc được $asset';
  }

  @override
  String get licensesNoiDungAmThanh => 'Nội dung âm thanh';

  @override
  String get licensesThuVienBenThuBa => 'Thư viện bên thứ ba';

  @override
  String get linkBamOMotMayThi => 'Bấm ▶/■ ở một máy thì các máy khác cùng chạy/dừng.';

  @override
  String get linkBatLink2 => 'Bật Link';

  @override
  String get linkCanQuyenMangCucBo => 'Cần quyền mạng cục bộ (iOS hỏi lần đầu bật).';

  @override
  String linkDangNoiThietBi(int peers) {
    return 'Đang nối: $peers thiết bị';
  }

  @override
  String get linkDongBoStartStop => 'Đồng bộ Start/Stop';

  @override
  String get linkDongBoTempoVaNhip =>
      'Đồng bộ tempo và nhịp với app/thiết bị khác cùng mạng Wi-Fi. Lưu theo từng project.';

  @override
  String get linkEngineChuaCoLinkP4 => 'Engine chưa có Link (P4-06) — đã lưu lựa chọn vào project';

  @override
  String get linkTitle => 'Ableton Link';

  @override
  String loopButtonLabel(String state) {
    String _temp0 = intl.Intl.selectLogic(state, {
      'idle': 'LOOP',
      'rec': 'THU',
      'play': 'PHÁT',
      'dub': 'CHỒNG',
      'other': 'LOOP',
    });
    return '$_temp0';
  }

  @override
  String get loopButtonTooltip => 'LOOP: chạm = thu → phát → chồng tiếng · chạm đúp = dừng · giữ = hoàn tác';

  @override
  String get loopDungTrack => 'Dừng track';

  @override
  String get loopHoanTac => 'Hoàn tác overdub / xoá clip';

  @override
  String get loopXoa => 'Xoá';

  @override
  String get loopXoaClipBody => 'Không có lớp overdub để hoàn tác.';

  @override
  String loopXoaClipTitle(Object name) {
    return 'Xoá clip \"$name\"?';
  }

  @override
  String menuClipMidiTrong(int bars) {
    return 'Clip MIDI trống · $bars bar';
  }

  @override
  String get menuCopy => 'Copy';

  @override
  String get menuCopyToiDay => 'Copy tới đây';

  @override
  String menuDan(Object p0) {
    return 'Dán \"$p0\"';
  }

  @override
  String get menuDanChuaCopyClipNao => 'Dán (chưa copy clip nào)';

  @override
  String get menuDiChuyenToiDay => 'Di chuyển tới đây';

  @override
  String get menuDoiTenClip => 'Đổi tên clip';

  @override
  String get menuKhongConOTrongBen => 'Không còn ô trống bên dưới';

  @override
  String get menuMauTrack => 'Màu track';

  @override
  String get menuODichDaCoClip => 'Ô đích đã có clip';

  @override
  String metronomeMode(String mode) {
    String _temp0 = intl.Intl.selectLogic(mode, {
      'off': 'Tắt',
      'always': 'Bật',
      'recordOnly': 'Khi thu',
      'other': '$mode',
    });
    return '$_temp0';
  }

  @override
  String get midiAnyChannel => 'mọi kênh';

  @override
  String get midiAnyDevice => 'mọi thiết bị';

  @override
  String get midiBatDauLearn => 'Bắt đầu learn';

  @override
  String get midiBoGan => 'Bỏ gán';

  @override
  String midiChannel(Object number) {
    return 'kênh $number';
  }

  @override
  String get midiChuaGanGi => 'Chưa gán gì.';

  @override
  String get midiChuaThayThietBiMidi => 'Chưa thấy thiết bị MIDI. Cắm qua USB hoặc ghép Bluetooth MIDI.';

  @override
  String midiClipO(Object p0, Object p1) {
    return 'Clip · $p0 · ô $p1';
  }

  @override
  String get midiDenLedPhanHoiTrang => 'Đèn LED phản hồi trạng thái clip (Launchpad) có ở P4-05.';

  @override
  String get midiDungMoiClipTheoQuantize => 'Dừng mọi clip theo quantize.';

  @override
  String get midiDungTatCa => 'Dừng tất cả';

  @override
  String midiEnableFailed(Object error) {
    return 'Không đổi được thiết bị: $error';
  }

  @override
  String get midiEngineChuaHoTroMidi => 'Engine chưa hỗ trợ MIDI (P4-01)';

  @override
  String midiFootswitchGhiChu(String kind) {
    String _temp0 = intl.Intl.selectLogic(kind, {
      'loopButton': 'Footswitch làm như nút LOOP trên màn hình, trên track đang chọn: thu → phát → chồng tiếng.',
      'trackStop': 'Footswitch dừng track đang chọn (theo quantize).',
      'undoOverdub': 'Footswitch hoàn tác lớp overdub gần nhất của clip đang phát trên track đang chọn.',
      'other': '$kind',
    });
    return '$_temp0';
  }

  @override
  String midiFxSlotItem(Object number, Object type) {
    return 'Slot $number · $type';
  }

  @override
  String get midiGanDieuKhienMidi2 => 'Gán điều khiển MIDI';

  @override
  String get midiGanMoi => 'Gán mới';

  @override
  String get midiGanMotNutNumTren => 'Gán một nút/núm trên controller cho clip hoặc tham số FX. Lưu trong project.';

  @override
  String get midiGhepThietBiBluetoothMidi => 'Ghép thiết bị Bluetooth MIDI';

  @override
  String get midiLamMoi => 'Làm mới';

  @override
  String midiLearnFailed(Object error) {
    return 'Không bắt đầu learn được: $error';
  }

  @override
  String get midiMidiLearnProjectDangMo => 'MIDI learn (project đang mở)';

  @override
  String midiO(Object p0) {
    return 'Ô $p0';
  }

  @override
  String midiParamFallback(Object id) {
    return 'tham số $id';
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
    return 'Nốt $number';
  }

  @override
  String get midiThietBiRa => 'Thiết bị ra';

  @override
  String get midiThietBiVao => 'Thiết bị vào';

  @override
  String get midiTrackNayChuaCoFx => 'Track này chưa có FX.';

  @override
  String get midiVanHoacBamMotNut => 'Vặn hoặc bấm một nút trên controller…';

  @override
  String get mixerMaster => 'Master';

  @override
  String get mixerMonitor => 'Monitor';

  @override
  String get mixerMonitorLuonBat => 'Monitor: Luôn bật';

  @override
  String get mixerMonitorTat => 'Monitor: Tắt';

  @override
  String get mixerMonitorTuDongKhiArm => 'Monitor: Tự động (khi arm)';

  @override
  String monitorBadge(String mode) {
    String _temp0 = intl.Intl.selectLogic(mode, {'off': 'Tắt', 'auto': 'Auto', 'always': 'Bật', 'other': '$mode'});
    return '$_temp0';
  }

  @override
  String get onboardingAmThanhChiLuuTren => 'Âm thanh chỉ lưu trên iPad của bạn, không gửi đi đâu.';

  @override
  String get onboardingBienMotTiengBanThu => 'Biến một tiếng bạn thu thành nhạc cụ chơi được trên bàn phím.';

  @override
  String get onboardingBoQua => 'Bỏ qua';

  @override
  String get onboardingChamMotODePhat => 'Chạm một ô để phát clip — clip vào đúng đầu ô nhịp kế tiếp.';

  @override
  String get onboardingChamSoSceneBenPhai => 'Chạm số scene bên phải để đổi cả hàng cùng lúc.';

  @override
  String get onboardingChoPhepMicro => 'Cho phép micro';

  @override
  String get onboardingChuaCoQuyenMicroThu => 'Chưa có quyền micro: thu âm sẽ hỏi lại khi bạn cần.';

  @override
  String get onboardingDeSau => 'Để sau';

  @override
  String get onboardingDoDoTreDeBan => 'Đo độ trễ để bản thu nằm đúng phách.';

  @override
  String onboardingKhongTaoDuocProjectDemo(Object e) {
    return 'Không tạo được project demo: $e';
  }

  @override
  String get onboardingMicroDangBiTatBan =>
      'Micro đang bị tắt. Bạn vẫn chơi loop và nhạc cụ có sẵn được; muốn thu âm thì bật lại trong Cài đặt.';

  @override
  String get onboardingMoCaiDat => 'Mở Cài đặt';

  @override
  String get onboardingMoProjectDemo => 'Mở project demo';

  @override
  String get onboardingMusicLooperCanMicro => 'Music Looper cần micro';

  @override
  String get onboardingTabInstrumentPhiaDuoiDe => 'Tab Instrument phía dưới để chơi pad và bàn phím.';

  @override
  String get onboardingThuGiongHatGuitarThanh => 'Thu giọng hát, guitar… thành clip lặp khớp nhịp.';

  @override
  String get onboardingThuNgayVoiProjectDemo => 'Thử ngay với project demo';

  @override
  String get onboardingTiepTuc => 'Tiếp tục';

  @override
  String get onboardingTuTaoProject => 'Tự tạo project';

  @override
  String panelTab(String tab) {
    String _temp0 = intl.Intl.selectLogic(tab, {
      'clip': 'Clip',
      'instrument': 'Nhạc cụ',
      'mixer': 'Mixer',
      'fx': 'FX',
      'browser': 'Thư viện',
      'other': '$tab',
    });
    return '$_temp0';
  }

  @override
  String projectsBanSao(Object name) {
    return '$name (bản sao)';
  }

  @override
  String get projectsCaiDat => 'Cài đặt';

  @override
  String get projectsChuaCoProjectBamProject => 'Chưa có project. Bấm \"Project mới\" hoặc \"Tạo project demo\".';

  @override
  String get projectsDoiTen => 'Đổi tên';

  @override
  String get projectsKhongDocDuocProject => 'Không đọc được project';

  @override
  String projectsLoi(Object e) {
    return 'Lỗi: $e';
  }

  @override
  String get projectsNhanBan => 'Nhân bản';

  @override
  String get projectsProjectJsonBiHongDa => 'project.json bị hỏng — đã mở bản lưu trước (.bak)';

  @override
  String get projectsProjectMoi => 'Project mới';

  @override
  String projectsSuaLanCuoi(Object p0) {
    return 'Sửa lần cuối: $p0';
  }

  @override
  String get projectsTaoProjectDemo => 'Tạo project demo';

  @override
  String get projectsTitle => 'Project';

  @override
  String projectsXoa(Object p0) {
    return 'Xoá \"$p0\"?';
  }

  @override
  String get projectsXoa2 => 'Xoá';

  @override
  String get projectsXoaCaBanThuAm => 'Xoá cả bản thu âm trong project. Không hoàn tác được.';

  @override
  String quantizeGrid(String grid) {
    String _temp0 = intl.Intl.selectLogic(grid, {
      'none': 'Không',
      'sixteenth': '1/16',
      'eighth': '1/8',
      'quarter': '1/4',
      'half': '1/2',
      'bar1': '1 bar',
      'bar2': '2 bar',
      'bar4': '4 bar',
      'other': '$grid',
    });
    return '$_temp0';
  }

  @override
  String recordQuantize(String grid) {
    String _temp0 = intl.Intl.selectLogic(grid, {'off': 'Tắt', 'sixteenth': '1/16', 'eighth': '1/8', 'other': '$grid'});
    return '$_temp0';
  }

  @override
  String get samplerCanQuyenMicroDeThu => 'Cần quyền micro để thu (Cài đặt → Music Looper → Micro)';

  @override
  String samplerChamDeThuMotNot(Object p0) {
    return 'Chạm để thu một nốt (tối đa $p0 giây)';
  }

  @override
  String get samplerDangCatKhoangLangVa => 'Đang cắt khoảng lặng và dò nốt…';

  @override
  String samplerDangTaoNhacCu13(Object p0) {
    return 'Đang tạo nhạc cụ (13 zone)… $p0%';
  }

  @override
  String samplerDoTinCay(Object p0) {
    return 'độ tin cậy $p0%';
  }

  @override
  String samplerHatHoacThoiMotNot(Object p0) {
    return 'Hát hoặc thổi một nốt đều… tự dừng sau $p0 giây';
  }

  @override
  String samplerKhongBatDuocAudio(Object p0) {
    return 'Không bật được audio: $p0';
  }

  @override
  String get samplerKhongChacVeCaoDo => 'Không chắc về cao độ — hãy chọn nốt gốc bằng − / + trước khi tạo';

  @override
  String get samplerKhongDoDuocCaoDo => 'Không dò được cao độ — hãy chọn nốt gốc rồi tạo lại';

  @override
  String get samplerKhongNgheThayTieng => 'Không nghe thấy tiếng — kiểm tra micro rồi thu lại';

  @override
  String get samplerNotGoc => 'Nốt gốc: ';

  @override
  String samplerPhanTichLoi(Object p0) {
    return 'Phân tích lỗi: $p0';
  }

  @override
  String get samplerTaoNhacCu => 'Tạo nhạc cụ';

  @override
  String samplerTaoNhacCuLoi(Object p0) {
    return 'Tạo nhạc cụ lỗi: $p0';
  }

  @override
  String get samplerThuAmNhacCu => 'Thu âm → nhạc cụ';

  @override
  String get samplerThuLai => 'Thu lại';

  @override
  String samplerTiengThu(Object p0) {
    return 'Tiếng thu $p0';
  }

  @override
  String samplerTrim(Object start, Object end) {
    return 'Cắt $start → $end s';
  }

  @override
  String sessionBanThu(Object p0) {
    return 'Bản thu $p0';
  }

  @override
  String get sessionBatEditRoiChamMot => 'Bật ✎ Edit rồi chạm một ô để chọn clip';

  @override
  String get sessionChoPhepMicro => 'Cho phép micro';

  @override
  String get sessionChuaCoQuyenMicroChi => 'Chưa có quyền micro: chỉ phát được, chưa thu được';

  @override
  String get sessionChuaMoProject => 'Chưa mở project';

  @override
  String sessionDangMoProject(Object p0, Object p1) {
    return 'Đang mở project… $p0/$p1';
  }

  @override
  String sessionKhongLuuDuocProject(Object reason) {
    return 'Không lưu được project: $reason';
  }

  @override
  String sessionLoiKhiMoProject(int p0, Object p1) {
    return '$p0 lỗi khi mở project: $p1';
  }

  @override
  String sessionMidiClipName(Object number) {
    return 'MIDI $number';
  }

  @override
  String get sessionMoPanel => 'Mở panel';

  @override
  String get sessionMoRongPanel => 'Mở rộng panel';

  @override
  String sessionMode(String mode) {
    String _temp0 = intl.Intl.selectLogic(mode, {'perform': 'Perform', 'edit': 'Edit', 'other': '$mode'});
    return '$_temp0';
  }

  @override
  String sessionOTrong(Object p0, Object p1) {
    return 'Ô $p0·$p1 trống';
  }

  @override
  String sessionSceneName(Object number) {
    return 'Scene $number';
  }

  @override
  String get sessionThuGon => 'Thu gọn';

  @override
  String get sessionThuNhoPanel => 'Thu nhỏ panel';

  @override
  String get sessionTitle => 'Session';

  @override
  String sessionTrackName(Object number) {
    return 'Track $number';
  }

  @override
  String get settingsNeedsProjectLink => 'Mở một project để bật Link.';

  @override
  String get settingsNeedsProjectMidi => 'Mở một project để gán điều khiển MIDI.';

  @override
  String settingsSection(String section) {
    String _temp0 = intl.Intl.selectLogic(section, {
      'audio': 'Audio',
      'latency': 'Độ trễ',
      'midi': 'MIDI',
      'link': 'Link',
      'licenses': 'Giấy phép',
      'about': 'Giới thiệu',
      'other': '$section',
    });
    return '$_temp0';
  }

  @override
  String get spike1MsDoLai => '✗ (> 1 ms, đo lại)';

  @override
  String spikeApiversion(Object p0) {
    return 'apiVersion = $p0';
  }

  @override
  String get spikeAppCanMicroDeThu =>
      'App cần micro để thu âm và đo latency.\nMở Cài đặt → Music Looper → bật Micro, rồi quay lại bấm Start.';

  @override
  String get spikeAudioBiNgatSiriCuoc => 'Audio bị ngắt (Siri/cuộc gọi…)';

  @override
  String get spikeBat => 'bật';

  @override
  String get spikeBlock20050Ms => 'Block 200 / 50 ms';

  @override
  String get spikeCamTaiNgheTruoc => 'Cắm tai nghe trước';

  @override
  String get spikeCanQuyenMicro => 'Cần quyền micro';

  @override
  String spikeCanhBaoBoNhoMb(Object p0) {
    return 'Cảnh báo bộ nhớ: $p0 MB';
  }

  @override
  String get spikeCheaper => 'Cheaper';

  @override
  String get spikeCo => 'có';

  @override
  String get spikeCoLatencyCao => 'có ⚠ latency cao';

  @override
  String spikeCpuDinh(Object cpuAvg, Object cpuPeak) {
    return 'CPU  $cpuAvg% · đỉnh $cpuPeak%';
  }

  @override
  String get spikeDaCamBat => 'Đã cắm, bật';

  @override
  String get spikeDangChay => 'Đang chạy…';

  @override
  String get spikeDangDo => 'Đang đo…';

  @override
  String get spikeDangThu4Giay => 'Đang thu 4 giây…';

  @override
  String get spikeDeSau => 'Để sau';

  @override
  String spikeDeviceInKenh(Object p0, Object p1) {
    return 'device: $p0 · in $p1 kênh';
  }

  @override
  String get spikeDo => 'Đo';

  @override
  String get spikeDoLatency => 'Đo latency';

  @override
  String get spikeDoLatencyBoTaiNghe =>
      'Đo latency: bỏ tai nghe, loa ngoài, phòng yên tĩnh (~3 giây, sine/loop tạm tắt). Stretch bench dùng bản vừa thu 4 giây.';

  @override
  String get spikeEventRouteNgatXrun => 'Event (route, ngắt, xrun)';

  @override
  String get spikeFakeengineChuaCoLoopcoreXcframework => 'FakeEngine (chưa có LoopCore.xcframework)';

  @override
  String get spikeFormant => 'Formant';

  @override
  String get spikeGain => 'Gain';

  @override
  String get spikeHetNgatAudio => 'Hết ngắt audio';

  @override
  String get spikeHuy => 'Huỷ';

  @override
  String spikeJobDangChay(Object op, Object jobId) {
    return '$op: job $jobId đang chạy…';
  }

  @override
  String spikeJobDangChay2(Object op, Object jobId, Object p2) {
    return '$op: job $jobId đang chạy… $p2%';
  }

  @override
  String spikeJobThatBai(Object op, Object jobId, Object p2, Object p3) {
    return '$op (job $jobId) thất bại: $p2 $p3';
  }

  @override
  String get spikeKetQua => 'Kết quả';

  @override
  String get spikeKhong => 'không';

  @override
  String spikeKhongDoDuocMicKhong(Object p0) {
    return 'Không đo được: mic không nghe rõ chirp (inputPeak = $p0).\nTăng âm lượng loa, bỏ tai nghe, đo trong phòng yên tĩnh rồi thử lại.';
  }

  @override
  String spikeLeAudioStartLoi(Object p0) {
    return 'le_audio_start lỗi: $p0';
  }

  @override
  String spikeLeCreateLoi(Object p0) {
    return 'le_create lỗi: $p0';
  }

  @override
  String get spikeLoi => 'lỗi';

  @override
  String spikeLoiEngine(Object p0) {
    return 'Lỗi engine: $p0';
  }

  @override
  String get spikeLoopcoreSpike => 'LoopCore spike';

  @override
  String get spikeMoCaiDat => 'Mở Cài đặt';

  @override
  String get spikeModeDefault => 'Mode default';

  @override
  String get spikeModeMeasurement => 'Mode measurement';

  @override
  String get spikeNaudioChuaChayBamStart => '\nAudio chưa chạy → bấm Start audio trước.';

  @override
  String get spikePassthroughCanTaiNghe => 'Passthrough (cần tai nghe)';

  @override
  String get spikePassthroughDuaMicRaLoa => 'Passthrough đưa mic ra loa. Không cắm tai nghe sẽ bị hú (feedback).';

  @override
  String get spikePhatLoop => 'Phát loop';

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
    return 'Round-trip đo được: $p0 ms ($p1 smp)\niOS báo: $p2 ms ($p3 smp)\nLệch giữa các lần: $p4 ms $p5 · hợp lệ $p6/$runs';
  }

  @override
  String spikeRouteDoiTaiNgheDay(Object p0, Object p1) {
    return 'Route đổi: tai nghe dây/interface=$p0, Bluetooth=$p1';
  }

  @override
  String get spikeSessionInfo => 'Session info';

  @override
  String spikeSineHzGain(Object p0, Object p1) {
    return 'Sine $p0 Hz · gain $p1';
  }

  @override
  String spikeSpikeSetbuffersizeLoi(Object p0) {
    return 'spike.setBufferSize lỗi: $p0';
  }

  @override
  String spikeSpikeSetsessionmodeLoi(Object p0) {
    return 'spike.setSessionMode lỗi: $p0';
  }

  @override
  String get spikeStartAudio => 'Start audio';

  @override
  String get spikeStopAudio => 'Stop audio';

  @override
  String get spikeStretchBench => 'Đo stretch';

  @override
  String spikeTaiGiaLapVoice(Object voices) {
    return 'Tải giả lập: $voices voice';
  }

  @override
  String get spikeTanSo => 'Tần số';

  @override
  String get spikeTat => 'tắt';

  @override
  String get spikeThu4Giay => 'Thu 4 giây';

  @override
  String spikeThuXong(Object frames, Object seconds) {
    return 'Thu xong: $frames frame ($seconds s)';
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
    return 'Tổng $p0 zone: $p1 ms $p2 · formant $p3\nSetup $p4 ms · ghi file $p5 ms\nms/zone  $zones\n$p7 file WAV: app Files → Trên iPad → Music Looper → spike';
  }

  @override
  String spikeXrunTong(Object totalXruns) {
    return 'Xrun (tổng $totalXruns)';
  }

  @override
  String tempoMode(String mode) {
    String _temp0 = intl.Intl.selectLogic(mode, {
      'fixed': 'BPM cố định',
      'firstLoop': 'Vòng đầu quyết định BPM',
      'other': '$mode',
    });
    return '$_temp0';
  }

  @override
  String get trackMenuDoiTenScene => 'Đổi tên scene';

  @override
  String get trackMenuDoiTenTrack => 'Đổi tên track';

  @override
  String get trackMenuMustBeEmpty => ' (track phải trống)';

  @override
  String get trackMenuToAudio => 'Chuyển thành track audio';

  @override
  String get trackMenuToInstrument => 'Chuyển thành track nhạc cụ';

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
  String get transportChoVongDau => 'chờ vòng đầu';

  @override
  String transportCpu(Object cpu, Object xruns) {
    return 'CPU $cpu% · xrun $xruns';
  }

  @override
  String transportDaGhi(Object p0, Object p1) {
    return 'Đã ghi $p0 ($p1)';
  }

  @override
  String transportDemVaoBar(int v) {
    return 'Đếm vào $v bar';
  }

  @override
  String get transportGhiBuoiJamMaster => 'Ghi buổi jam (master)';

  @override
  String get transportKhongDemVao => 'Không đếm vào';

  @override
  String transportKhongGhiDuocJam(Object err) {
    return 'Không ghi được jam: $err';
  }

  @override
  String get transportMetronomeCountInNhip => 'Metronome · count-in · nhịp';

  @override
  String transportMetronomeItem(Object mode) {
    return 'Metronome: $mode';
  }

  @override
  String transportNhip(Object n, Object d) {
    return 'Nhịp $n/$d';
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
  String get transportThem => 'Thêm';

  @override
  String warpMode(String mode) {
    String _temp0 = intl.Intl.selectLogic(mode, {'stretch': 'Stretch', 'repitch': 'Re-Pitch', 'other': '$mode'});
    return '$_temp0';
  }
}
