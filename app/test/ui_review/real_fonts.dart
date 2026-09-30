import 'dart:io';

import 'package:flutter/services.dart';

/// Test đo chữ / golden cần font thật (font mặc định của test là ô vuông, rộng hơn chữ thật): Roboto + MaterialIcons
/// từ bộ cache của Flutter. Chỉ có trên Mac có FLUTTER_ROOT.
bool get realFontsAvailable => Platform.isMacOS && Platform.environment['FLUTTER_ROOT'] != null;

Future<void> loadRealFonts() async {
  final fonts = '${Platform.environment['FLUTTER_ROOT']}/bin/cache/artifacts/material_fonts';
  Future<ByteData> file(String name) async => ByteData.sublistView(File('$fonts/$name').readAsBytesSync());
  final roboto = FontLoader('Roboto');
  for (final w in ['Regular', 'Medium', 'Bold']) {
    roboto.addFont(file('Roboto-$w.ttf'));
  }
  await roboto.load();
  await (FontLoader('MaterialIcons')..addFont(file('MaterialIcons-Regular.otf'))).load();
}
