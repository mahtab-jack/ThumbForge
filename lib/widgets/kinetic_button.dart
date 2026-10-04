import 'package:flutter/material.dart';
import '../theme/kinetic_theme.dart';

enum KineticButtonVariant {
  primary,
  outline,
  danger,
}

class KineticButton extends StatefulWidget {
  final String label;
  final VoidCallback? onPressed;
  final KineticButtonVariant variant;
  final bool isLoading;
  final double height;
  final IconData? icon;

  const KineticButton({
    super.key,
    required this.label,
    required this.onPressed,
    this.variant = KineticButtonVariant.primary,
    this.isLoading = false,
    this.height = 54.0,
    this.icon,
  });

  @override
  State<KineticButton> createState() => _KineticButtonState();
}

class _KineticButtonState extends State<KineticButton> {
  bool _isHovered = false;
  bool _isPressed = false;

  @override
  Widget build(BuildContext context) {
    final palette = KineticPalette.of(context);
    final enabled = widget.onPressed != null && !widget.isLoading;

    Color bg;
    Color fg;
    Border border;

    switch (widget.variant) {
      case KineticButtonVariant.primary:
        if (!enabled) {
          bg = palette.accent.withValues(alpha: 0.3);
          fg = palette.accentForeground.withValues(alpha: 0.5);
          border = Border.all(color: Colors.transparent, width: 2);
        } else if (_isHovered) {
          bg = const Color(0xFFEFF115); // Vibrant hover acid yellow
          fg = palette.accentForeground;
          border = Border.all(color: palette.foreground, width: 2);
        } else {
          bg = palette.accent;
          fg = palette.accentForeground;
          border = Border.all(color: palette.accent, width: 2);
        }
        break;

      case KineticButtonVariant.outline:
        if (!enabled) {
          bg = Colors.transparent;
          fg = palette.mutedForeground.withValues(alpha: 0.4);
          border = Border.all(color: palette.border.withValues(alpha: 0.4), width: 2);
        } else if (_isHovered) {
          bg = palette.foreground;
          fg = palette.background;
          border = Border.all(color: palette.foreground, width: 2);
        } else {
          bg = Colors.transparent;
          fg = palette.foreground;
          border = Border.all(color: palette.border, width: 2);
        }
        break;

      case KineticButtonVariant.danger:
        if (!enabled) {
          bg = Colors.transparent;
          fg = palette.mutedForeground.withValues(alpha: 0.4);
          border = Border.all(color: palette.border.withValues(alpha: 0.4), width: 2);
        } else if (_isHovered) {
          bg = KineticColors.error;
          fg = Colors.white;
          border = Border.all(color: KineticColors.error, width: 2);
        } else {
          bg = Colors.transparent;
          fg = palette.foreground;
          border = Border.all(color: palette.border, width: 2);
        }
        break;
    }

    final scale = _isPressed ? 0.97 : (_isHovered ? 1.01 : 1.0);

    return MouseRegion(
      cursor: enabled ? SystemMouseCursors.click : SystemMouseCursors.basic,
      onEnter: (_) => setState(() => _isHovered = true),
      onExit: (_) => setState(() => _isHovered = false),
      child: GestureDetector(
        onTapDown: enabled ? (_) => setState(() => _isPressed = true) : null,
        onTapUp: enabled ? (_) => setState(() => _isPressed = false) : null,
        onTapCancel: enabled ? () => setState(() => _isPressed = false) : null,
        onTap: enabled ? widget.onPressed : null,
        child: AnimatedScale(
          scale: scale,
          duration: const Duration(milliseconds: 100),
          curve: Curves.easeOutQuad,
          child: AnimatedContainer(
            duration: const Duration(milliseconds: 150),
            height: widget.height,
            padding: const EdgeInsets.symmetric(horizontal: 24.0),
            decoration: BoxDecoration(
              color: bg,
              borderRadius: BorderRadius.zero,
              border: border,
            ),
            child: Center(
              child: widget.isLoading
                  ? SizedBox(
                      width: 20,
                      height: 20,
                      child: CircularProgressIndicator(
                        strokeWidth: 2.5,
                        valueColor: AlwaysStoppedAnimation<Color>(fg),
                      ),
                    )
                  : Row(
                      mainAxisSize: MainAxisSize.min,
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        if (widget.icon != null) ...[
                          Icon(widget.icon, size: 16, color: fg),
                          const SizedBox(width: 8),
                        ],
                        Flexible(
                          child: Text(
                            widget.label.toUpperCase(),
                            maxLines: 1,
                            overflow: TextOverflow.ellipsis,
                            style: KineticTheme.buttonText.copyWith(
                              color: fg,
                              letterSpacing: 0.5,
                            ),
                          ),
                        ),
                      ],
                    ),
            ),
          ),
        ),
      ),
    );
  }
}
