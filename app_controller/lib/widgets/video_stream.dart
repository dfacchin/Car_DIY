import 'package:flutter/material.dart';
import 'package:flutter_mjpeg/flutter_mjpeg.dart';

class VideoStreamWidget extends StatelessWidget {
  final String streamUrl;

  const VideoStreamWidget({super.key, required this.streamUrl});

  @override
  Widget build(BuildContext context) {
    return Container(
      color: Colors.black,
      child: Mjpeg(
        stream: streamUrl,
        isLive: true,
        fit: BoxFit.contain,
        error: (context, error, stack) {
          return const Center(
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
          );
        },
      ),
    );
  }
}
