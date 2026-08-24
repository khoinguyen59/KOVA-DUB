---
name: audio-dsp-ffmpeg-pro
description: Audio processing, multi-track timeline, UVR5 vocal separation, FFmpeg filtergraphs, audio normalization, resamplers, and video muxing.
---

# Audio DSP & FFmpeg Media Engineering Skill

This skill provides industry best practices for audio digital signal processing (DSP), high-throughput media pipelines, multi-track alignment, and FFmpeg filtergraph construction.

---

## 1. Core Principles

### 1.1 Multi-Track Audio Management
- **Vocal & Background Separation**: UVR5 MDX-Net / Roformer to produce distinct 44.1kHz / 48kHz WAV streams.
- **Audio Mixing & Normalization**:
  - Voice ducking when speech is detected (`sidechaincompress` or dynamic EQ).
  - Peak limiting and EBU R128 loudness normalization (`-filter:a loudnorm=I=-16:LRA=11:TP=-1.5`).
- **Resampling**: Always resample audio cleanly using SOXR / high-quality polyphase resamplers before feeding into neural TTS models (e.g. 16kHz for Whisper/PP-OCR, 24kHz/48kHz for neural vocoders).

### 1.2 FFmpeg Filtergraph Construction & Subtitling
- **Hardsub Burn-in with Subtitle Masking**:
  ```bash
  ffmpeg -y -i input.mp4 -vf "drawbox=x=0:y=ih-120:w=iw:h=120:color=black@0.9:t=fill,subtitles='sub.srt':force_style='FontSize=22,PrimaryColour=&H00FFFFFF,Outline=1,Alignment=2,MarginV=25'" -c:v libx264 -preset slow -crf 18 -c:a aac -b:a 192k output.mp4
  ```
- **Precise Stream Mapping**:
  Always use explicit `-map` arguments to prevent FFmpeg from silently selecting the wrong audio track:
  ```bash
  ffmpeg -y -i video.mp4 -i dubbed_vocals.wav -map 0:v:0 -map 1:a:0 -c:v copy -c:a aac -b:a 192k -shortest output.mp4
  ```

---

## 2. Low-Latency Audio Streaming in C++
- Lock-free ring buffers for multi-threaded playback and real-time visualization.
- Separation of GUI thread (QML) and Audio Processing thread (Qt Multimedia / PortAudio / MiniAudio).
- Accurate timecode synchronization between video frames and audio sample positions ($ms \leftrightarrow samples$).
