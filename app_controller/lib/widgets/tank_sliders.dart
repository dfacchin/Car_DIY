import 'package:flutter/material.dart';

class TankSlidersWidget extends StatefulWidget {
  final void Function(int left, int right) onChanged;
  final VoidCallback onReleased;
  final int maxRpm;

  const TankSlidersWidget({
    super.key,
    required this.onChanged,
    required this.onReleased,
    this.maxRpm = 130,
  });

  @override
  State<TankSlidersWidget> createState() => _TankSlidersWidgetState();
}

class _TankSlidersWidgetState extends State<TankSlidersWidget> {
  double _leftValue = 0;   // -1 to +1
  double _rightValue = 0;  // -1 to +1

  @override
  Widget build(BuildContext context) {
    return Row(
      mainAxisAlignment: MainAxisAlignment.spaceEvenly,
      children: [
        _buildSlider('Left', _leftValue, (v) {
          setState(() => _leftValue = v);
          _emitValues();
        }, () {
          setState(() => _leftValue = 0);
          _emitValues();
          widget.onReleased();
        }),
        _buildSlider('Right', _rightValue, (v) {
          setState(() => _rightValue = v);
          _emitValues();
        }, () {
          setState(() => _rightValue = 0);
          _emitValues();
          widget.onReleased();
        }),
      ],
    );
  }

  void _emitValues() {
    widget.onChanged(
      (_leftValue * widget.maxRpm).round(),
      (_rightValue * widget.maxRpm).round(),
    );
  }

  Widget _buildSlider(
    String label,
    double value,
    ValueChanged<double> onChanged,
    VoidCallback onRelease,
  ) {
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        Text(label,
            style: const TextStyle(
                color: Color(0xFF4CC9F0), fontWeight: FontWeight.w600)),
        const SizedBox(height: 8),
        GestureDetector(
          onVerticalDragUpdate: (d) {
            final box = context.findRenderObject() as RenderBox?;
            if (box == null) return;
            // Convert drag to -1..+1 (up = positive)
            final delta = -d.delta.dy / 100;
            final newVal = (value + delta).clamp(-1.0, 1.0);
            onChanged(newVal);
          },
          onVerticalDragEnd: (_) => onRelease(),
          onVerticalDragCancel: onRelease,
          child: Container(
            width: 60,
            height: 220,
            decoration: BoxDecoration(
              color: const Color(0xFF2A2A4A),
              borderRadius: BorderRadius.circular(30),
              border: Border.all(color: const Color(0xFF4CC9F0), width: 2),
            ),
            child: CustomPaint(
              painter: _SliderPainter(value),
            ),
          ),
        ),
        const SizedBox(height: 8),
        Text(
          '${(value * widget.maxRpm).round()}',
          style: const TextStyle(
              fontSize: 18, fontWeight: FontWeight.bold, color: Colors.white),
        ),
      ],
    );
  }
}

class _SliderPainter extends CustomPainter {
  final double value; // -1 to +1

  _SliderPainter(this.value);

  @override
  void paint(Canvas canvas, Size size) {
    final centerY = size.height / 2;

    // Fill
    if (value != 0) {
      final fillHeight = (value.abs() * size.height / 2);
      final rect = value > 0
          ? Rect.fromLTRB(4, centerY - fillHeight, size.width - 4, centerY)
          : Rect.fromLTRB(4, centerY, size.width - 4, centerY + fillHeight);
      canvas.drawRRect(
        RRect.fromRectAndRadius(rect, const Radius.circular(4)),
        Paint()..color = const Color(0xFF4CC9F0).withOpacity(0.3),
      );
    }

    // Thumb
    final thumbY = centerY - (value * size.height / 2);
    canvas.drawRRect(
      RRect.fromRectAndRadius(
        Rect.fromCenter(center: Offset(size.width / 2, thumbY), width: 50, height: 20),
        const Radius.circular(10),
      ),
      Paint()..color = const Color(0xFF4CC9F0),
    );
  }

  @override
  bool shouldRepaint(covariant _SliderPainter old) => value != old.value;
}
