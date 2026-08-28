#!/usr/bin/env python3
"""Generate length-prefixed dummy pattern assets for esp_media_dummy_service.

Format: repeated [uint16_le frame_len][frame_bytes]

Environment:
  FFMPEG  Path to ffmpeg with libx264 (default: /home/tempo/ffmpeg/ffmpeg/ffmpeg)

Outputs:
  s.aac, s.opus, s.mp3, s.h264, s.mjpeg

Frame durations expected by media_dummy_pattern_frame_duration_ms()
(esp_media_frame_t PTS is milliseconds):
  AAC   1024 samples @ 16 kHz  -> 64 ms
  Opus  20 ms packets @ 48 kHz -> 20 ms
  MP3   MPEG-2 L3 576 @ 16 kHz -> 36 ms
  H264  -r 15                  -> 66 ms (1000/15)
  MJPEG -r 10                  -> 100 ms
  PCM   (generated in C)       -> 20 ms slices
  raw V (generated in C)       -> 1000/fps ms (default 10 -> 100)
"""

from __future__ import annotations

import ctypes
import math
import os
import struct
import subprocess
import tempfile
from ctypes import POINTER, c_int, c_short, c_ubyte, c_void_p
from pathlib import Path

FF = os.environ.get('FFMPEG', '/home/tempo/ffmpeg/ffmpeg/ffmpeg')
OUT = Path(__file__).resolve().parent
TMP = Path(tempfile.mkdtemp(prefix='dummy_pat_'))


def run(cmd):
    print('+', ' '.join(map(str, cmd)))
    subprocess.check_call(cmd)


def wrap(frames, path: Path):
    blob = bytearray()
    for fr in frames:
        if not fr:
            continue
        if len(fr) > 0xFFFF:
            raise SystemExit(f'frame too large {len(fr)}')
        blob += struct.pack('<H', len(fr))
        blob += fr
    path.write_bytes(blob)
    print(f'{path.name}: {len(blob)} bytes, {len(frames)} frames')


def split_adts(data: bytes):
    frames, i = [], 0
    while i + 7 <= len(data):
        if data[i] != 0xFF or (data[i + 1] & 0xF0) != 0xF0:
            i += 1
            continue
        fl = ((data[i + 3] & 0x03) << 11) | (data[i + 4] << 3) | ((data[i + 5] & 0xE0) >> 5)
        if fl < 7 or i + fl > len(data):
            break
        frames.append(data[i : i + fl])
        i += fl
    return frames


def split_opus_ogg(data: bytes):
    packets, i = [], 0
    while i + 27 <= len(data):
        if data[i : i + 4] != b'OggS':
            i += 1
            continue
        nseg = data[i + 26]
        segs = data[i + 27 : i + 27 + nseg]
        hdr = 27 + nseg
        body_len = sum(segs)
        body = data[i + hdr : i + hdr + body_len]
        pkt = bytearray()
        off = 0
        for s in segs:
            pkt += body[off : off + s]
            off += s
            if s < 255:
                if not (pkt.startswith(b'OpusHead') or pkt.startswith(b'OpusTags')):
                    packets.append(bytes(pkt))
                pkt = bytearray()
        i += hdr + body_len
    return packets


def _normalize_annexb_4byte(nal: bytes) -> bytes:
    """Force a single 4-byte start code (00 00 00 01).

    libx264 Annex-B mixes 3- and 4-byte start codes. Prefer uniform
    4-byte start codes — the common Annex-B form used by most tools.
    """
    if len(nal) >= 4 and nal[:4] == b'\x00\x00\x00\x01':
        return nal
    if len(nal) >= 3 and nal[:3] == b'\x00\x00\x01':
        return b'\x00\x00\x00\x01' + nal[3:]
    return b'\x00\x00\x00\x01' + nal


def split_h264(data: bytes):
    # Prefer 4-byte start codes so 00 00 00 01 is not split as 00 00 01.
    starts, i = [], 0
    while i + 3 < len(data):
        if i + 4 <= len(data) and data[i : i + 4] == b'\x00\x00\x00\x01':
            starts.append(i)
            i += 4
            continue
        if data[i : i + 3] == b'\x00\x00\x01':
            starts.append(i)
            i += 3
            continue
        i += 1

    def nal_type(nal):
        off = 4 if len(nal) >= 4 and nal[:4] == b'\x00\x00\x00\x01' else 3
        return nal[off] & 0x1F if off < len(nal) else 0

    frames, cur, has_vcl = [], bytearray(), False
    for si, st in enumerate(starts):
        end = starts[si + 1] if si + 1 < len(starts) else len(data)
        nal = _normalize_annexb_4byte(data[st:end])
        is_vcl = 1 <= nal_type(nal) <= 5
        if is_vcl and has_vcl:
            frames.append(bytes(cur))
            cur = bytearray(nal)
            has_vcl = True
        else:
            cur += nal
            has_vcl = has_vcl or is_vcl
    if cur:
        frames.append(bytes(cur))
    return frames


