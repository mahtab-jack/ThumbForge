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
  bool _isApplyLoading = false;
  bool _isResetLoading = false;
  bool _isRefreshLoading = false;

  bool get _isAnyActionLoading => _isApplyLoading || _isResetLoading || _isRefreshLoading;

  String? _statusBanner;
  bool _bannerIsSuccess = true;

  bool _pdfEnabled = true;
  bool _videoEnabled = true;
  bool _audioEnabled = true;
  bool _apkEnabled = true;
  bool _codeEnabled = true;
  bool _htmlEnabled = true;
  bool _epubEnabled = true;

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
          _htmlEnabled = s.isHtmlEnabled;
          _epubEnabled = s.isEpubEnabled;
        }
      });
    }
  }

  Future<void> _handleApply() async {
    setState(() {
      _isApplyLoading = true;
      _statusBanner = null;
    });

    final ok = await ShellService.applyThumbnails(
      enablePdf: _pdfEnabled,
      enableVideo: _videoEnabled,
      enableAudio: _audioEnabled,
      enableApk: _apkEnabled,
      enableCode: _codeEnabled,
      enableHtml: _htmlEnabled,
      enableEpub: _epubEnabled,
    );

    await _refreshStatus();

    if (mounted) {
      setState(() {
        _isApplyLoading = false;
        _bannerIsSuccess = ok;
        _statusBanner = ok
            ? 'CUSTOM THUMBNAILS APPLIED // WINDOWS EXPLORER NOTIFIED'
            : 'REGISTRATION FAILED // CHECK COMPONENT AVAILABILITY';
      });
    }
  }

  Future<void> _handleReset() async {
    setState(() {
      _isResetLoading = true;
      _statusBanner = null;
    });

    final ok = await ShellService.resetThumbnails();
    await _refreshStatus();

    if (mounted) {
      setState(() {
        _isResetLoading = false;
        _bannerIsSuccess = ok;
        _statusBanner = ok
            ? 'RESTORED WINDOWS DEFAULT THUMBNAILS // EXPLORER CACHE FLUSHED'
            : 'RESET COMPLETED WITH WARNINGS';
      });
    }
  }

  Future<void> _handleRefreshCache() async {
    setState(() {
      _isRefreshLoading = true;
      _statusBanner = null;
    });

    final ok = await ShellService.refreshExplorer();

    if (mounted) {
      setState(() {
        _isRefreshLoading = false;
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
                        SizedBox(
                          width: 38,
                          height: 38,
                          child: Image.asset(
                            'Assets/app_icon.png',
                            fit: BoxFit.contain,
                            errorBuilder: (context, error, stackTrace) =>
                                const SizedBox.shrink(),
                          ),
                        ),
                        const SizedBox(width: 14),
                        Text(
                          'THUMBFORGE',
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
                    _telemetryBlock(palette, 'CODE', '${_status.codeExtensionsEnabledCount}/6', _status.isCodeEnabled),
                    const SizedBox(width: 12),
                    _telemetryBlock(palette, 'HTML', '${_status.htmlExtensionsEnabledCount}/2', _status.isHtmlEnabled),
                    const SizedBox(width: 12),
                    _telemetryBlock(palette, 'EPUB', '${_status.epubExtensionsEnabledCount}/1', _status.isEpubEnabled),
                  ],
                ),
              ],
            ),
          ),

          // Infinite Marquee Ticker
          const MarqueeTicker(
            items: [
              'ENGINE: CHROMIUM HEADLESS • PDFIUM V8076 • FFMPEG 7.1 • NATIVE GDI+',
              'ISOLATION: SYSTEM FILE ASSOCIATIONS SAFE // ZERO REGISTRY CONFLICT',
              'HTML: TRUE BROWSER RENDERING (A4 RATIO) // NO CODE TEXT',
              'EPUB: A4 BOOK SHEET & COVER PREVIEW // PDF COMPATIBLE RATIO',
              'PDF: CLEAN NATURAL CORNER // CENTER LOCK & CORRUPT CANVAS',
              'VIDEO: 16:9 • 9:16 • 1:1 ASPECT PRESERVATION WITH CENTER OVERLAY',
              'AUDIO: ALBUM ART COVER WITH CENTER MUSIC EMBLEM',
              'APK: ANDROID ORIGINAL APP LAUNCHER ICON RESOLUTION',
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
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.stretch,
                      children: [
                        // Status Notification Banner (Fixed position)
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
                        Expanded(
                          child: SingleChildScrollView(
                            child: Column(
                              crossAxisAlignment: CrossAxisAlignment.stretch,
                              children: [


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

                          const SizedBox(height: 16),

                          // Module 06: HTML Browser Preview
                          KineticToggleCard(
                            number: '06',
                            title: 'HTML BROWSER PREVIEWS',
                            subtitle: 'Actual web page rendering in A4 format (not code/text) via headless browser engine',
                            tags: const ['A4 FORMAT', 'TRUE BROWSER', 'NO CODE', '.HTML', '.HTM'],
                            value: _htmlEnabled,
                            onChanged: (val) => setState(() => _htmlEnabled = val),
                          ),

                          const SizedBox(height: 16),

                          // Module 07: EPUB Book Preview
                          KineticToggleCard(
                            number: '07',
                            title: 'EPUB BOOK PREVIEWS',
                            subtitle: 'A4 book sheet layout with high-res cover art and chapter previews similar to PDF',
                            tags: const ['A4 BOOK SHEET', 'COVER ART', 'PDF STYLE', '.EPUB'],
                            value: _epubEnabled,
                            onChanged: (val) => setState(() => _epubEnabled = val),
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
                                  isLoading: _isApplyLoading,
                                  onPressed: _isAnyActionLoading ? null : _handleApply,
                                ),
                              ),
                              const SizedBox(width: 12),
                              Expanded(
                                flex: 2,
                                child: KineticButton(
                                  label: 'RESET DEFAULTS',
                                  variant: KineticButtonVariant.outline,
                                  isLoading: _isResetLoading,
                                  onPressed: _isAnyActionLoading ? null : _handleReset,
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
                            isLoading: _isRefreshLoading,
                            onPressed: _isAnyActionLoading ? null : _handleRefreshCache,
                          ),
                        ],
                      ),
                    ),
                  ),
                      ],
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
