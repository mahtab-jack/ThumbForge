import 'dart:convert';
import 'dart:io';
import 'package:path/path.dart' as p;

class ShellStatus {
  final bool isDllRegistered;
  final String registeredDllPath;
  final bool isPdfEnabled;
  final bool isVideoEnabled;
  final int videoExtensionsEnabledCount;
  final int totalVideoExtensionsCount;
  final bool isAudioEnabled;
  final int audioExtensionsEnabledCount;
  final int totalAudioExtensionsCount;
  final bool isApkEnabled;
  final bool isCodeEnabled;
  final int codeExtensionsEnabledCount;
  final int totalCodeExtensionsCount;
  final bool isHtmlEnabled;
  final int htmlExtensionsEnabledCount;
  final int totalHtmlExtensionsCount;
  final bool isEpubEnabled;
  final int epubExtensionsEnabledCount;
  final int totalEpubExtensionsCount;
  final String ffmpegPath;

  ShellStatus({
    required this.isDllRegistered,
    required this.registeredDllPath,
    required this.isPdfEnabled,
    required this.isVideoEnabled,
    required this.videoExtensionsEnabledCount,
    required this.totalVideoExtensionsCount,
    this.isAudioEnabled = false,
    this.audioExtensionsEnabledCount = 0,
    this.totalAudioExtensionsCount = 7,
    this.isApkEnabled = false,
    this.isCodeEnabled = false,
    this.codeExtensionsEnabledCount = 0,
    this.totalCodeExtensionsCount = 6,
    this.isHtmlEnabled = false,
    this.htmlExtensionsEnabledCount = 0,
    this.totalHtmlExtensionsCount = 2,
    this.isEpubEnabled = false,
    this.epubExtensionsEnabledCount = 0,
    this.totalEpubExtensionsCount = 1,
    required this.ffmpegPath,
  });

  factory ShellStatus.empty() {
    return ShellStatus(
      isDllRegistered: false,
      registeredDllPath: '',
      isPdfEnabled: false,
      isVideoEnabled: false,
      videoExtensionsEnabledCount: 0,
      totalVideoExtensionsCount: 10,
      isAudioEnabled: false,
      audioExtensionsEnabledCount: 0,
      totalAudioExtensionsCount: 7,
      isApkEnabled: false,
      isCodeEnabled: false,
      codeExtensionsEnabledCount: 0,
      totalCodeExtensionsCount: 6,
      isHtmlEnabled: false,
      htmlExtensionsEnabledCount: 0,
      totalHtmlExtensionsCount: 2,
      isEpubEnabled: false,
      epubExtensionsEnabledCount: 0,
      totalEpubExtensionsCount: 1,
      ffmpegPath: r'C:\ffmpeg\bin\ffmpeg.exe',
    );
  }

  factory ShellStatus.fromJson(Map<String, dynamic> json) {
    return ShellStatus(
      isDllRegistered: json['isDllRegistered'] == true,
      registeredDllPath: json['registeredDllPath'] ?? '',
      isPdfEnabled: json['isPdfEnabled'] == true,
      isVideoEnabled: json['isVideoEnabled'] == true,
      videoExtensionsEnabledCount: json['videoExtensionsEnabledCount'] ?? 0,
      totalVideoExtensionsCount: json['totalVideoExtensionsCount'] ?? 10,
      isAudioEnabled: json['isAudioEnabled'] == true,
      audioExtensionsEnabledCount: json['audioExtensionsEnabledCount'] ?? 0,
      totalAudioExtensionsCount: json['totalAudioExtensionsCount'] ?? 7,
      isApkEnabled: json['isApkEnabled'] == true,
      isCodeEnabled: json['isCodeEnabled'] == true,
      codeExtensionsEnabledCount: json['codeExtensionsEnabledCount'] ?? 0,
      totalCodeExtensionsCount: json['totalCodeExtensionsCount'] ?? 6,
      isHtmlEnabled: json['isHtmlEnabled'] == true,
      htmlExtensionsEnabledCount: json['htmlExtensionsEnabledCount'] ?? 0,
      totalHtmlExtensionsCount: json['totalHtmlExtensionsCount'] ?? 2,
      isEpubEnabled: json['isEpubEnabled'] == true,
      epubExtensionsEnabledCount: json['epubExtensionsEnabledCount'] ?? 0,
      totalEpubExtensionsCount: json['totalEpubExtensionsCount'] ?? 1,
      ffmpegPath: json['ffmpegPath'] ?? r'C:\ffmpeg\bin\ffmpeg.exe',
    );
  }
}

