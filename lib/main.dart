import 'dart:io';
import 'package:flutter/material.dart';
import 'package:window_manager/window_manager.dart';
import 'theme/kinetic_theme.dart';
import 'screens/home_screen.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();

  if (!Platform.environment.containsKey('FLUTTER_TEST')) {
    await windowManager.ensureInitialized();

    const windowOptions = WindowOptions(
      size: Size(1320, 840),
      minimumSize: Size(1020, 680),
      center: true,
      backgroundColor: Colors.transparent,
      skipTaskbar: false,
      titleBarStyle: TitleBarStyle.hidden,
    );

    await windowManager.waitUntilReadyToShow(windowOptions, () async {
      await windowManager.show();
      await windowManager.focus();
    });
  }

  runApp(const ThumbForgeApp());
}

class ThumbForgeApp extends StatelessWidget {
  const ThumbForgeApp({super.key});

  @override
  Widget build(BuildContext context) {
    return ValueListenableBuilder<ThemeMode>(
      valueListenable: appThemeModeNotifier,
      builder: (context, currentMode, _) {
        return MaterialApp(
          title: 'THUMBFORGE // WINDOWS SHELL THUMBNAIL MANAGER',
          debugShowCheckedModeBanner: false,
          theme: KineticTheme.lightTheme,
          darkTheme: KineticTheme.darkTheme,
          themeMode: currentMode,
          home: const HomeScreen(),
        );
      },
    );
  }
}
