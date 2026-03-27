# Car DIY Controller - Flutter App

Smartphone controller for the Car DIY RC car.

## Features

- **Joystick mode** - Virtual joystick with differential drive mixing
- **Tilt mode** - Accelerometer-based steering (tilt forward/back for throttle, left/right for steering)
- **Tank mode** - Dual vertical sliders for independent motor control
- **Live video** - MJPEG stream from ESP32-CAM
- **Emergency stop** - Always visible stop button

## Setup

1. Connect your phone to the `CarDIY` WiFi network (password: `cardiy1234`)
2. Open the app
3. The app connects to `192.168.4.1` (ESP32-CAM default AP IP)

## Build

```bash
flutter pub get
flutter run
```

## Dependencies

- `sensors_plus` - Accelerometer for tilt control
- `flutter_mjpeg` - MJPEG video stream viewer
- `provider` - State management
- `http` - HTTP API calls
