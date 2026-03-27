import 'dart:async';
import 'dart:typed_data';
import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;

/// Simple MJPEG stream viewer that parses multipart JPEG frames.
class VideoStreamWidget extends StatefulWidget {
  final String streamUrl;

  const VideoStreamWidget({super.key, required this.streamUrl});

  @override
  State<VideoStreamWidget> createState() => _VideoStreamWidgetState();
}

class _VideoStreamWidgetState extends State<VideoStreamWidget> {
  Uint8List? _currentFrame;
  StreamSubscription? _subscription;
  bool _connected = false;

  @override
  void initState() {
    super.initState();
    _connect();
  }

  @override
  void dispose() {
    _subscription?.cancel();
    super.dispose();
  }

  Future<void> _connect() async {
    try {
      final client = http.Client();
      final request = http.Request('GET', Uri.parse(widget.streamUrl));
      final response = await client.send(request);

      if (response.statusCode != 200) {
        _scheduleReconnect();
        return;
      }

      setState(() => _connected = true);

      // Parse MJPEG multipart stream
      final buffer = BytesBuilder();
      bool inFrame = false;

      _subscription = response.stream.listen(
        (chunk) {
          for (int i = 0; i < chunk.length - 1; i++) {
            if (chunk[i] == 0xFF && chunk[i + 1] == 0xD8) {
              // JPEG start marker
              buffer.clear();
              inFrame = true;
            }
            if (inFrame) {
              buffer.addByte(chunk[i]);
            }
            if (inFrame && chunk[i] == 0xFF && chunk[i + 1] == 0xD9) {
              // JPEG end marker
              buffer.addByte(chunk[i + 1]);
              i++; // skip next byte
              if (mounted) {
                setState(() {
                  _currentFrame = buffer.toBytes();
                });
              }
              buffer.clear();
              inFrame = false;
            }
          }
          // Handle last byte if in frame
          if (inFrame && chunk.isNotEmpty) {
            buffer.addByte(chunk.last);
          }
        },
        onError: (_) {
          setState(() => _connected = false);
          _scheduleReconnect();
        },
        onDone: () {
          setState(() => _connected = false);
          _scheduleReconnect();
        },
      );
    } catch (_) {
      _scheduleReconnect();
    }
  }

  void _scheduleReconnect() {
    Future.delayed(const Duration(seconds: 2), () {
      if (mounted) _connect();
    });
  }

  @override
  Widget build(BuildContext context) {
    return Container(
      color: Colors.black,
      child: _currentFrame != null
          ? Image.memory(
              _currentFrame!,
              fit: BoxFit.contain,
              gaplessPlayback: true, // Prevents flicker between frames
            )
          : const Center(
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Icon(Icons.videocam_off, color: Colors.white54, size: 48),
                  SizedBox(height: 8),
                  Text(
                    'No video signal',
                    style: TextStyle(color: Colors.white54),
                  ),
                ],
              ),
            ),
    );
  }
}
