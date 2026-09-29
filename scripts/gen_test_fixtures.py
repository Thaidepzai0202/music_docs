#!/usr/bin/env python3
"""Sinh fixture WAV TỔNG HỢP cho scenario engine (không phải golden, tạo lại được bất cứ lúc nào).
   scripts/gen_test_fixtures.py   → engine/tests/fixtures/clip_*.wav (float32, 48 kHz)
Các file dùng bởi: launch_quantized_1bar, clip_loop_120, clip_repitch_100_to_120, solo_one_track, transport_stop_fade,
   record_*, monitor_*, overdub_*."""
import math, os, struct

SR = 48000
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "engine", "tests", "fixtures")

def write_wav(name, channels):
    """channels: list các list float (cùng độ dài). WAVE_FORMAT_IEEE_FLOAT 32-bit."""
    n = len(channels[0]); nch = len(channels)
    data = bytearray()
    for i in range(n):
        for c in channels:
            data += struct.pack("<f", c[i])
    fmt = struct.pack("<HHIIHH", 3, nch, SR, SR * nch * 4, nch * 4, 32)
    fact = struct.pack("<I", n)
    body = b"WAVE" + b"fmt " + struct.pack("<I", len(fmt)) + fmt + b"fact" + struct.pack("<I", 4) + fact \
         + b"data" + struct.pack("<I", len(data)) + bytes(data)
    path = os.path.join(OUT, name)
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", len(body)) + body)
    print(f"  {name}: {nch} kênh, {n} frame")

def clicks(beats, bpm):
    """Burst 1 kHz dài 10 ms ở đầu mỗi beat. Sample đầu tiên của burst KHÁC 0 (để kiểm onset đúng sample)."""
    spb = 60.0 * SR / bpm
    n = int(round(beats * spb)); x = [0.0] * n
    for k in range(beats):
        s0 = int(round(k * spb))
        for i in range(480):
            env = min(1.0, (i + 1) / 24.0) * (1.0 - i / 480.0)
            x[s0 + i] = 0.5 * env * math.sin(2 * math.pi * 1000.0 * (i + 1) / SR)
    return x

def main():
    os.makedirs(OUT, exist_ok=True)
    write_wav("clip_click_4beats_120.wav", [clicks(4, 120)])            # 96000 frame
    write_wav("clip_click_4beats_100.wav", [clicks(4, 100)])            # 115200 frame, phát ở 120 BPM → ×1.2
    write_wav("clip_silence_4beats_120.wav", [[0.0] * 96000])        # P1-22: overdub lên clip im lặng
    n = 96000   # 500 Hz × 2 s = 1000 chu kỳ nguyên → vòng liền mạch; L 0.5, R 0.25
    write_wav("clip_sine_4beats_120.wav", [[0.5 * math.sin(2 * math.pi * 500 * i / SR) for i in range(n)],
                                           [0.25 * math.sin(2 * math.pi * 500 * i / SR) for i in range(n)]])

if __name__ == "__main__":
    main()
