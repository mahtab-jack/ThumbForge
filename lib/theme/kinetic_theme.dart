import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';

final ValueNotifier<ThemeMode> appThemeModeNotifier = ValueNotifier<ThemeMode>(ThemeMode.dark);

class KineticColors {
  static const Color background = Color(0xFF09090B);
  static const Color foreground = Color(0xFFFAFAFA);
  static const Color muted = Color(0xFF27272A);
  static const Color mutedForeground = Color(0xFFA1A1AA);
  static const Color accent = Color(0xFFDFE104);
  static const Color accentForeground = Color(0xFF000000);
  static const Color border = Color(0xFF3F3F46);
  static const Color borderLight = Color(0xFF52525B);
  static const Color success = Color(0xFF10B981);
  static const Color error = Color(0xFFEF4444);
}

class KineticPalette {
  final bool isDark;
  final Color background;
  final Color foreground;
  final Color surface;
  final Color muted;
  final Color mutedForeground;
  final Color accent;
  final Color accentForeground;
  final Color border;
  final Color borderLight;

  const KineticPalette({
    required this.isDark,
    required this.background,
    required this.foreground,
    required this.surface,
    required this.muted,
    required this.mutedForeground,
    required this.accent,
    required this.accentForeground,
    required this.border,
    required this.borderLight,
  });

  static KineticPalette of(BuildContext context) {
    final isDark = Theme.of(context).brightness == Brightness.dark;
    if (isDark) {
      return const KineticPalette(
        isDark: true,
        background: Color(0xFF09090B),
        foreground: Color(0xFFFAFAFA),
        surface: Color(0xFF141418),
        muted: Color(0xFF27272A),
        mutedForeground: Color(0xFFA1A1AA),
        accent: Color(0xFFDFE104),
        accentForeground: Color(0xFF000000),
        border: Color(0xFF3F3F46),
        borderLight: Color(0xFF52525B),
      );
    } else {
      return const KineticPalette(
        isDark: false,
        background: Color(0xFFF4F4F6),
        foreground: Color(0xFF09090B),
        surface: Color(0xFFFFFFFF),
        muted: Color(0xFFE4E4E7),
        mutedForeground: Color(0xFF52525B),
        accent: Color(0xFFDFE104),
        accentForeground: Color(0xFF000000),
        border: Color(0xFFD4D4D8),
        borderLight: Color(0xFFA1A1AA),
      );
    }
  }
}

class KineticTheme {
  static ThemeData get darkTheme {
    final darkBase = ThemeData.dark();
    return darkBase.copyWith(
      brightness: Brightness.dark,
      scaffoldBackgroundColor: const Color(0xFF09090B),
      primaryColor: KineticColors.accent,
      dividerColor: const Color(0xFF3F3F46),
      textTheme: GoogleFonts.spaceGroteskTextTheme(darkBase.textTheme).apply(
        bodyColor: const Color(0xFFFAFAFA),
        displayColor: const Color(0xFFFAFAFA),
      ),
      colorScheme: const ColorScheme.dark(
        surface: Color(0xFF27272A),
        primary: KineticColors.accent,
        onPrimary: KineticColors.accentForeground,
        secondary: KineticColors.accent,
        onSecondary: KineticColors.accentForeground,
        onSurface: Color(0xFFFAFAFA),
      ),
    );
  }

  static ThemeData get lightTheme {
    final lightBase = ThemeData.light();
    return lightBase.copyWith(
      brightness: Brightness.light,
      scaffoldBackgroundColor: const Color(0xFFF4F4F6),
      primaryColor: KineticColors.accent,
      dividerColor: const Color(0xFFD4D4D8),
      textTheme: GoogleFonts.spaceGroteskTextTheme(lightBase.textTheme).apply(
        bodyColor: const Color(0xFF09090B),
        displayColor: const Color(0xFF09090B),
      ),
      colorScheme: const ColorScheme.light(
        surface: Color(0xFFE4E4E7),
        primary: KineticColors.accent,
        onPrimary: KineticColors.accentForeground,
        secondary: KineticColors.accent,
        onSecondary: KineticColors.accentForeground,
        onSurface: Color(0xFF09090B),
      ),
    );
  }

  // Display Typography Tokens
  static TextStyle displayLarge = GoogleFonts.spaceGrotesk(
    fontSize: 48,
    fontWeight: FontWeight.w800,
    letterSpacing: -1.5,
    height: 0.95,
  );

  static TextStyle displayMedium = GoogleFonts.spaceGrotesk(
    fontSize: 32,
    fontWeight: FontWeight.w700,
    letterSpacing: -1.0,
    height: 1.0,
  );

  static TextStyle sectionTitle = GoogleFonts.spaceGrotesk(
    fontSize: 22,
    fontWeight: FontWeight.w700,
    letterSpacing: -0.8,
    height: 1.1,
  );

  static TextStyle cardTitle = GoogleFonts.spaceGrotesk(
    fontSize: 18,
    fontWeight: FontWeight.w700,
    letterSpacing: -0.5,
  );

  static TextStyle bodyText = GoogleFonts.spaceGrotesk(
    fontSize: 14,
    fontWeight: FontWeight.w400,
    height: 1.4,
  );

  static TextStyle buttonText = GoogleFonts.spaceGrotesk(
    fontSize: 14,
    fontWeight: FontWeight.w700,
    letterSpacing: -0.3,
  );

  static TextStyle labelSmall = GoogleFonts.spaceGrotesk(
    fontSize: 11,
    fontWeight: FontWeight.w700,
    letterSpacing: 1.2,
  );

  static TextStyle massiveNumber = GoogleFonts.spaceGrotesk(
    fontSize: 72,
    fontWeight: FontWeight.w900,
    letterSpacing: -3.0,
    height: 0.85,
  );
}
