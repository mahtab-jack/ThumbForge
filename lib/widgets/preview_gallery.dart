import 'dart:io';
import 'package:flutter/material.dart';
import 'package:file_picker/file_picker.dart';
import 'package:path/path.dart' as p;
import '../theme/kinetic_theme.dart';
import '../services/shell_service.dart';

class PreviewGallery extends StatefulWidget {
  const PreviewGallery({super.key});

  @override
  State<PreviewGallery> createState() => _PreviewGalleryState();
}

class _PreviewGalleryState extends State<PreviewGallery> {
  String _activeMode = 'pdf_normal';
  RenderResult? _currentResult;
  bool _isLoading = false;
  String? _customFileName;

  final List<Map<String, String>> _presetModes = [
    {'id': 'pdf_normal', 'label': 'PDF PAGE 1', 'category': 'PDF'},
    {'id': 'pdf_locked', 'label': 'PDF LOCKED', 'category': 'PDF'},
    {'id': 'pdf_corrupt', 'label': 'PDF CORRUPT', 'category': 'PDF'},
    {'id': 'video_landscape', 'label': 'VIDEO 16:9', 'category': 'VIDEO'},
    {'id': 'video_portrait', 'label': 'VIDEO 9:16', 'category': 'VIDEO'},
    {'id': 'video_square', 'label': 'VIDEO 1:1', 'category': 'VIDEO'},
    {'id': 'audio_cover', 'label': 'AUDIO WITH COVER', 'category': 'AUDIO'},
    {'id': 'audio_nocover', 'label': 'AUDIO DISC', 'category': 'AUDIO'},
    {'id': 'apk_sample', 'label': 'APK APP ICON', 'category': 'APK'},
    {'id': 'code_json', 'label': 'JSON', 'category': 'CODE'},
    {'id': 'code_markdown', 'label': 'MARKDOWN', 'category': 'CODE'},
    {'id': 'code_js', 'label': 'JAVASCRIPT', 'category': 'CODE'},
    {'id': 'code_css', 'label': 'CSS', 'category': 'CODE'},
  ];

  @override
  void initState() {
    super.initState();
    _loadPreset(_activeMode);
  }

  Future<void> _loadPreset(String modeId) async {
    setState(() {
      _isLoading = true;
      _activeMode = modeId;
      _customFileName = null;
    });

    final res = await ShellService.renderSample(modeId, cx: 320);

    if (mounted) {
      setState(() {
        _isLoading = false;
        _currentResult = res;
      });
    }
  }

  Future<void> _pickAndRenderFile() async {
    try {
      final files = await FilePicker.pickFiles(
        type: FileType.custom,
        allowedExtensions: [
          'pdf',
          'mp4', 'mkv', 'avi', 'mov', 'webm', 'wmv', 'flv', 'm4v', 'ts', '3gp',
          'mp3', 'flac', 'wav', 'm4a', 'aac', 'ogg', 'opus', 'wma',
          'apk',
          'json', 'md', 'markdown', 'js', 'mjs', 'css',
        ],
      );

      if (files.isNotEmpty && files.first.path != null) {
        final filePath = files.first.path!;
        setState(() {
          _isLoading = true;
          _activeMode = 'custom';
          _customFileName = p.basename(filePath);
        });

        final res = await ShellService.renderFile(filePath, cx: 320);

        if (mounted) {
          setState(() {
            _isLoading = false;
            _currentResult = res;
          });
        }
      }
    } catch (_) {
      if (mounted) {
        setState(() => _isLoading = false);
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    final palette = KineticPalette.of(context);

    return Container(
      decoration: BoxDecoration(
        color: palette.background,
        borderRadius: BorderRadius.zero,
        border: Border.all(color: palette.border, width: 2.0),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          // Header Bar
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 14),
            decoration: BoxDecoration(
              color: palette.muted,
              border: Border(
                bottom: BorderSide(color: palette.border, width: 1.5),
              ),
            ),
            child: Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Expanded(
                  child: Row(
                    children: [
                      Container(
                        width: 8,
                        height: 8,
                        color: palette.accent,
                      ),
                      const SizedBox(width: 10),
                      Flexible(
                        child: Text(
                          'THUMBNAIL PREVIEW LAB',
                          overflow: TextOverflow.ellipsis,
                          style: KineticTheme.cardTitle.copyWith(
                            color: palette.foreground,
                            letterSpacing: 0.5,
                          ),
                        ),
                      ),
                    ],
                  ),
                ),
                const SizedBox(width: 8),
                // File Picker Trigger
                MouseRegion(
                  cursor: SystemMouseCursors.click,
                  child: GestureDetector(
                    onTap: _pickAndRenderFile,
                    child: Container(
                      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 6),
                      decoration: BoxDecoration(
                        color: _activeMode == 'custom' ? palette.accent : Colors.transparent,
                        borderRadius: BorderRadius.zero,
                        border: Border.all(
                          color: _activeMode == 'custom' ? palette.accent : palette.borderLight,
                          width: 1.5,
                        ),
                      ),
                      child: Text(
                        'TEST LOCAL FILE',
                        style: KineticTheme.labelSmall.copyWith(
                          color: _activeMode == 'custom'
                              ? palette.accentForeground
                              : palette.foreground,
                        ),
                      ),
                    ),
                  ),
                ),
              ],
            ),
          ),

