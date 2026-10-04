import 'package:flutter/material.dart';
import '../theme/kinetic_theme.dart';
import '../services/shell_service.dart';
import '../widgets/custom_title_bar.dart';
import '../widgets/marquee_ticker.dart';
import '../widgets/kinetic_button.dart';
import '../widgets/kinetic_toggle_card.dart';
import '../widgets/preview_gallery.dart';

class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  ShellStatus _status = ShellStatus.empty();
  bool _isActionLoading = false;
  String? _statusBanner;
  bool _bannerIsSuccess = true;

  bool _pdfEnabled = true;
  bool _videoEnabled = true;
  bool _audioEnabled = true;
  bool _apkEnabled = true;
  bool _codeEnabled = true;

  @override
  void initState() {
    super.initState();
    _refreshStatus();
  }

  Future<void> _refreshStatus() async {
    final s = await ShellService.getStatus();
    if (mounted) {
      setState(() {
        _status = s;
        if (s.isDllRegistered) {
          _pdfEnabled = s.isPdfEnabled;
          _videoEnabled = s.isVideoEnabled;
          _audioEnabled = s.isAudioEnabled;
          _apkEnabled = s.isApkEnabled;
          _codeEnabled = s.isCodeEnabled;
        }
      });
    }
  }

  Future<void> _handleApply() async {
    setState(() {
      _isActionLoading = true;
      _statusBanner = null;
    });

    final ok = await ShellService.applyThumbnails(
      enablePdf: _pdfEnabled,
      enableVideo: _videoEnabled,
      enableAudio: _audioEnabled,
      enableApk: _apkEnabled,
      enableCode: _codeEnabled,
    );

    await _refreshStatus();

    if (mounted) {
      setState(() {
        _isActionLoading = false;
        _bannerIsSuccess = ok;
        _statusBanner = ok
            ? 'CUSTOM THUMBNAILS APPLIED // WINDOWS EXPLORER NOTIFIED'
            : 'REGISTRATION FAILED // CHECK COMPONENT AVAILABILITY';
      });
    }
  }

  Future<void> _handleReset() async {
    setState(() {
      _isActionLoading = true;
      _statusBanner = null;
    });

    final ok = await ShellService.resetThumbnails();
    await _refreshStatus();

    if (mounted) {
      setState(() {
        _isActionLoading = false;
        _bannerIsSuccess = ok;
        _statusBanner = ok
            ? 'RESTORED WINDOWS DEFAULT THUMBNAILS // EXPLORER CACHE FLUSHED'
            : 'RESET COMPLETED WITH WARNINGS';
      });
    }
  }

  Future<void> _handleRefreshCache() async {
    setState(() {
      _isActionLoading = true;
      _statusBanner = null;
    });

    final ok = await ShellService.refreshExplorer();

    if (mounted) {
      setState(() {
        _isActionLoading = false;
        _bannerIsSuccess = ok;
        _statusBanner = ok
            ? 'THUMBNAIL CACHE PURGED // EXPLORER WINDOWS RELOADED'
            : 'REFRESH FAILED';
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    final palette = KineticPalette.of(context);
    final isSystemActive = _status.isDllRegistered && (_status.isPdfEnabled || _status.isVideoEnabled);

    return Scaffold(
      backgroundColor: palette.background,
      body: Column(
        children: [
          // Custom Frameless Top Bar with Window Controls & Theme Switcher
          const CustomTitleBar(),

          // Main Header Section
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 32.0, vertical: 20.0),
            decoration: BoxDecoration(
              color: palette.background,
              border: Border(
                bottom: BorderSide(color: palette.border, width: 2.0),
              ),
            ),
            child: Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              crossAxisAlignment: CrossAxisAlignment.center,
              children: [
                Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Row(
                      children: [
                        Container(
                          width: 36,
                          height: 36,
                          decoration: BoxDecoration(
                            border: Border.all(color: palette.accent, width: 2),
                          ),
                          child: Image.asset(
                            'Assets/app_icon.png',
                            fit: BoxFit.cover,
                            errorBuilder: (context, error, stackTrace) =>
                                const SizedBox.shrink(),
                          ),
                        ),
                        const SizedBox(width: 14),
                        Text(
                          'PREVIEWICON',
                          style: KineticTheme.displayLarge.copyWith(
                            color: palette.foreground,
                            letterSpacing: -1.0,
                          ),
                        ),
                        const SizedBox(width: 14),
                        Container(
                          padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 4),
                          decoration: BoxDecoration(
                            color: isSystemActive ? palette.accent : palette.muted,
                            borderRadius: BorderRadius.zero,
                          ),
                          child: Text(
                            isSystemActive ? 'ONLINE' : 'STANDBY',
                            style: KineticTheme.labelSmall.copyWith(
                              color: isSystemActive
                                  ? palette.accentForeground
                                  : palette.mutedForeground,
                              fontWeight: FontWeight.w900,
                            ),
                          ),
                        ),
                      ],
                    ),
                    const SizedBox(height: 6),
                    Text(
                      'WINDOWS SHELL THUMBNAIL PROVIDER // NATIVE C++ ENGINE',
                      style: KineticTheme.labelSmall.copyWith(
                        color: palette.mutedForeground,
                        letterSpacing: 1.2,
                      ),
                    ),
                  ],
                ),
                // Telemetry summary
                Row(
                  children: [
                    _telemetryBlock(palette, 'PDF', _status.isPdfEnabled ? 'ON' : 'OFF', _status.isPdfEnabled),
                    const SizedBox(width: 12),
                    _telemetryBlock(palette, 'VIDEO', '${_status.videoExtensionsEnabledCount}/10', _status.isVideoEnabled),
                    const SizedBox(width: 12),
                    _telemetryBlock(palette, 'AUDIO', '${_status.audioExtensionsEnabledCount}/7', _status.isAudioEnabled),
                    const SizedBox(width: 12),
                    _telemetryBlock(palette, 'APK', _status.isApkEnabled ? 'ON' : 'OFF', _status.isApkEnabled),
                    const SizedBox(width: 12),
                    _telemetryBlock(palette, 'CODE', '${_status.codeExtensionsEnabledCount}/9', _status.isCodeEnabled),
                  ],
                ),
              ],
            ),
          ),

          // Infinite Marquee Ticker
          const MarqueeTicker(
            items: [
              'ENGINE: PDFIUM V8076 • FFMPEG 7.1 • NATIVE GDI+ • SYSTEM PARSERS',
              'ISOLATION: SYSTEM FILE ASSOCIATIONS SAFE // ZERO REGISTRY CONFLICT',
              'PDF: CLEAN NATURAL CORNER // CENTER LOCK & CORRUPT CANVAS',
              'VIDEO: 16:9 • 9:16 • 1:1 ASPECT PRESERVATION WITH CENTER OVERLAY',
              'AUDIO: ALBUM ART COVER WITH CENTER MUSIC EMBLEM (ASSETS/MUSIC.PNG)',
              'APK: ANDROID APP LAUNCHER ICON CENTERED ON CARD CANVAS',
              'CODE: JSON • MARKDOWN • JAVASCRIPT • CSS COLORFUL SYNTAX SHEETS',
              'DEFAULT APP ASSOCIATIONS: 100% UNTOUCHED // CLEAN RESTORE SUPPORTED',
            ],
            speed: 50.0,
          ),

          // Main Responsive Content Area
          Expanded(
            child: Padding(
              padding: const EdgeInsets.all(32.0),
              child: Row(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  // Left Control Column
                  Expanded(
                    flex: 5,
                    child: SingleChildScrollView(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.stretch,
                        children: [
                          // Status Notification Banner
                          if (_statusBanner != null) ...[
                            Container(
                              padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
                              decoration: BoxDecoration(
                                color: _bannerIsSuccess
                                    ? palette.muted
                                    : (palette.isDark ? const Color(0xFF2A1010) : const Color(0xFFFDE8E8)),
                                border: Border.all(
                                  color: _bannerIsSuccess ? palette.accent : KineticColors.error,
                                  width: 1.5,
                                ),
                              ),
                              child: Row(
                                children: [
                                  Container(
                                    width: 8,
                                    height: 8,
                                    color: _bannerIsSuccess ? palette.accent : KineticColors.error,
                                  ),
                                  const SizedBox(width: 12),
                                  Expanded(
                                    child: Text(
                                      _statusBanner!,
                                      style: KineticTheme.labelSmall.copyWith(
                                        color: palette.foreground,
                                        letterSpacing: 1.0,
                                      ),
                                    ),
                                  ),
                                  GestureDetector(
                                    onTap: () => setState(() => _statusBanner = null),
                                    child: MouseRegion(
                                      cursor: SystemMouseCursors.click,
                                      child: Icon(Icons.close, size: 16, color: palette.mutedForeground),
                                    ),
                                  ),
                                ],
                              ),
                            ),
                            const SizedBox(height: 20),
                          ],

                          // Module 01: PDF
                          KineticToggleCard(
                            number: '01',
                            title: 'PDF THUMBNAILS',
                            subtitle: 'Page 1 preview • Center lock & warning on white canvas • Clean corner',
                            tags: const ['PAGE 1', 'CENTER LOCK', 'CENTER CORRUPT', 'CLEAN CORNER', 'PDFIUM'],
                            value: _pdfEnabled,
                            onChanged: (val) => setState(() => _pdfEnabled = val),
                          ),

                          const SizedBox(height: 16),

                          // Module 02: Video
                          KineticToggleCard(
                            number: '02',
                            title: 'VIDEO THUMBNAILS',
                            subtitle: 'Aspect ratio preservation with centered play-button overlay',
                            tags: const ['16:9 LANDSCAPE', '9:16 PORTRAIT', '1:1 SQUARE', '10 FORMATS', 'FFMPEG'],
                            value: _videoEnabled,
                            onChanged: (val) => setState(() => _videoEnabled = val),
                          ),

                          const SizedBox(height: 16),

                          // Module 03: Audio
                          KineticToggleCard(
                            number: '03',
                            title: 'AUDIO THUMBNAILS',
                            subtitle: 'Embedded album art with music.png center emblem • Vinyl disc fallback',
                            tags: const ['MP3', 'FLAC', 'WAV', 'M4A', 'AAC', 'OGG', 'OPUS', 'WMA', 'CENTER EMBLEM'],
                            value: _audioEnabled,
                            onChanged: (val) => setState(() => _audioEnabled = val),
                          ),

                          const SizedBox(height: 16),

                          // Module 04: APK
                          KineticToggleCard(
                            number: '04',
                            title: 'APK APP ICONS',
                            subtitle: 'Android launcher icon extracted and centered on card canvas with APK badge',
                            tags: const ['APP ICON', 'CENTERED CANVAS', 'FAST EXTRACT', 'APK BADGE'],
                            value: _apkEnabled,
                            onChanged: (val) => setState(() => _apkEnabled = val),
                          ),

                          const SizedBox(height: 16),

                          // Module 05: Code & Markup
                          KineticToggleCard(
                            number: '05',
                            title: 'CODE & MARKUP PREVIEWS',
                            subtitle: 'Rich colorful syntax sheets with line numbers and format badge header',
                            tags: const ['JSON', 'MARKDOWN', 'JAVASCRIPT', 'CSS', 'SYNTAX COLORS'],
                            value: _codeEnabled,
                            onChanged: (val) => setState(() => _codeEnabled = val),
                          ),

                          const SizedBox(height: 28),

                          // Action Buttons
                          Row(
                            children: [
                              Expanded(
                                flex: 3,
                                child: KineticButton(
                                  label: 'APPLY THUMBNAILS',
                                  variant: KineticButtonVariant.primary,
                                  isLoading: _isActionLoading,
                                  onPressed: _handleApply,
                                ),
                              ),
                              const SizedBox(width: 12),
                              Expanded(
                                flex: 2,
                                child: KineticButton(
                                  label: 'RESET DEFAULTS',
                                  variant: KineticButtonVariant.outline,
                                  isLoading: _isActionLoading,
                                  onPressed: _handleReset,
                                ),
                              ),
                            ],
                          ),

                          const SizedBox(height: 12),

                          // Explorer Refresh Action
                          KineticButton(
                            label: 'PURGE CACHE & RELOAD EXPLORER',
                            variant: KineticButtonVariant.outline,
                            height: 44,
                            isLoading: _isActionLoading,
                            onPressed: _handleRefreshCache,
                          ),
                        ],
                      ),
                    ),
                  ),

                  const SizedBox(width: 32),

                  // Right Live Preview Column
                  const Expanded(
                    flex: 6,
                    child: PreviewGallery(),
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _telemetryBlock(KineticPalette palette, String label, String value, bool active) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      decoration: BoxDecoration(
        color: palette.muted,
        borderRadius: BorderRadius.zero,
        border: Border.all(color: palette.border, width: 1.0),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            label,
            style: KineticTheme.labelSmall.copyWith(
              color: palette.mutedForeground,
              fontSize: 10,
            ),
          ),
          const SizedBox(height: 2),
          Text(
            value,
            style: KineticTheme.cardTitle.copyWith(
              color: active ? palette.accent : palette.foreground,
              fontSize: 14,
            ),
          ),
        ],
      ),
    );
  }
}
