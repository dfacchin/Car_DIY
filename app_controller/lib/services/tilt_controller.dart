import 'dart:async';
import 'dart:math';
import 'package:sensors_plus/sensors_plus.dart';
import '../models/car_state.dart';

/// Converts phone tilt (accelerometer) into motor RPM commands.
///
/// Tilt forward = both motors forward (throttle)
/// Tilt sideways = differential steering
class TiltController {
  final CarState state;
  final void Function(int left, int right) onUpdate;

  StreamSubscription? _subscription;

  static const int maxRpm = 130;
  static const double deadZone = 1.5;  // m/s^2 - ignore small tilts
  static const double maxTilt = 7.0;   // m/s^2 - full throttle/steering

  TiltController({required this.state, required this.onUpdate});

  void start() {
    _subscription = accelerometerEventStream(
      samplingPeriod: const Duration(milliseconds: 50),
    ).listen((event) {
      // Phone held landscape:
      // event.x = tilt left/right (steering)
      // event.y = tilt forward/back (throttle)

      double throttle = _applyDeadzone(-event.y);  // Forward tilt = negative Y
      double steering = _applyDeadzone(event.x);

      // Normalize to -1..+1
      throttle = (throttle / maxTilt).clamp(-1.0, 1.0);
      steering = (steering / maxTilt).clamp(-1.0, 1.0);

      // Differential drive mixing
      int left = (maxRpm * (throttle + steering)).round().clamp(-maxRpm, maxRpm);
      int right = (maxRpm * (throttle - steering)).round().clamp(-maxRpm, maxRpm);

      onUpdate(left, right);
    });
  }

  void stop() {
    _subscription?.cancel();
    _subscription = null;
    onUpdate(0, 0);
  }

  double _applyDeadzone(double value) {
    if (value.abs() < deadZone) return 0.0;
    // Remove deadzone offset for smooth response
    double sign = value >= 0 ? 1.0 : -1.0;
    return sign * (value.abs() - deadZone);
  }
}
