// ignore_for_file: avoid_print, prefer_interpolation_to_compose_strings

import 'dart:io';

void main() async {
  final headerPath = r'native\src\AssetsData.h';
  final lockImg = r'Assets\lock.png';

  if (!File(lockImg).existsSync()) {
    print('Lock image not found');
    exit(1);
  }

  final data = await File(lockImg).readAsBytes();
  
  var outStr = 'const unsigned char kLockPngData[] = {\n';
  for (var i = 0; i < data.length; i += 16) {
    outStr += '    ';
    final chunk = data.skip(i).take(16);
    outStr += chunk.map((b) => '0x${b.toRadixString(16).padLeft(2, '0')}').join(', ') + ',\n';
  }
  outStr += '};\n';
  outStr += 'const unsigned int kLockPngData_size = ${data.length};';

  final content = await File(headerPath).readAsString();
  final newContent = content.replaceFirst(
    RegExp(r'const unsigned char kLockPngData\[\] = \{[\s\S]*?\};\s*const unsigned int kLockPngData_size = \d+;'),
    outStr
  );

  await File(headerPath).writeAsString(newContent);
  print('Updated AssetsData.h');
}
