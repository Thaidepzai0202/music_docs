import 'package:flutter/material.dart';

import '../../app/router.dart';

/// Khung màn Projects. Danh sách, tạo, đổi tên, nhân bản, xoá làm ở P2-23.
class ProjectsScreen extends StatelessWidget {
  const ProjectsScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Projects')),
      body: Center(
        child: FilledButton(
          key: const Key('projects.openSession'),
          onPressed: () => Navigator.pushNamed(context, AppRoutes.session),
          child: const Text('Mở Session'),
        ),
      ),
    );
  }
}