          // Preset Mode Selector Tabs
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
            decoration: BoxDecoration(
              color: palette.isDark ? const Color(0xFF111114) : const Color(0xFFEBEBEF),
              border: Border(
                bottom: BorderSide(color: palette.border, width: 1.0),
              ),
            ),
            child: SingleChildScrollView(
              scrollDirection: Axis.horizontal,
              child: Row(
                children: [
                  ..._presetModes.map((mode) {
                    final isSelected = (_activeMode == mode['id']);
                    return Padding(
                      padding: const EdgeInsets.only(right: 8.0),
                      child: MouseRegion(
                        cursor: SystemMouseCursors.click,
                        child: GestureDetector(
                          onTap: () => _loadPreset(mode['id']!),
                          child: AnimatedContainer(
                            duration: const Duration(milliseconds: 150),
                            padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
                            decoration: BoxDecoration(
                              color: isSelected ? palette.foreground : Colors.transparent,
                              borderRadius: BorderRadius.zero,
                              border: Border.all(
                                color: isSelected ? palette.foreground : palette.border,
                                width: 1.0,
                              ),
                            ),
                            child: Text(
                              mode['label']!,
                              style: KineticTheme.labelSmall.copyWith(
                                color: isSelected ? palette.background : palette.mutedForeground,
                                fontWeight: isSelected ? FontWeight.w800 : FontWeight.w600,
                              ),
                            ),
                          ),
                        ),
                      ),
                    );
                  }),
                  if (_customFileName != null)
                    Container(
                      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
                      decoration: BoxDecoration(
                        color: palette.accent,
                        borderRadius: BorderRadius.zero,
                        border: Border.all(color: palette.accent, width: 1.0),
                      ),
                      child: Text(
                        _customFileName!.toUpperCase(),
                        style: KineticTheme.labelSmall.copyWith(
                          color: palette.accentForeground,
                          fontWeight: FontWeight.w800,
                        ),
                      ),
                    ),
                ],
              ),
            ),
          ),

          // Main Canvas Display
          Expanded(
            child: Container(
              color: palette.isDark ? const Color(0xFF070709) : const Color(0xFFEAEAEF),
              child: Stack(
                alignment: Alignment.center,
                children: [
                  // Minimal geometric grid dots
                  Positioned.fill(
                    child: CustomPaint(
                      painter: _GridPainter(isDark: palette.isDark),
                    ),
                  ),

                  // Image or Loading
                  if (_isLoading)
                    Center(
                      child: SizedBox(
                        width: 36,
                        height: 36,
                        child: CircularProgressIndicator(
                          strokeWidth: 2,
                          valueColor: AlwaysStoppedAnimation<Color>(palette.accent),
                        ),
                      ),
                    )
                  else if (_currentResult != null && _currentResult!.success && _currentResult!.pngPath != null)
                    Padding(
                      padding: const EdgeInsets.all(24.0),
                      child: Center(
                        child: Container(
                          decoration: BoxDecoration(
                            border: Border.all(color: palette.borderLight, width: 1.5),
                          ),
                          child: Image.file(
                            File(_currentResult!.pngPath!),
                            fit: BoxFit.contain,
                            key: ValueKey(_currentResult!.pngPath),
                          ),
                        ),
                      ),
                    )
                  else
                    Center(
                      child: Text(
                        'NO PREVIEW GENERATED',
                        style: KineticTheme.labelSmall.copyWith(color: palette.mutedForeground),
                      ),
                    ),
                ],
              ),
            ),
          ),

          // Technical Telemetry Footer
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 12),
            decoration: BoxDecoration(
              color: palette.muted,
              border: Border(
                top: BorderSide(color: palette.border, width: 1.5),
              ),
            ),
            child: Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Flexible(
                  child: Text(
                    _currentResult != null && _currentResult!.success
                        ? '${_currentResult!.width} X ${_currentResult!.height} PX // RESOLUTION'
                        : 'STANDBY // 0 X 0 PX',
                    overflow: TextOverflow.ellipsis,
                    style: KineticTheme.labelSmall.copyWith(
                      color: palette.mutedForeground,
                    ),
                  ),
                ),
                const SizedBox(width: 8),
                Text(
                  _activeMode.startsWith('pdf') || (_currentResult?.type == 'pdf')
                      ? 'PDFIUM V8076'
                      : 'FFMPEG 7.1',
                  style: KineticTheme.labelSmall.copyWith(
                    color: palette.accent,
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _GridPainter extends CustomPainter {
  final bool isDark;
  _GridPainter({required this.isDark});

  @override
  void paint(Canvas canvas, Size size) {
    final paint = Paint()
      ..color = isDark ? const Color(0xFF1A1A20) : const Color(0xFFD4D4D8)
      ..strokeWidth = 1.0;

    const step = 28.0;
    for (double x = 0; x < size.width; x += step) {
      for (double y = 0; y < size.height; y += step) {
        canvas.drawCircle(Offset(x, y), 0.8, paint);
      }
    }
  }

  @override
  bool shouldRepaint(covariant _GridPainter oldDelegate) => oldDelegate.isDark != isDark;
}