def split_jpeg(data: bytes):
    frames, i = [], 0
    while i + 1 < len(data):
        if data[i] == 0xFF and data[i + 1] == 0xD8:
            j = i + 2
            while j + 1 < len(data):
                if data[j] == 0xFF and data[j + 1] == 0xD9:
                    frames.append(data[i : j + 2])
                    i = j + 2
                    break
                j += 1
            else:
                break
        else:
            i += 1
    return frames


def split_mp3(data: bytes):
    frames, i = [], 0
    br_tbl = {
        3: [0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0],
        2: [0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0],
        0: [0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0],
    }
    sr_tbl = {3: [44100, 48000, 32000, 0], 2: [22050, 24000, 16000, 0], 0: [11025, 12000, 8000, 0]}
    while i + 4 <= len(data):
        if data[i] != 0xFF or (data[i + 1] & 0xE0) != 0xE0:
            i += 1
            continue
        ver = (data[i + 1] >> 3) & 0x03
        layer = (data[i + 1] >> 1) & 0x03
        br_idx = (data[i + 2] >> 4) & 0x0F
        sr_idx = (data[i + 2] >> 2) & 0x03
        pad = (data[i + 2] >> 1) & 0x01
        if layer != 1 or br_idx in (0, 15) or sr_idx == 3 or ver not in br_tbl:
            i += 1
            continue
        br = br_tbl[ver][br_idx] * 1000
        srate = sr_tbl[ver][sr_idx]
        if not br or not srate:
            i += 1
            continue
        fl = (144 * br // srate + pad) if ver == 3 else (72 * br // srate + pad)
        if fl < 4 or i + fl > len(data):
            break
        frames.append(data[i : i + fl])
        i += fl
    return frames


def make_bipop(sr=16000, freq1=660, freq2=880, pop_ms=12, gap_ms=40, tail_ms=20, amp=0.35):
    """Soft two-pop chirp (less harsh than a continuous 1 kHz tone)."""
    def pop(freq, n):
        out = []
        for i in range(n):
            # Fast attack, exponential decay envelope
            env = math.exp(-4.5 * i / max(n - 1, 1))
            s = amp * env * math.sin(2 * math.pi * freq * i / sr)
            out.append(int(32767 * s))
        return out

    pop_n = max(1, sr * pop_ms // 1000)
    gap_n = max(0, sr * gap_ms // 1000)
    tail_n = max(0, sr * tail_ms // 1000)
    samples = pop(freq1, pop_n) + [0] * gap_n + pop(freq2, pop_n) + [0] * tail_n
    pcm = bytearray()
    for s in samples:
        pcm += struct.pack('<h', max(-32767, min(32767, s)))
    path = TMP / 'bipop.pcm'
    path.write_bytes(pcm)
    return path, sr


def make_pcm(sr=16000):
    """Convert the first one-second sync.wav period to mono S16LE PCM."""
    sync_wav = OUT / 'sync.wav'
    if not sync_wav.exists():
        raise SystemExit(f'missing source pattern: {sync_wav}')
    path = TMP / 'sync_1s.pcm'
    run([FF, '-y', '-i', str(sync_wav), '-t', '1', '-ar', str(sr), '-ac', '1',
         '-f', 's16le', str(path)])
    return path, sr


def compact_audio(frames, signal_frames):
    """Keep initial signal packets and one final mute packet."""
    if len(frames) <= signal_frames:
        raise SystemExit(f'not enough encoded frames: {len(frames)}')
    return frames[:signal_frames] + [frames[-1]]


def gen_ball_rgb(w, h, frames=15, ball_r=20):
    out = []
    for fi in range(frames):
        t = fi / max(frames, 1)
        cx = w // 2
        cy = int(h / 2 + math.sin(2 * math.pi * t) * (h / 2 - ball_r - 4))
        buf = bytearray([255, 255, 255]) * (w * h)
        r2 = ball_r * ball_r
        for y in range(max(0, cy - ball_r), min(h, cy + ball_r + 1)):
            for x in range(max(0, cx - ball_r), min(w, cx + ball_r + 1)):
                if (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r2:
                    o = (y * w + x) * 3
                    buf[o] = 255
                    buf[o + 1] = 0
                    buf[o + 2] = 0
        out.append(bytes(buf))
    return out


def encode_mp3_lame(pcm_path, sr, out_mp3):
    lib = ctypes.CDLL('libmp3lame.so.0')
    lib.lame_init.restype = c_void_p
    lib.lame_set_in_samplerate.argtypes = [c_void_p, c_int]
    lib.lame_set_num_channels.argtypes = [c_void_p, c_int]
    lib.lame_set_brate.argtypes = [c_void_p, c_int]
    lib.lame_set_mode.argtypes = [c_void_p, c_int]
    lib.lame_init_params.argtypes = [c_void_p]
    lib.lame_encode_buffer.argtypes = [c_void_p, POINTER(c_short), POINTER(c_short), c_int, POINTER(c_ubyte), c_int]
    lib.lame_encode_buffer.restype = c_int
    lib.lame_encode_flush.argtypes = [c_void_p, POINTER(c_ubyte), c_int]
    lib.lame_encode_flush.restype = c_int
    lib.lame_close.argtypes = [c_void_p]
    gfp = lib.lame_init()
    lib.lame_set_in_samplerate(gfp, sr)
    lib.lame_set_num_channels(gfp, 1)
    lib.lame_set_brate(gfp, 32)
    lib.lame_set_mode(gfp, 3)
    if lib.lame_init_params(gfp) < 0:
        raise SystemExit('lame_init_params failed')
    samples = memoryview(pcm_path.read_bytes()).cast('h')
    out = bytearray()
    mp3buf = (c_ubyte * 8192)()
    chunk = 1152
    for i in range(0, len(samples), chunk):
        part = samples[i : i + chunk]
        arr = (c_short * len(part))(*part)
        n = lib.lame_encode_buffer(gfp, arr, arr, len(part), mp3buf, 8192)
        if n > 0:
            out += bytes(mp3buf[:n])
    n = lib.lame_encode_flush(gfp, mp3buf, 8192)
    if n > 0:
        out += bytes(mp3buf[:n])
    lib.lame_close(gfp)
    out_mp3.write_bytes(out)


def main():
    pcm_path, sr = make_pcm()

    aac_raw = TMP / 's.aac.raw'
    run([FF, '-y', '-f', 's16le', '-ar', str(sr), '-ac', '1', '-i', str(pcm_path),
         '-c:a', 'aac', '-b:a', '32k', '-f', 'adts', str(aac_raw)])
    wrap(compact_audio(split_adts(aac_raw.read_bytes()), 2), OUT / 's.aac')

    opus_ogg = TMP / 's.opus.ogg'
    run([FF, '-y', '-f', 's16le', '-ar', str(sr), '-ac', '1', '-i', str(pcm_path),
         '-ar', '48000', '-c:a', 'opus', '-b:a', '16k', '-strict', '-2', '-f', 'ogg', str(opus_ogg)])
    wrap(compact_audio(split_opus_ogg(opus_ogg.read_bytes()), 5), OUT / 's.opus')

    mp3_raw = TMP / 's.mp3.raw'
    encode_mp3_lame(pcm_path, sr, mp3_raw)
    wrap(compact_audio(split_mp3(mp3_raw.read_bytes()), 3), OUT / 's.mp3')

    frames = gen_ball_rgb(320, 240, 15, 20)
    raw = TMP / 'ball_h264.rgb'
    with open(raw, 'wb') as f:
        for fr in frames:
            f.write(fr)
    h264_raw = TMP / 's.h264.raw'
    run([FF, '-y', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-s', '320x240', '-r', '15', '-i', str(raw),
         '-c:v', 'libx264', '-profile:v', 'baseline', '-level', '3.0', '-bf', '0', '-g', '15',
         '-pix_fmt', 'yuv420p', '-f', 'h264', str(h264_raw)])
    wrap(split_h264(h264_raw.read_bytes()), OUT / 's.h264')

    frames = gen_ball_rgb(640, 480, 10, 36)
    raw = TMP / 'ball_m.rgb'
    with open(raw, 'wb') as f:
        for fr in frames:
            f.write(fr)
    mjpg = TMP / 's.mjpg'
    run([FF, '-y', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-s', '640x480', '-r', '10', '-i', str(raw),
         '-c:v', 'mjpeg', '-q:v', '5', '-f', 'mjpeg', str(mjpg)])
    wrap(split_jpeg(mjpg.read_bytes()), OUT / 's.mjpeg')

    print('generated in', OUT)


if __name__ == '__main__':
    main()
