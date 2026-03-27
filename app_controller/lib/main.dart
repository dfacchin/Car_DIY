import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'models/car_state.dart';
import 'screens/control_screen.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(const CarDiyApp());
}

class CarDiyApp extends StatelessWidget {
  const CarDiyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return ChangeNotifierProvider(
      create: (_) => CarState(),
      child: MaterialApp(
        title: 'Car DIY Controller',
        debugShowCheckedModeBanner: false,
        theme: ThemeData.dark(useMaterial3: true).copyWith(
          colorScheme: ColorScheme.fromSeed(
            seedColor: const Color(0xFF4CC9F0),
            brightness: Brightness.dark,
          ),
          scaffoldBackgroundColor: const Color(0xFF1A1A2E),
        ),
        home: const ControlScreen(),
      ),
    );
  }
}
