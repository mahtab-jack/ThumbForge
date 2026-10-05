import 'dart:io';
import 'package:flutter/material.dart';
import 'package:window_manager/window_manager.dart';
import '../theme/kinetic_theme.dart';

class CustomTitleBar extends StatefulWidget {
  const CustomTitleBar({super.key});

  @override
  State<CustomTitleBar> createState() => _CustomTitleBarState();
}

class _CustomTitleBarState extends State<CustomTitleBar> {
  bool _closeHovered = false;
  bool _minimizeHovered = false;
  bool _maximizeHovered = false;
  bool _themeHovered = false;
  bool _githubHovered = false;

  void _handleMinimize() {
    if (!Platform.environment.containsKey('FLUTTER_TEST')) {
      windowManager.minimize();
    }
  }

  void _handleMaximize() async {
    if (!Platform.environment.containsKey('FLUTTER_TEST')) {
      final isMax = await windowManager.isMaximized();
      if (isMax) {
        windowManager.unmaximize();
      } else {
        windowManager.maximize();
      }
    }
  }

  void _handleClose() {
    if (!Platform.environment.containsKey('FLUTTER_TEST')) {
      windowManager.close();
    }
  }

  @override
  Widget build(BuildContext context) {
    final palette = KineticPalette.of(context);

    return Container(
      height: 42,
      decoration: BoxDecoration(
        color: palette.muted,
        border: Border(
          bottom: BorderSide(color: palette.border, width: 1.5),
        ),
      ),
      child: Row(
        children: [
          // Left: App brand mark & title inside drag area
          DragToMoveArea(
            child: Container(
              padding: const EdgeInsets.symmetric(horizontal: 16),
              alignment: Alignment.centerLeft,
              child: Row(
                children: [
                  SizedBox(
                    width: 22,
                    height: 22,
                    child: Image.asset(
                      'Assets/app_icon.png',
                      fit: BoxFit.contain,
                    ),
                  ),
                  const SizedBox(width: 10),
                  Text(
                    'THUMBFORGE // THUMBNAIL MANAGER',
                    style: KineticTheme.labelSmall.copyWith(
                      color: palette.foreground,
                      fontWeight: FontWeight.w800,
                      letterSpacing: 1.0,
                    ),
                  ),
                ],
              ),
            ),
          ),

          // Center: Fill remaining space as draggable region
          Expanded(
            child: DragToMoveArea(
              child: Container(
                color: Colors.transparent,
              ),
            ),
          ),

          // Right Controls: Theme Toggle & Window Buttons
          Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              // Github Button
              MouseRegion(
                cursor: SystemMouseCursors.click,
                onEnter: (_) => setState(() => _githubHovered = true),
                onExit: (_) => setState(() => _githubHovered = false),
                child: GestureDetector(
                  onTap: () {
                    if (!Platform.environment.containsKey('FLUTTER_TEST')) {
                      Process.run('cmd', ['/c', 'start', 'https://github.com/mahtab-jack/ThumbForge']);
                    }
                  },
                  child: AnimatedContainer(
                    duration: const Duration(milliseconds: 150),
                    height: 42,
                    padding: const EdgeInsets.symmetric(horizontal: 14),
                    color: _githubHovered ? palette.surface : Colors.transparent,
                    child: Row(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        Icon(
                          Icons.open_in_new,
                          size: 15,
                          color: palette.foreground,
                        ),
                        const SizedBox(width: 8),
                        Text(
                          'GITHUB',
                          style: KineticTheme.labelSmall.copyWith(
                            color: palette.foreground,
                            fontWeight: FontWeight.w800,
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
              ),

              Container(
                width: 1,
                height: 24,
                color: palette.border,
              ),

              // Light / Dark Theme Button
              MouseRegion(
                cursor: SystemMouseCursors.click,
                onEnter: (_) => setState(() => _themeHovered = true),
                onExit: (_) => setState(() => _themeHovered = false),
                child: GestureDetector(
                  onTap: () {
                    final current = appThemeModeNotifier.value;
                    appThemeModeNotifier.value = (current == ThemeMode.dark ? ThemeMode.light : ThemeMode.dark);
                  },
                  child: AnimatedContainer(
                    duration: const Duration(milliseconds: 150),
                    height: 42,
                    padding: const EdgeInsets.symmetric(horizontal: 14),
                    color: _themeHovered ? palette.surface : Colors.transparent,
                    child: Row(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        Icon(
                          palette.isDark ? Icons.light_mode_outlined : Icons.dark_mode_outlined,
                          size: 15,
                          color: palette.accent,
                        ),
                        const SizedBox(width: 8),
                        Text(
                          palette.isDark ? 'LIGHT MODE' : 'DARK MODE',
                          style: KineticTheme.labelSmall.copyWith(
                            color: palette.foreground,
                            fontWeight: FontWeight.w800,
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
              ),

              Container(
                width: 1,
                height: 24,
                color: palette.border,
              ),

              // Minimize
              MouseRegion(
                cursor: SystemMouseCursors.click,
                onEnter: (_) => setState(() => _minimizeHovered = true),
                onExit: (_) => setState(() => _minimizeHovered = false),
                child: GestureDetector(
                  onTap: _handleMinimize,
                  child: Container(
                    width: 46,
                    height: 42,
                    color: _minimizeHovered ? palette.surface : Colors.transparent,
                    child: Center(
                      child: Icon(
                        Icons.remove,
                        size: 16,
                        color: palette.foreground,
                      ),
                    ),
                  ),
                ),
              ),

              // Maximize / Restore
              MouseRegion(
                cursor: SystemMouseCursors.click,
                onEnter: (_) => setState(() => _maximizeHovered = true),
                onExit: (_) => setState(() => _maximizeHovered = false),
                child: GestureDetector(
                  onTap: _handleMaximize,
                  child: Container(
                    width: 46,
                    height: 42,
                    color: _maximizeHovered ? palette.surface : Colors.transparent,
                    child: Center(
                      child: Icon(
                        Icons.crop_square,
                        size: 14,
                        color: palette.foreground,
                      ),
                    ),
                  ),
                ),
              ),

              // Close
              MouseRegion(
                cursor: SystemMouseCursors.click,
                onEnter: (_) => setState(() => _closeHovered = true),
                onExit: (_) => setState(() => _closeHovered = false),
                child: GestureDetector(
                  onTap: _handleClose,
                  child: Container(
                    width: 46,
                    height: 42,
                    color: _closeHovered ? KineticColors.error : Colors.transparent,
                    child: Center(
                      child: Icon(
                        Icons.close,
                        size: 16,
                        color: _closeHovered ? Colors.white : palette.foreground,
                      ),
                    ),
                  ),
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }
}