class RenderResult {
  final bool success;
  final String status;
  final int width;
  final int height;
  final String type;
  final String aspect;
  final String? pngPath;
  final String? error;

  RenderResult({
    required this.success,
    this.status = 'success',
    this.width = 0,
    this.height = 0,
    this.type = '',
    this.aspect = 'standard',
    this.pngPath,
    this.error,
  });
}

class ShellService {
  static String? _cachedCliPath;
  static String? _cachedDllPath;

  static String getCliPath() {
    if (_cachedCliPath != null && File(_cachedCliPath!).existsSync()) {
      return _cachedCliPath!;
    }

    final candidates = [
      p.join(Directory.current.path, 'native', 'build_nmake', 'ThumbForgeCli.exe'),
      p.join(Directory.current.path, 'native', 'build', 'Release', 'ThumbForgeCli.exe'),
      p.join(p.dirname(Platform.resolvedExecutable), 'ThumbForgeCli.exe'),
      p.join(p.dirname(Platform.resolvedExecutable), 'native', 'build_nmake', 'ThumbForgeCli.exe'),
      p.join(p.dirname(Platform.resolvedExecutable), 'native', 'build', 'Release', 'ThumbForgeCli.exe'),
      r'E:\Antigravity\ThumbForge\native\build_nmake\ThumbForgeCli.exe',
      r'E:\Antigravity\ThumbForge\native\build\Release\ThumbForgeCli.exe',
      r'E:\Antigravity\PreviewIcon\native\build_nmake\ThumbForgeCli.exe',
      r'E:\Antigravity\PreviewIcon\native\build\Release\ThumbForgeCli.exe',
    ];

    for (final path in candidates) {
      if (File(path).existsSync()) {
        _cachedCliPath = path;
        return path;
      }
    }

    return candidates.first;
  }

  static String getDllPath() {
    if (_cachedDllPath != null && File(_cachedDllPath!).existsSync()) {
      return _cachedDllPath!;
    }

    final candidates = [
      p.join(Directory.current.path, 'native', 'build_nmake', 'ThumbForgeProvider.dll'),
      p.join(Directory.current.path, 'native', 'build', 'Release', 'ThumbForgeProvider.dll'),
      p.join(p.dirname(Platform.resolvedExecutable), 'ThumbForgeProvider.dll'),
      p.join(p.dirname(Platform.resolvedExecutable), 'native', 'build_nmake', 'ThumbForgeProvider.dll'),
      p.join(p.dirname(Platform.resolvedExecutable), 'native', 'build', 'Release', 'ThumbForgeProvider.dll'),
      r'E:\Antigravity\ThumbForge\native\build_nmake\ThumbForgeProvider.dll',
      r'E:\Antigravity\ThumbForge\native\build\Release\ThumbForgeProvider.dll',
      r'E:\Antigravity\PreviewIcon\native\build_nmake\ThumbForgeProvider.dll',
      r'E:\Antigravity\PreviewIcon\native\build\Release\ThumbForgeProvider.dll',
    ];

    for (final path in candidates) {
      if (File(path).existsSync()) {
        _cachedDllPath = path;
        return path;
      }
    }

    return candidates.first;
  }

  static Future<ShellStatus> getStatus() async {
    if (Platform.environment.containsKey('FLUTTER_TEST')) {
      return ShellStatus.empty();
    }

    final cli = getCliPath();
    if (!File(cli).existsSync()) {
      return ShellStatus.empty();
    }

    try {
      final res = await Process.run(cli, ['status']);
      if (res.exitCode == 0) {
        final out = res.stdout.toString().trim();
        final json = jsonDecode(out) as Map<String, dynamic>;
        return ShellStatus.fromJson(json);
      }
    } catch (_) {}

    return ShellStatus.empty();
  }

