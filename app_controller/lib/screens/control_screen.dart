import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:provider/provider.dart';
import '../models/car_state.dart';
import '../services/car_api.dart';
import '../services/tilt_controller.dart';
import '../widgets/joystick.dart';
import '../widgets/tank_sliders.dart';
import '../widgets/video_stream.dart';

class ControlScreen extends StatefulWidget {
  const ControlScreen({super.key});

  @override
  State<ControlScreen> createState() => _ControlScreenState();
}

class _ControlScreenState extends State<ControlScreen> {
  late CarApi _api;
  TiltController? _tiltController;

  @override
  void initState() {
    super.initState();
    // Force landscape
    SystemChrome.setPreferredOrientations([
      DeviceOrientation.landscapeLeft,
      DeviceOrientation.landscapeRight,
    ]);
    SystemChrome.setEnabledSystemUIMode(SystemUiMode.immersiveSticky);

    final state = context.read<CarState>();
    _api = CarApi(state);
    _api.start();
  }

  @override
  void dispose() {
    _api.stop();
    _tiltController?.stop();
    SystemChrome.setPreferredOrientations(DeviceOrientation.values);
    SystemChrome.setEnabledSystemUIMode(SystemUiMode.edgeToEdge);
    super.dispose();
  }

  void _onControlModeChanged(ControlMode mode) {
    final state = context.read<CarState>();
    state.setControlMode(mode);

    // Start/stop tilt controller
    _tiltController?.stop();
    _tiltController = null;

    if (mode == ControlMode.tilt) {
      _tiltController = TiltController(
        state: state,
        onUpdate: (left, right) {
          state.setTargets(left, right);
        },
      );
      _tiltController!.start();
    }
  }

  void _onJoystickChanged(double x, double y) {
    final state = context.read<CarState>();
    const maxRpm = 130;
    final left = ((y + x) * maxRpm).round().clamp(-maxRpm, maxRpm);
    final right = ((y - x) * maxRpm).round().clamp(-maxRpm, maxRpm);
    state.setTargets(left, right);
  }

  void _onJoystickReleased() {
    final state = context.read<CarState>();
    state.setTargets(0, 0);
    _api.sendControl(0, 0);
  }

  void _onTankChanged(int left, int right) {
    context.read<CarState>().setTargets(left, right);
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xFF1A1A2E),
      body: Consumer<CarState>(
        builder: (context, state, _) {
          return Row(
            children: [
              // Video stream (left half)
              Expanded(
                flex: 3,
                child: Stack(
                  children: [
                    VideoStreamWidget(streamUrl: state.streamUrl),
                    // Status overlay
                    Positioned(
                      top: 8,
                      left: 8,
                      right: 8,
                      child: Row(
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,
                        children: [
                          Text(
                            state.connected ? 'Connected' : 'Disconnected',
                            style: TextStyle(
                              color: state.connected ? Colors.green : Colors.red,
                              fontSize: 12,
                              shadows: const [
                                Shadow(blurRadius: 4, color: Colors.black),
                              ],
                            ),
                          ),
                          Text(
                            'L: ${state.leftRpm} | R: ${state.rightRpm}',
                            style: const TextStyle(
                              color: Colors.green,
                              fontSize: 12,
                              shadows: [
                                Shadow(blurRadius: 4, color: Colors.black),
                              ],
                            ),
                          ),
                        ],
                      ),
                    ),
                  ],
                ),
              ),

              // Controls (right half)
              Expanded(
                flex: 2,
                child: Column(
                  children: [
                    // Mode selector
                    Padding(
                      padding: const EdgeInsets.all(8),
                      child: SegmentedButton<ControlMode>(
                        segments: const [
                          ButtonSegment(
                            value: ControlMode.joystick,
                            label: Text('Joystick'),
                            icon: Icon(Icons.gamepad),
                          ),
                          ButtonSegment(
                            value: ControlMode.tilt,
                            label: Text('Tilt'),
                            icon: Icon(Icons.screen_rotation),
                          ),
                          ButtonSegment(
                            value: ControlMode.tank,
                            label: Text('Tank'),
                            icon: Icon(Icons.linear_scale),
                          ),
                        ],
                        selected: {state.controlMode},
                        onSelectionChanged: (modes) {
                          _onControlModeChanged(modes.first);
                        },
                      ),
                    ),

                    // Control widget
                    Expanded(
                      child: Center(
                        child: _buildControlWidget(state.controlMode),
                      ),
                    ),

                    // Emergency stop
                    Padding(
                      padding: const EdgeInsets.all(8),
                      child: SizedBox(
                        width: double.infinity,
                        height: 50,
                        child: ElevatedButton(
                          onPressed: () => _api.sendStop(),
                          style: ElevatedButton.styleFrom(
                            backgroundColor: const Color(0xFFE63946),
                            foregroundColor: Colors.white,
                            textStyle: const TextStyle(
                              fontSize: 18,
                              fontWeight: FontWeight.w900,
                              letterSpacing: 4,
                            ),
                          ),
                          child: const Text('STOP'),
                        ),
                      ),
                    ),
                  ],
                ),
              ),
            ],
          );
        },
      ),
    );
  }

  Widget _buildControlWidget(ControlMode mode) {
    switch (mode) {
      case ControlMode.joystick:
        return JoystickWidget(
          onChanged: _onJoystickChanged,
          onReleased: _onJoystickReleased,
        );
      case ControlMode.tilt:
        return const Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(Icons.screen_rotation, size: 64, color: Color(0xFF4CC9F0)),
            SizedBox(height: 16),
            Text(
              'Tilt phone to drive',
              style: TextStyle(color: Colors.white70, fontSize: 16),
            ),
            SizedBox(height: 8),
            Text(
              'Forward/Back = Throttle\nLeft/Right = Steering',
              textAlign: TextAlign.center,
              style: TextStyle(color: Colors.white38, fontSize: 12),
            ),
          ],
        );
      case ControlMode.tank:
        return TankSlidersWidget(
          onChanged: _onTankChanged,
          onReleased: _onJoystickReleased,
        );
    }
  }
}
