#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""WebSocket/WSS audio test tool for esp_audio_analyzer_app.

Provides record / play / duplex test cases over a ws:// or wss:// endpoint.
CLI options map one-to-one with device-side commands for easy troubleshooting.

Path convention (auto-dispatched by is_device_path):
  - Paths starting with `/sdcard/` or `embed://` are treated as device-side.
  - All other paths refer to local files on the host.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import os
import signal
import ssl
import struct
import traceback
import wave
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable
from urllib.parse import urlparse

import websockets


DEVICE_PATH_PREFIXES: tuple[str, ...] = ('/sdcard/', 'embed://')


def is_device_path(path: str) -> bool:
    """Return True if `path` refers to a file on the device (SD card / embed flash)."""
    return path.startswith(DEVICE_PATH_PREFIXES)


@dataclass
class AudioConfig:
    """Audio parameters matching the device `configure` command."""

    sample_rate: int = 16000
    bits_per_sample: int = 16
    channels: int = 2
    mic_gain: int | None = 40
    volume: int | None = 70
    channel_type: str | None = 'MMNR'

    def to_params(self) -> dict[str, Any]:
        params: dict[str, Any] = {
            'sample_rate': self.sample_rate,
            'bits_per_sample': self.bits_per_sample,
            'channels': self.channels,
        }
        if self.mic_gain is not None:
            params['mic_gain'] = self.mic_gain
        if self.volume is not None:
            params['volume'] = self.volume
        if self.channel_type is not None:
            params['channel_type'] = self.channel_type
        return params

    def with_wave(self, wav_path: str | Path) -> 'AudioConfig':
        """Return a copy with sample params overridden by a local WAV header."""
        with wave.open(str(wav_path), 'rb') as wav:
            return AudioConfig(
                sample_rate=wav.getframerate(),
                bits_per_sample=wav.getsampwidth() * 8,
                channels=wav.getnchannels(),
                mic_gain=self.mic_gain,
                volume=self.volume,
                channel_type=self.channel_type,
            )