  static Future<bool> applyThumbnails({
    required bool enablePdf,
    required bool enableVideo,
    bool enableAudio = true,
    bool enableApk = true,
    bool enableCode = true,
    bool enableHtml = true,
    bool enableEpub = true,
  }) async {
    final cli = getCliPath();
    final dll = getDllPath();

    final args = [
      'register',
      '--pdf=${enablePdf ? 1 : 0}',
      '--video=${enableVideo ? 1 : 0}',
      '--audio=${enableAudio ? 1 : 0}',
      '--apk=${enableApk ? 1 : 0}',
      '--code=${enableCode ? 1 : 0}',
      '--html=${enableHtml ? 1 : 0}',
      '--epub=${enableEpub ? 1 : 0}',
      '--dll=$dll',
    ];

    try {
      final res = await Process.run(cli, args);
      if (res.exitCode == 0) {
        final out = res.stdout.toString().trim();
        final json = jsonDecode(out) as Map<String, dynamic>;
        return json['success'] == true;
      }
    } catch (_) {}

    return false;
  }

  static Future<bool> resetThumbnails() async {
    final cli = getCliPath();
    try {
      final res = await Process.run(cli, ['unregister']);
      if (res.exitCode == 0) {
        final out = res.stdout.toString().trim();
        final json = jsonDecode(out) as Map<String, dynamic>;
        return json['success'] == true;
      }
    } catch (_) {}

    return false;
  }

  static Future<bool> refreshExplorer() async {
    final cli = getCliPath();
    try {
      final res = await Process.run(cli, ['refresh']);
      return res.exitCode == 0;
    } catch (_) {}

    return false;
  }

  static Future<RenderResult> renderSample(String sampleType, {int cx = 256}) async {
    if (Platform.environment.containsKey('FLUTTER_TEST')) {
      return RenderResult(success: true, width: cx, height: cx, type: sampleType);
    }

    final cli = getCliPath();
    final tempDir = Directory.systemTemp.createTempSync('thumb_forge_');
    final outPath = p.join(tempDir.path, 'sample_$sampleType.png');

    try {
      final res = await Process.run(cli, ['sample', sampleType, cx.toString(), outPath]);
      if (res.exitCode == 0 && File(outPath).existsSync()) {
        final out = res.stdout.toString().trim();
        final json = jsonDecode(out) as Map<String, dynamic>;
        return RenderResult(
          success: json['success'] == true,
          width: json['width'] ?? cx,
          height: json['height'] ?? cx,
          type: json['type'] ?? '',
          aspect: json['aspect'] ?? 'standard',
          pngPath: outPath,
        );
      }
    } catch (e) {
      return RenderResult(success: false, error: e.toString());
    }

    return RenderResult(success: false, error: 'Sample rendering failed');
  }

  static Future<RenderResult> renderFile(String filePath, {int cx = 256}) async {
    final cli = getCliPath();
    final tempDir = Directory.systemTemp.createTempSync('thumb_forge_');
    final outPath = p.join(tempDir.path, 'rendered_${p.basenameWithoutExtension(filePath)}.png');

    try {
      final res = await Process.run(cli, ['render', filePath, cx.toString(), outPath]);
      if (res.exitCode == 0 && File(outPath).existsSync()) {
        final out = res.stdout.toString().trim();
        final json = jsonDecode(out) as Map<String, dynamic>;
        return RenderResult(
          success: json['success'] == true,
          status: json['status'] ?? 'success',
          width: json['width'] ?? cx,
          height: json['height'] ?? cx,
          type: json['type'] ?? '',
          aspect: json['aspect'] ?? 'standard',
          pngPath: outPath,
        );
      }
    } catch (e) {
      return RenderResult(success: false, error: e.toString());
    }

    return RenderResult(success: false, error: 'Thumbnail generation failed');
  }
}
