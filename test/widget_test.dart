import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:thumb_forge/main.dart';

void main() {
  testWidgets('ThumbForge desktop app smoke test', (WidgetTester tester) async {
    tester.view.physicalSize = const Size(1320, 840);
    tester.view.devicePixelRatio = 1.0;
    addTearDown(() => tester.view.resetPhysicalSize());

    await tester.pumpWidget(const ThumbForgeApp());
    await tester.pump();

    expect(find.text('THUMBFORGE'), findsOneWidget);
    expect(find.text('PDF THUMBNAILS'), findsOneWidget);
    expect(find.text('VIDEO THUMBNAILS'), findsOneWidget);
    expect(find.text('APPLY THUMBNAILS'), findsOneWidget);
    expect(find.text('RESET DEFAULTS'), findsOneWidget);
  });
}