class WebSocketAudioClient:
    """WebSocket/WSS client for protocol exchange and audio streaming."""

    DEFAULT_CHUNK_SIZE = 1024
    PROBE_BUFFER_LIMIT = 8192

    def __init__(self, url: str, connect_timeout: float = 10.0) -> None:
        self.url = url
        self.connect_timeout = connect_timeout

        self.ws: websockets.WebSocketClientProtocol | None = None
        self.should_stop = False
        self.recording = False
        self.playing = False
        self.duplex = False

        self._wav: wave.Wave_write | None = None
        self._rx_header_checked = False
        self._rx_probe_buffer = bytearray()

        self._progress_active = False
        self._section_emitted = False

    # ---- logging helpers -------------------------------------------------

    @staticmethod
    def _compact_json(obj: Any) -> str:
        return json.dumps(obj, ensure_ascii=False, separators=(',', ':'))

    def _emit(self, line: str = '') -> None:
        """Print one line; terminate any pending progress (\\r) line first."""
        if self._progress_active:
            print()
            self._progress_active = False
        print(line)

    def _log_section(self, title: str, width: int = 60) -> None:
        if self._section_emitted:
            self._emit()
        prefix = f'── {title} '
        self._emit(prefix + '─' * max(width - len(prefix), 3))
        self._section_emitted = True

    def _log_kv(self, key: str, value: Any, width: int = 10) -> None:
        self._emit(f'  {key:<{width}} {value}')

    def _log_send(self, payload: Any) -> None:
        self._emit(f'>> {self._compact_json(payload)}')

    def _log_recv(self, data: Any) -> None:
        if isinstance(data, (dict, list)):
            self._emit(f'<< {self._compact_json(data)}')
        else:
            self._emit(f'<< {data}')

    def _log_note(self, msg: str) -> None:
        self._emit(f'* {msg}')

    def _log_progress(self, text: str) -> None:
        print(f'\r  {text}', end='', flush=True)
        self._progress_active = True

    # ---- transport -------------------------------------------------------

    async def _connect_with_options(
        self,
        ssl_context: ssl.SSLContext | None = None,
    ) -> websockets.WebSocketClientProtocol:
        # Disable proxy to avoid local proxy interfering with device traffic.
        # The `proxy` kwarg is only supported on newer websockets versions.
        kwargs: dict[str, Any] = {'proxy': None}
        if ssl_context is not None:
            kwargs['ssl'] = ssl_context

        try:
            return await asyncio.wait_for(
                websockets.connect(self.url, **kwargs),
                timeout=self.connect_timeout,
            )
        except TypeError:
            kwargs.pop('proxy', None)
            return await asyncio.wait_for(
                websockets.connect(self.url, **kwargs),
                timeout=self.connect_timeout,
            )

    async def connect(self) -> None:
        """Establish WS/WSS connection and consume the optional welcome frame."""
        self._log_section('connect')
        self._log_kv('url', self.url)
        parsed = urlparse(self.url)
        try:
            self.ws = await self._connect_with_options()
        except ssl.SSLCertVerificationError as exc:
            if parsed.scheme != 'wss':
                raise
            # Fall back to no-verify for self-signed certificates (test only).
            self._log_note(f'WSS cert verify failed: {exc}')
            self._log_note('retrying insecure (test only)')
            insecure_ctx = ssl.create_default_context()
            insecure_ctx.check_hostname = False
            insecure_ctx.verify_mode = ssl.CERT_NONE
            self.ws = await self._connect_with_options(ssl_context=insecure_ctx)
            self._log_kv('tls', 'no-verify (self-signed)')

        try:
            welcome_raw = await asyncio.wait_for(self.ws.recv(), timeout=3.0)
        except asyncio.TimeoutError:
            self._log_note('no welcome frame (continuing)')
            return
        try:
            welcome: Any = json.loads(welcome_raw)
        except (json.JSONDecodeError, TypeError):
            welcome = welcome_raw
        self._log_recv(welcome)

    async def send_command(self, command: str, params: dict[str, Any] | None = None) -> None:
        """Send a JSON command to the device."""
        if self.ws is None:
            raise RuntimeError('WebSocket is not connected')

        payload: dict[str, Any] = {'command': command}
        if params:
            payload['params'] = params
        self._log_send(payload)
        await self.ws.send(self._compact_json(payload))

    async def receive_message(self) -> tuple[str, Any]:
        """Receive one frame; binary frames are routed to the WAV writer."""
        if self.ws is None:
            raise RuntimeError('WebSocket is not connected')

        msg = await self.ws.recv()
        if isinstance(msg, bytes):
            if self._wav is not None:
                self._write_rx_audio_chunk(msg)
            return 'binary', len(msg)

        try:
            data = json.loads(msg)
        except json.JSONDecodeError:
            data = msg

        self._log_recv(data)
        if isinstance(data, dict) and data.get('status') == 'error':
            self.should_stop = True

        return 'text', data

    async def _recv_text_message(self, timeout: float = 5.0) -> Any:
        """Wait for the next text frame, skipping binary frames in between."""
        loop = asyncio.get_running_loop()
        deadline = loop.time() + timeout

        while True:
            remain = deadline - loop.time()
            if remain <= 0:
                raise TimeoutError('Timeout waiting for text message')
            _type, data = await asyncio.wait_for(self.receive_message(), timeout=remain)
            if _type == 'text':
                return data

    async def _configure(self, cfg: AudioConfig) -> Any:
        await self.send_command('configure', cfg.to_params())
        return await self._recv_text_message()

    async def _wait_duration_or_stop(self, duration_s: float, sleep_s: float = 0.1) -> None:
        """Sleep up to duration_s seconds, honoring Ctrl+C early exit."""
        loop = asyncio.get_running_loop()
        start = loop.time()
        while not self.should_stop:
            if loop.time() - start >= duration_s:
                break
            await asyncio.sleep(sleep_s)

    def _open_wav_writer(self, output_file: str, cfg: AudioConfig) -> None:
        wav = wave.open(output_file, 'wb')
        wav.setnchannels(cfg.channels)
        wav.setsampwidth(cfg.bits_per_sample // 8)
        wav.setframerate(cfg.sample_rate)
        self._wav = wav
        self._rx_header_checked = False
        self._rx_probe_buffer.clear()

    @staticmethod
    def _find_wav_data_offset(raw: bytes) -> int | None:
        """Return the offset of the WAVE `data` chunk payload, or None if unknown yet."""
        if len(raw) < 12 or raw[0:4] != b'RIFF' or raw[8:12] != b'WAVE':
            return None

        offset = 12
        total = len(raw)
        while True:
            if offset + 8 > total:
                return None
            chunk_id = raw[offset : offset + 4]
            chunk_size = struct.unpack_from('<I', raw, offset + 4)[0]
            data_start = offset + 8

            if chunk_id == b'data':
                return data_start if data_start <= total else None

            padded_end = data_start + chunk_size + (chunk_size & 1)
            if padded_end > total:
                return None
            offset = padded_end

    def _write_rx_audio_chunk(self, chunk: bytes) -> None:
        """Accept raw PCM or WAV-wrapped streams from the device."""
        if self._wav is None:
            return
        if self._rx_header_checked:
            self._wav.writeframes(chunk)
            return

        self._rx_probe_buffer.extend(chunk)
        probe = bytes(self._rx_probe_buffer)

        # Common case: device sends raw PCM without a RIFF/WAVE header.
        if len(probe) >= 12 and not (probe.startswith(b'RIFF') and probe[8:12] == b'WAVE'):
            self._rx_header_checked = True
            self._wav.writeframes(probe)
            self._rx_probe_buffer.clear()
            return

        # If it is WAV-wrapped, strip the header and write only the payload.
        data_offset = self._find_wav_data_offset(probe)
        if data_offset is not None:
            self._rx_header_checked = True
            payload = probe[data_offset:]
            if payload:
                self._wav.writeframes(payload)
            self._rx_probe_buffer.clear()
            self._log_note('detected WAV header in RX stream, stripped')
            return

        # Bound the probe buffer so a malformed header cannot stall recording.
        if len(self._rx_probe_buffer) >= self.PROBE_BUFFER_LIMIT:
            self._rx_header_checked = True
            self._wav.writeframes(bytes(self._rx_probe_buffer))
            self._rx_probe_buffer.clear()
            self._log_note('WAV header probe limit exceeded, writing as raw PCM')

    def _close_wav_writer(self) -> None:
        # Flush any pending probe bytes so short recordings are not lost.
        if self._wav is not None and self._rx_probe_buffer:
            probe = bytes(self._rx_probe_buffer)
            data_offset = self._find_wav_data_offset(probe)
            if data_offset is not None:
                payload = probe[data_offset:]
                if payload:
                    self._wav.writeframes(payload)
            else:
                self._wav.writeframes(probe)
            self._rx_probe_buffer.clear()

        if self._wav is not None:
            self._wav.close()
        self._wav = None
        self._rx_header_checked = False

    async def _stop_command(self, command: str) -> None:
        await self.send_command(command)
        try:
            await self._recv_text_message(timeout=5.0)
        except TimeoutError:
            self._log_note(f'{command}: response timeout')

    async def stop_current_operation(self) -> None:
        """Stop any active streaming operation before closing."""
        if self.duplex:
            await self._stop_command('stop_duplex')
            self.duplex = False
        elif self.recording:
            await self._stop_command('stop_recording')
            self.recording = False
        elif self.playing:
            await self._stop_command('stop_playback')
            self.playing = False

    async def _send_audio_file(
        self,
        input_file: str | Path,
        chunk_size: int,
        bytes_per_second: int,
        on_progress: Callable[[int], None] | None = None,
    ) -> int:
        """Stream raw bytes from a local file to the device at real-time pace.

        Note: sends the file verbatim, including any RIFF/WAVE header; the
        device is responsible for parsing or skipping the header.
        """
        if self.ws is None:
            raise RuntimeError('WebSocket is not connected')

        sleep_per_chunk = chunk_size / bytes_per_second if bytes_per_second else 0.0
        total_sent = 0
        with open(input_file, 'rb') as f:
            while not self.should_stop:
                chunk = f.read(chunk_size)
                if not chunk:
                    break
                await self.ws.send(chunk)
                total_sent += len(chunk)
                if on_progress:
                    on_progress(total_sent)
                if sleep_per_chunk > 0:
                    await asyncio.sleep(sleep_per_chunk)
        return total_sent

    async def start_recording(self, path: str, duration: int, cfg: AudioConfig) -> None:
        """Unified recording entry: auto-dispatches based on path location."""
        if path.startswith('embed://'):
            raise ValueError('embed:// is read-only and cannot be used as a recording target')
        if is_device_path(path):
            await self._record_device(path, duration, cfg)
        else:
            await self._record_local(path, duration, cfg)

    async def _record_device(self, file_path: str, duration: int, cfg: AudioConfig) -> None:
        """Ask the device to record audio to its own storage (e.g. /sdcard/foo.wav)."""
        self._log_section('record')
        self._log_kv('target', f'device  {file_path}')
        self._log_kv(
            'params',
            f'{cfg.sample_rate}Hz/{cfg.bits_per_sample}bit/{cfg.channels}ch  '
            f'gain={cfg.mic_gain} vol={cfg.volume} {cfg.channel_type}',
        )
        self._log_kv('duration', f'{duration}s')

        await self._configure(cfg)
        await self.send_command('start_recording', {'file_path': file_path})
        await self._recv_text_message()

        self.recording = True
        await self._wait_duration_or_stop(duration, sleep_s=0.2)
        if self.recording:
            await self._stop_command('stop_recording')
            self.recording = False

        self._log_section('done')
        self._log_kv('record', f'device  {file_path}')

    async def _record_local(self, output_file: str, duration: int, cfg: AudioConfig) -> None:
        """Record audio to a local WAV file by consuming WS frames from the device."""
        self._log_section('record')
        self._log_kv('target', f'local   {output_file}')
        self._log_kv(
            'params',
            f'{cfg.sample_rate}Hz/{cfg.bits_per_sample}bit/{cfg.channels}ch  '
            f'gain={cfg.mic_gain} vol={cfg.volume} {cfg.channel_type}',
        )
        self._log_kv('duration', f'{duration}s')

        self._open_wav_writer(output_file, cfg)
        total_bytes = 0
        try:
            await self._configure(cfg)
            await self.send_command('start_recording')
            await self._recv_text_message()

            self.recording = True
            loop = asyncio.get_running_loop()
            start = loop.time()
            while self.recording and not self.should_stop:
                if loop.time() - start >= duration:
                    break
                try:
                    msg_type, data = await asyncio.wait_for(self.receive_message(), timeout=1.0)
                except asyncio.TimeoutError:
                    continue
                if msg_type == 'binary':
                    total_bytes += int(data)
                    self._log_progress(f'RX={total_bytes} B')

            if self.recording:
                await self._stop_command('stop_recording')
                self.recording = False
        finally:
            self._close_wav_writer()

        self._log_section('done')
        self._log_kv('record', output_file)
        self._log_kv('RX total', f'{total_bytes} B')

    async def start_playback(self, path: str, cfg: AudioConfig) -> None:
        """Unified playback entry: auto-dispatches based on path location."""
        if is_device_path(path):
            await self._playback_device(path, cfg)
        else:
            await self._playback_local(path, cfg)

    async def _playback_local(self, input_file: str, cfg: AudioConfig) -> None:
        """Stream a local WAV file to the device (full file including header)."""
        in_path = Path(input_file)
        if not in_path.exists():
            self._log_note(f'local playback file not found: {input_file}')
            return
        play_cfg = cfg.with_wave(in_path)

        self._log_section('play')
        self._log_kv('source', f'local   {input_file}')
        self._log_kv(
            'params',
            f'{play_cfg.sample_rate}Hz/{play_cfg.bits_per_sample}bit/{play_cfg.channels}ch  '
            f'gain={play_cfg.mic_gain} vol={play_cfg.volume} {play_cfg.channel_type}',
        )

        await self._configure(play_cfg)
        await self.send_command('start_playback')
        await self._recv_text_message()

        # Pace the send rate at the real-time playback rate. Over WSS each
        # frame incurs an extra TLS cost; sending too fast can saturate the
        # device and trigger flow-control messages that we do not consume.
        bytes_per_second = (
            play_cfg.sample_rate * (play_cfg.bits_per_sample // 8) * play_cfg.channels
        )
        chunk_size = self.DEFAULT_CHUNK_SIZE
        # Guard against malformed WAV headers that would collapse the denominator.
        sleep_per_chunk = chunk_size / max(bytes_per_second, 1)
        self._log_kv(
            'tx rate',
            f'{bytes_per_second // 1024} KB/s  chunk={sleep_per_chunk * 1000:.1f}ms',
        )

        self.playing = True
        total_sent = await self._send_audio_file(
            in_path,
            chunk_size,
            bytes_per_second,
            on_progress=lambda n: self._log_progress(f'TX={n} B'),
        )

        await asyncio.sleep(1.0)
        if self.playing:
            await self._stop_command('stop_playback')
            self.playing = False

        self._log_section('done')
        self._log_kv('TX total', f'{total_sent} B')

    async def _playback_device(self, file_path: str, cfg: AudioConfig) -> None:
        """Ask the device to play a file from its own storage or embedded flash."""
        self._log_section('play')
        self._log_kv('source', f'device  {file_path}')
        self._log_kv(
            'params',
            f'{cfg.sample_rate}Hz/{cfg.bits_per_sample}bit/{cfg.channels}ch  '
            f'gain={cfg.mic_gain} vol={cfg.volume} {cfg.channel_type}',
        )

        await self._configure(cfg)
        await self.send_command('start_playback', {'file_path': file_path})
        await self._recv_text_message()

        self.playing = True
        while self.playing and not self.should_stop:
            try:
                msg_type, data = await asyncio.wait_for(self.receive_message(), timeout=1.0)
            except asyncio.TimeoutError:
                continue
            if msg_type == 'text':
                if isinstance(data, dict) and data.get('status') == 'playing_stopped':
                    break

        if self.should_stop and self.playing:
            await self._stop_command('stop_playback')
        self.playing = False

        self._log_section('done')
        self._log_kv('source', f'device  {file_path}')

    async def run_duplex(
        self,
        play_path: str,
        record_path: str,
        duration: int,
        cfg: AudioConfig,
        enable_afe: bool = True,
    ) -> None:
        """Run a duplex session; play/record endpoints are auto-dispatched by path."""
        if record_path.startswith('embed://'):
            raise ValueError('embed:// is read-only and cannot be used as a recording target')

        need_send = not is_device_path(play_path)
        need_recv = not is_device_path(record_path)

        if need_send and not Path(play_path).exists():
            self._log_note(f'duplex playback file not found: {play_path}')
            return

        self._log_section('duplex')
        self._log_kv('play', f"{'local ' if need_send else 'device'}  {play_path}")
        self._log_kv('record', f"{'local ' if need_recv else 'device'}  {record_path}")
        self._log_kv(
            'params',
            f'{cfg.sample_rate}Hz/{cfg.bits_per_sample}bit/{cfg.channels}ch  '
            f'gain={cfg.mic_gain} vol={cfg.volume} {cfg.channel_type}  '
            f'afe={"on" if enable_afe else "off"}',
        )
        self._log_kv('duration', f'{duration}s')

        rsp = await self._configure(cfg)

        # Honor the sample params echoed by the device, in case they were clipped.
        sample_rate_rsp = cfg.sample_rate
        bits_rsp = cfg.bits_per_sample
        channels_rsp = cfg.channels
        if isinstance(rsp, dict):
            params = rsp.get('params')
            if isinstance(params, dict):
                sample_rate_rsp = params.get('sample_rate', sample_rate_rsp)
                bits_rsp = params.get('bits_per_sample', bits_rsp)
                channels_rsp = params.get('channels', channels_rsp)

        if need_recv:
            self._open_wav_writer(
                record_path,
                AudioConfig(
                    sample_rate=sample_rate_rsp,
                    bits_per_sample=bits_rsp,
                    channels=channels_rsp,
                    mic_gain=cfg.mic_gain,
                    volume=cfg.volume,
                    channel_type=cfg.channel_type,
                ),
            )

        duplex_params: dict[str, Any] = {'enable_afe': enable_afe}
        if not need_recv:
            duplex_params['record_file_path'] = record_path
        if not need_send:
            duplex_params['play_file_path'] = play_path

        await self.send_command('start_duplex', duplex_params)
        duplex_rsp = await self._recv_text_message()

        # When AFE is enabled, the device emits a fixed 16k/16bit/1ch stream.
        if (
            need_recv
            and isinstance(duplex_rsp, dict)
            and duplex_rsp.get('message') == 'afe_enabled'
        ):
            self._log_note('AFE on: RX locked at 16000Hz/16bit/1ch, rebuilding WAV')
            self._close_wav_writer()
            self._open_wav_writer(
                record_path,
                AudioConfig(
                    sample_rate=16000,
                    bits_per_sample=16,
                    channels=1,
                    mic_gain=cfg.mic_gain,
                    volume=cfg.volume,
                    channel_type=cfg.channel_type,
                ),
            )

        self.duplex = True
        total_tx = 0
        total_rx = 0
        loop = asyncio.get_running_loop()
        start = loop.time()

        play_duration = float(duration)
        if need_send:
            try:
                with wave.open(str(play_path), 'rb') as wav:
                    play_duration = wav.getnframes() / wav.getframerate()
            except Exception as exc:
                self._log_note(f'could not read play duration: {exc}')
                play_duration = float(duration)
        actual_duration = max(float(duration), play_duration)

        tx_chunk_size = self.DEFAULT_CHUNK_SIZE
        tx_bps = cfg.sample_rate * (cfg.bits_per_sample // 8) * cfg.channels

        def on_tx_progress(n: int) -> None:
            nonlocal total_tx
            total_tx = n
            self._log_progress(f'TX={total_tx} B   RX={total_rx} B')

        async def tx_task() -> None:
            if not need_send:
                await self._wait_duration_or_stop(actual_duration)
                return

            await self._send_audio_file(
                play_path, tx_chunk_size, tx_bps, on_progress=on_tx_progress
            )

            while not self.should_stop:
                if loop.time() - start >= actual_duration:
                    break
                await asyncio.sleep(0.1)

        async def rx_task() -> None:
            nonlocal total_rx
            while not self.should_stop:
                if loop.time() - start >= actual_duration:
                    break
                try:
                    msg_type, data = await asyncio.wait_for(self.receive_message(), timeout=0.5)
                except asyncio.TimeoutError:
                    continue
                if msg_type == 'binary':
                    total_rx += int(data)
                    if not need_send:
                        self._log_progress(f'RX={total_rx} B')

        tasks = [rx_task()]
        if need_send:
            tasks.append(tx_task())
        await asyncio.gather(*tasks, return_exceptions=True)

        if self.duplex:
            await self._stop_command('stop_duplex')
            self.duplex = False

        self._close_wav_writer()

        self._log_section('done')
        if need_recv:
            self._log_kv('record', record_path)
        else:
            self._log_kv('record', f'device  {record_path}')
        self._log_kv('TX total', f'{total_tx} B')
        self._log_kv('RX total', f'{total_rx} B')

    async def close(self) -> None:
        """Stop any in-flight operation and release resources."""
        try:
            await self.stop_current_operation()
        finally:
            self._close_wav_writer()
            if self.ws:
                await self.ws.close()
                self.ws = None
                self._log_note('connection closed')


def normalize_server_url(server: str) -> str:
    """Normalize and validate a ws/wss server URL."""
    raw = server.strip()
    if '://' not in raw:
        raw = f'ws://{raw}'

    parsed = urlparse(raw)
    if parsed.scheme not in {'ws', 'wss'}:
        raise ValueError(f'Unsupported scheme: {parsed.scheme} (only ws/wss)')
    if not parsed.netloc:
        raise ValueError('Server address is incomplete, missing host')
    if not parsed.path:
        raw = f"{raw.rstrip('/')}/ws"
    return raw


def resolve_server(server_arg: str | None) -> str:
    """Resolve the server URL from CLI argument, falling back to WS_SERVER env."""
    candidate = (server_arg or '').strip()
    if candidate:
        return candidate

    env_server = os.getenv('WS_SERVER', '').strip()
    if env_server:
        return env_server

    raise ValueError(
        'No server address provided. Use --server ws://<ip>:<port>/ws '
        'or export WS_SERVER=ws://<ip>:<port>/ws'
    )


def build_parser() -> argparse.ArgumentParser:
    examples = """
Path convention (all file arguments):
  - '/sdcard/...' or 'embed://...' -> device-side file
  - any other path                 -> local file on this host
  ('embed://' is read-only and cannot be used as a recording target)

Examples:
  # Set the server URL once to avoid repeating it
  export WS_SERVER=ws://192.168.0.179:80/ws
  # For WSS just export the wss URL into the same variable:
  export WS_SERVER=wss://192.168.0.179:443/ws

  #### Recording
  # WebSocket recording (device streams audio back to a local file)
  python3 esp_audio_analyzer_client.py --server $WS_SERVER --test record --duration 10
  python3 esp_audio_analyzer_client.py --server $WS_SERVER --test record --duration 20 --record-file ./recording_output.wav
  python3 esp_audio_analyzer_client.py --server $WS_SERVER --test record --duration 10 --sample-rate 48000 --channels 2
  python3 esp_audio_analyzer_client.py --server $WS_SERVER --test record --duration 5 --sample-rate 16000 --channels 4 --record-file ./rmnm_16k_4ch.wav

  # Record to the device SD card (path prefix /sdcard/ routes to device, only support pcm format)
  python3 esp_audio_analyzer_client.py --server $WS_SERVER --test record --duration 10 --record-file /sdcard/recording.pcm
  python3 esp_audio_analyzer_client.py --server $WS_SERVER --test record --duration 10 --record-file /sdcard/record/recording.pcm --channels 4

  #### Playback
  # WebSocket playback (stream a local file to the device, only support wav format)
  python3 esp_audio_analyzer_client.py --server $WS_SERVER --test play --play-file ~/Downloads/audio_samples/0dBFS_1KHz_16000_1ch.wav

  # Play a file from the device SD card
  python3 esp_audio_analyzer_client.py --server $WS_SERVER --test play --play-file /sdcard/audios/0dBFS_1KHz_16000_1ch.wav
  python3 esp_audio_analyzer_client.py --server $WS_SERVER --test play --play-file /sdcard/audios/embed_0dBFS_1KHz_16K.flac

  # Play a file from device embedded flash
  python3 esp_audio_analyzer_client.py --server $WS_SERVER --test play --play-file embed://tone/0_embed_0dBFS_1KHz_16K.flac
  python3 esp_audio_analyzer_client.py --server $WS_SERVER --test play --play-file embed://tone/1_embed_Silence_16K.flac
  python3 esp_audio_analyzer_client.py --server $WS_SERVER --test play --play-file embed://tone/2_embed_Stepped_sweep_61pt_16KHz.flac

  #### Duplex (simultaneous play and record)
  # Fully over WebSocket (local play + local record)
  python3 esp_audio_analyzer_client.py --server $WS_SERVER --test duplex --duplex-play ~/Downloads/audio_samples/0dBFS_1KHz_16000_2ch.wav --duplex-record ./rec.wav
  python3 esp_audio_analyzer_client.py --server $WS_SERVER --test duplex --duplex-play ~/Downloads/audio_samples/0dBFS_1KHz_16000_1ch.wav --duplex-record ./rec.wav --duration 10 --sample-rate 16000 --channels 4
  python3 esp_audio_analyzer_client.py --server $WS_SERVER --test duplex --duplex-play ~/Downloads/audio_samples/0dBFS_1KHz_16000_1ch.wav --duplex-record ./rec.wav --duration 10 --sample-rate 16000 --channels 4 --afe

  # Record to device + play from device (fully on-device)
  python3 esp_audio_analyzer_client.py --server $WS_SERVER --test duplex --duplex-record /sdcard/duplex_rec.wav --duplex-play /sdcard/0dBFS_1KHz_16000_2ch.wav --duration 10
  python3 esp_audio_analyzer_client.py --server $WS_SERVER --test duplex --duplex-record /sdcard/duplex_rec.wav --duplex-play /sdcard/0dBFS_1KHz_16000_2ch.wav --duration 10 --sample-rate 16000 --channels 4
  python3 esp_audio_analyzer_client.py --server $WS_SERVER --test duplex --duplex-record /sdcard/duplex_rec.wav --duplex-play /sdcard/0dBFS_1KHz_16000_2ch.wav --duration 10 --sample-rate 16000 --channels 4 --afe

  # Record to device + WebSocket playback
  python3 esp_audio_analyzer_client.py --server $WS_SERVER --test duplex --duplex-record /sdcard/duplex_rec.wav --duplex-play ~/Downloads/audio_samples/0dBFS_1KHz_16000_1ch.wav --duration 10
  python3 esp_audio_analyzer_client.py --server $WS_SERVER --test duplex --duplex-record /sdcard/duplex_rec.wav --duplex-play ~/Downloads/audio_samples/0dBFS_1KHz_16000_2ch.wav --duration 10 --sample-rate 16000 --channels 4
  python3 esp_audio_analyzer_client.py --server $WS_SERVER --test duplex --duplex-record /sdcard/duplex_rec.wav --duplex-play ~/Downloads/audio_samples/0dBFS_1KHz_16000_2ch.wav --duration 10 --sample-rate 16000 --channels 4 --afe

  # WebSocket recording + play from device
  python3 esp_audio_analyzer_client.py --server $WS_SERVER --test duplex --duplex-record ./rec.wav --duplex-play /sdcard/0dBFS_1KHz_16000_2ch.wav --duration 10
  python3 esp_audio_analyzer_client.py --server $WS_SERVER --test duplex --duplex-record ./rec.wav --duplex-play /sdcard/0dBFS_1KHz_16000_2ch.wav --duration 10 --sample-rate 16000 --channels 4
  python3 esp_audio_analyzer_client.py --server $WS_SERVER --test duplex --duplex-record ./rec.wav --duplex-play /sdcard/0dBFS_1KHz_16000_2ch.wav --duration 10 --sample-rate 16000 --channels 4 --afe
  python3 esp_audio_analyzer_client.py --server $WS_SERVER --test duplex --duplex-record ./rec.wav --duplex-play embed://tone/0_embed_0dBFS_1KHz_16K.flac --duration 10 --sample-rate 16000 --channels 4
    """

    parser = argparse.ArgumentParser(
        description='ESP WebSocket/WSS audio test tool',
        formatter_class=argparse.RawTextHelpFormatter,
        epilog=examples,
    )

    conn = parser.add_argument_group('Connection')
    conn.add_argument(
        '--server',
        nargs='?',
        const='',
        default=None,
        help=(
            'Full WebSocket URL, e.g. ws://192.168.0.179:80/ws or wss://192.168.0.179:443/ws. '
            'If left empty, falls back to the WS_SERVER environment variable.'
        ),
    )

    test = parser.add_argument_group('Test')
    test.add_argument(
        '--test',
        choices=['record', 'play', 'duplex', 'all'],
        default='all',
        help='Test type (all = record + play)',
    )
    test.add_argument('--duration', type=int, default=5, help='Recording/duplex duration (seconds)')
    test.add_argument('--afe', action='store_true', help='Enable AFE (AEC/NS) in duplex')

    audio = parser.add_argument_group('Audio (configure)')
    audio.add_argument('--sample-rate', type=int, default=16000, help='Sample rate (Hz)')
    audio.add_argument('--bits', type=int, default=16, help='Bits per sample')
    audio.add_argument('--channels', type=int, default=2, help='Channel count')
    audio.add_argument('--mic-gain', type=int, default=32, help='Microphone gain (device-defined)')
    audio.add_argument('--volume', type=int, default=30, help='Playback volume (device-defined)')
    audio.add_argument('--channel-type', default='MMNR', help='Channel type, e.g. MMNR')

    path_help = (
        "Auto-dispatched by prefix: '/sdcard/...' or 'embed://...' -> device-side file; "
        'any other path -> local file.'
    )

    rec = parser.add_argument_group('Recording')
    rec.add_argument(
        '--record-file',
        default='recording.wav',
        help=f'Recording target. {path_help} (embed:// not allowed for writes).',
    )

    play = parser.add_argument_group('Playback')
    play.add_argument(
        '--play-file',
        default='recording.wav',
        help=f'Playback source. {path_help}',
    )

    duplex = parser.add_argument_group('Duplex')
    duplex.add_argument(
        '--duplex-play',
        default='recording.wav',
        help=f'Duplex playback source. {path_help}',
    )
    duplex.add_argument(
        '--duplex-record',
        default='duplex_recording.wav',
        help=f'Duplex recording target. {path_help} (embed:// not allowed for writes).',
    )

    return parser


async def main() -> None:
    parser = build_parser()
    args = parser.parse_args()

    try:
        url = normalize_server_url(resolve_server(args.server))
    except ValueError as exc:
        parser.error(str(exc))
        return

    cfg = AudioConfig(
        sample_rate=args.sample_rate,
        bits_per_sample=args.bits,
        channels=args.channels,
        mic_gain=args.mic_gain,
        volume=args.volume,
        channel_type=args.channel_type,
    )
    client = WebSocketAudioClient(url=url)

    def signal_handler(_signum: int, _frame: Any) -> None:
        client._log_note('interrupt received, stopping')
        client.should_stop = True

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    try:
        await client.connect()

        if args.test in {'record', 'all'}:
            await client.start_recording(args.record_file, args.duration, cfg)

        if args.test in {'play', 'all'}:
            await client.start_playback(args.play_file, cfg)

        if args.test == 'duplex':
            await client.run_duplex(
                play_path=args.duplex_play,
                record_path=args.duplex_record,
                duration=args.duration,
                cfg=cfg,
                enable_afe=args.afe,
            )

    except (FileNotFoundError, wave.Error, OSError) as exc:
        client._log_note(f'file I/O error: {exc}')
    except ValueError as exc:
        client._log_note(f'invalid argument: {exc}')
    except Exception as exc:
        client._log_note(f'execution failed: {exc}')
        traceback.print_exc()
    finally:
        await client.close()


if __name__ == '__main__':
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print('* interrupted by user')
