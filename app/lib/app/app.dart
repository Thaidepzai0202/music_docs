import 'package:flutter/material.dart';

import '../features/spike/spike_screen.dart';
import 'theme.dart';

class LoopCoreApp extends StatelessWidget {
  const LoopCoreApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Music Looper',
      debugShowCheckedModeBanner: false,
      theme: buildAppTheme(),
      home: const SpikeScreen(),
    );
  }
}
