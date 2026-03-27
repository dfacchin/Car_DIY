import 'dart:math';
import 'package:flutter/material.dart';

class JoystickWidget extends StatefulWidget {
  final double size;
  final void Function(double x, double y) onChanged;  // -1..+1 for each axis
  final VoidCallback onReleased;

  const JoystickWidget({
    super.key,
    this.size = 200,
    required this.onChanged,
    required this.onReleased,
  });

  @override
  State<JoystickWidget> createState() => _JoystickWidgetState();
}

class _JoystickWidgetState extends State<JoystickWidget> {
  double _dx = 0;
  double _dy = 0;

  double get _maxDist => widget.size / 2 - 30;

  void _updatePosition(Offset localPosition) {
    final center = Offset(widget.size / 2, widget.size / 2);
    var delta = localPosition - center;
    final dist = delta.distance;

    if (dist > _maxDist) {
      delta = delta / dist * _maxDist;
    }

    setState(() {
      _dx = delta.dx;
      _dy = delta.dy;
    });

    widget.onChanged(delta.dx / _maxDist, -delta.dy / _maxDist);
  }

  void _reset() {
    setState(() { _dx = 0; _dy = 0; });
    widget.onReleased();
  }

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onPanStart: (d) => _updatePosition(d.localPosition),
      onPanUpdate: (d) => _updatePosition(d.localPosition),
      onPanEnd: (_) => _reset(),
      onPanCancel: _reset,
      child: SizedBox(
        width: widget.size,
        height: widget.size,
        child: CustomPaint(
          painter: _JoystickPainter(_dx, _dy, widget.size),
        ),
      ),
    );
  }
}

class _JoystickPainter extends CustomPainter {
  final double dx, dy, size;

  _JoystickPainter(this.dx, this.dy, this.size);

  @override
  void paint(Canvas canvas, Size s) {
    final center = Offset(s.width / 2, s.height / 2);

    // Base circle
    canvas.drawCircle(
      center,
      size / 2 - 2,
      Paint()
        ..color = const Color(0xFF2A2A4A)
        ..style = PaintingStyle.fill,
    );
    canvas.drawCircle(
      center,
      size / 2 - 2,
      Paint()
        ..color = const Color(0xFF4CC9F0)
        ..style = PaintingStyle.stroke
        ..strokeWidth = 2,
    );

    // Crosshair
    final crossPaint = Paint()
      ..color = const Color(0xFF333355)
      ..strokeWidth = 1;
    canvas.drawLine(Offset(center.dx, 4), Offset(center.dx, s.height - 4), crossPaint);
    canvas.drawLine(Offset(4, center.dy), Offset(s.width - 4, center.dy), crossPaint);

    // Handle
    canvas.drawCircle(
      center + Offset(dx, dy),
      30,
      Paint()
        ..shader = RadialGradient(
          colors: [const Color(0xFF4CC9F0), const Color(0xFF3A86B4)],
        ).createShader(Rect.fromCircle(center: center + Offset(dx, dy), radius: 30)),
    );
  }

  @override
  bool shouldRepaint(covariant _JoystickPainter old) =>
      dx != old.dx || dy != old.dy;
}
