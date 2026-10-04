import 'package:flutter/material.dart';
import '../theme/kinetic_theme.dart';

class KineticToggleCard extends StatefulWidget {
  final String number;
  final String title;
  final String subtitle;
  final List<String> tags;
  final bool value;
  final ValueChanged<bool> onChanged;

  const KineticToggleCard({
    super.key,
    required this.number,
    required this.title,
    required this.subtitle,
    required this.tags,
    required this.value,
    required this.onChanged,
  });

  @override
  State<KineticToggleCard> createState() => _KineticToggleCardState();
}

class _KineticToggleCardState extends State<KineticToggleCard> {
  bool _isHovered = false;

  @override
  Widget build(BuildContext context) {
    final palette = KineticPalette.of(context);

    Color cardBg;
    if (widget.value) {
      cardBg = _isHovered ? palette.surface : (palette.isDark ? const Color(0xFF111115) : const Color(0xFFFFFFFF));
    } else {
      cardBg = palette.background;
    }

    return MouseRegion(
      onEnter: (_) => setState(() => _isHovered = true),
      onExit: (_) => setState(() => _isHovered = false),
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 200),
        padding: const EdgeInsets.all(24.0),
        decoration: BoxDecoration(
          color: cardBg,
          borderRadius: BorderRadius.zero,
          border: Border.all(
            color: widget.value
                ? (_isHovered ? palette.accent : palette.borderLight)
                : palette.border,
            width: 2.0,
          ),
        ),
        child: Stack(
          children: [
            // Decorative background number (Kinetic signature)
            Positioned(
              right: 0,
              top: -8,
              child: IgnorePointer(
                child: Text(
                  widget.number,
                  style: KineticTheme.massiveNumber.copyWith(
                    color: widget.value
                        ? palette.muted.withValues(alpha: palette.isDark ? 0.4 : 0.6)
                        : palette.muted.withValues(alpha: palette.isDark ? 0.15 : 0.3),
                  ),
                ),
              ),
            ),
            Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(
                            widget.title.toUpperCase(),
                            style: KineticTheme.cardTitle.copyWith(
                              color: palette.foreground,
                              letterSpacing: 0.5,
                            ),
                          ),
                          const SizedBox(height: 6),
                          Text(
                            widget.subtitle,
                            style: KineticTheme.bodyText.copyWith(
                              color: palette.mutedForeground,
                            ),
                          ),
                        ],
                      ),
                    ),
                    const SizedBox(width: 16),
                    // Brutalist sharp toggle switch
                    GestureDetector(
                      onTap: () => widget.onChanged(!widget.value),
                      child: MouseRegion(
                        cursor: SystemMouseCursors.click,
                        child: AnimatedContainer(
                          duration: const Duration(milliseconds: 150),
                          width: 58,
                          height: 32,
                          decoration: BoxDecoration(
                            color: widget.value ? palette.accent : palette.muted,
                            borderRadius: BorderRadius.zero,
                            border: Border.all(
                              color: widget.value ? palette.accent : palette.border,
                              width: 2.0,
                            ),
                          ),
                          child: Stack(
                            alignment: Alignment.center,
                            children: [
                              AnimatedPositioned(
                                duration: const Duration(milliseconds: 150),
                                curve: Curves.easeOutCubic,
                                left: widget.value ? 28 : 4,
                                child: Container(
                                  width: 20,
                                  height: 20,
                                  color: widget.value
                                      ? palette.accentForeground
                                      : palette.mutedForeground,
                                ),
                              ),
                            ],
                          ),
                        ),
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 20),
                Wrap(
                  spacing: 8,
                  runSpacing: 8,
                  children: widget.tags.map((tag) {
                    return Container(
                      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 4),
                      decoration: BoxDecoration(
                        color: widget.value
                            ? palette.muted
                            : palette.background,
                        borderRadius: BorderRadius.zero,
                        border: Border.all(
                          color: widget.value ? palette.borderLight : palette.border,
                          width: 1.0,
                        ),
                      ),
                      child: Text(
                        tag.toUpperCase(),
                        style: KineticTheme.labelSmall.copyWith(
                          color: widget.value ? palette.foreground : palette.mutedForeground,
                        ),
                      ),
                    );
                  }).toList(),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}
