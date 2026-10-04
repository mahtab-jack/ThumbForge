import 'dart:io';
import 'package:flutter/material.dart';
import '../theme/kinetic_theme.dart';

class MarqueeTicker extends StatefulWidget {
  final List<String> items;
  final double speed; // pixels per second

  const MarqueeTicker({
    super.key,
    required this.items,
    this.speed = 45.0,
  });

  @override
  State<MarqueeTicker> createState() => _MarqueeTickerState();
}

class _MarqueeTickerState extends State<MarqueeTicker> with SingleTickerProviderStateMixin {
  late final ScrollController _scrollController;
  late final AnimationController _animationController;

  @override
  void initState() {
    super.initState();
    _scrollController = ScrollController();
    _animationController = AnimationController(
      vsync: this,
      duration: const Duration(seconds: 40),
    );

    if (!Platform.environment.containsKey('FLUTTER_TEST')) {
      _animationController.addListener(_tick);
      WidgetsBinding.instance.addPostFrameCallback((_) {
        if (mounted) {
          _startScrolling();
        }
      });
    }
  }

  void _startScrolling() {
    _animationController.repeat();
  }

  void _tick() {
    if (!_scrollController.hasClients) return;
    final maxScroll = _scrollController.position.maxScrollExtent;
    if (maxScroll <= 0) return;

    final current = _scrollController.offset;
    final next = current + (widget.speed / 60.0);

    if (next >= maxScroll) {
      _scrollController.jumpTo(0);
    } else {
      _scrollController.jumpTo(next);
    }
  }

  @override
  void dispose() {
    _animationController.dispose();
    _scrollController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final palette = KineticPalette.of(context);
    final repeatedItems = <String>[];
    for (int i = 0; i < 6; i++) {
      repeatedItems.addAll(widget.items);
    }

    return Container(
      height: 38,
      decoration: BoxDecoration(
        color: palette.muted,
        border: Border(
          top: BorderSide(color: palette.border, width: 1.5),
          bottom: BorderSide(color: palette.border, width: 1.5),
        ),
      ),
      child: ListView.builder(
        controller: _scrollController,
        scrollDirection: Axis.horizontal,
        physics: const NeverScrollableScrollPhysics(),
        itemCount: repeatedItems.length,
        itemBuilder: (context, index) {
          final text = repeatedItems[index];
          return Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              Padding(
                padding: const EdgeInsets.symmetric(horizontal: 16.0),
                child: Text(
                  text.toUpperCase(),
                  style: KineticTheme.labelSmall.copyWith(
                    color: palette.foreground,
                    letterSpacing: 1.5,
                  ),
                ),
              ),
              Container(
                width: 5,
                height: 5,
                color: palette.accent,
              ),
            ],
          );
        },
      ),
    );
  }
}
