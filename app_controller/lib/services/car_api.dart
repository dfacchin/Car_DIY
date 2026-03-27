import 'dart:async';
import 'dart:convert';
import 'package:http/http.dart' as http;
import '../models/car_state.dart';

class CarApi {
  final CarState state;
  Timer? _statusTimer;
  Timer? _commandTimer;

  static const int maxRpm = 130;
  static const Duration cmdInterval = Duration(milliseconds: 50);
  static const Duration statusInterval = Duration(milliseconds: 200);

  CarApi(this.state);

  String get _baseUrl => 'http://${state.host}';

  /// Start periodic status polling and command sending
  void start() {
    _statusTimer = Timer.periodic(statusInterval, (_) => fetchStatus());
    _commandTimer = Timer.periodic(cmdInterval, (_) {
      if (state.targetLeft != 0 || state.targetRight != 0) {
        sendControl(state.targetLeft, state.targetRight);
      }
    });
  }

  /// Stop all periodic tasks
  void stop() {
    _statusTimer?.cancel();
    _commandTimer?.cancel();
  }

  /// Send motor control command
  Future<void> sendControl(int left, int right) async {
    left = left.clamp(-maxRpm, maxRpm);
    right = right.clamp(-maxRpm, maxRpm);
    try {
      await http.post(
        Uri.parse('$_baseUrl/api/control?left=$left&right=$right'),
      ).timeout(const Duration(seconds: 1));
    } catch (_) {}
  }

  /// Send emergency stop
  Future<void> sendStop() async {
    state.setTargets(0, 0);
    try {
      await http.post(
        Uri.parse('$_baseUrl/api/stop'),
      ).timeout(const Duration(seconds: 1));
    } catch (_) {}
  }

  /// Fetch current motor status
  Future<void> fetchStatus() async {
    try {
      final res = await http.get(
        Uri.parse('$_baseUrl/api/status'),
      ).timeout(const Duration(seconds: 1));

      if (res.statusCode == 200) {
        final data = json.decode(res.body);
        state.updateStatus(
          data['left_rpm'] as int,
          data['right_rpm'] as int,
          data['error'] as int,
        );
      }
    } catch (_) {
      state.setConnected(false);
    }
  }
}
