import 'package:flutter/foundation.dart';

enum ControlMode { joystick, tilt, tank }

class CarState extends ChangeNotifier {
  String _host = '192.168.4.1';
  bool _connected = false;
  int _leftRpm = 0;
  int _rightRpm = 0;
  int _targetLeft = 0;
  int _targetRight = 0;
  int _error = 0;
  ControlMode _controlMode = ControlMode.joystick;

  String get host => _host;
  bool get connected => _connected;
  int get leftRpm => _leftRpm;
  int get rightRpm => _rightRpm;
  int get targetLeft => _targetLeft;
  int get targetRight => _targetRight;
  int get error => _error;
  ControlMode get controlMode => _controlMode;
  String get streamUrl => 'http://$_host:81/stream';

  void setHost(String host) {
    _host = host;
    notifyListeners();
  }

  void setConnected(bool connected) {
    _connected = connected;
    notifyListeners();
  }

  void updateStatus(int leftRpm, int rightRpm, int error) {
    _leftRpm = leftRpm;
    _rightRpm = rightRpm;
    _error = error;
    _connected = true;
    notifyListeners();
  }

  void setTargets(int left, int right) {
    _targetLeft = left;
    _targetRight = right;
    notifyListeners();
  }

  void setControlMode(ControlMode mode) {
    _controlMode = mode;
    notifyListeners();
  }
}
