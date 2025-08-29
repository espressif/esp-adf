#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
# SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
"""
BLE OTA driver for the ota_ble example.

Drives an ESP32-S3 that runs the native ESP BLE OTA APP protocol via an
ESP32-C6 ESP-AT BLE bridge. Implements the same on-air protocol as the
EspressifApps/esp-ble-ota-android client:

  - GATT service 0x8018
  - COMMAND  characteristic 0x8022 (Start = 0x01, Stop = 0x02)
  - RECV_FW  characteristic 0x8020 (3-byte header + payload, last packet of
    a 4 KB sector uses pkt_seq = 0xFF)
  - 20-byte indication ACK on the same characteristic (CRC16-CCITT, poly 0x1021)

Subcommands
-----------
  upload            Push a versioned bin to the device (default when a bin is
                    supplied with no explicit subcommand).
  check-c6          Probe ESP-AT on the C6 bridge and print AT+GMR version.
  flash-c6          One-shot: esptool write_flash the C6 with the factory
                    image or the custom MTU=517 build (both live under
                    tools/esp_at_esp32c6/).

Examples
--------
  # Upload the newest ota_ble_v*.bin published by build_firmware.py
  ./ota_ble.py upload --port /dev/ttyUSB2

  # Pin an explicit version
  ./ota_ble.py upload --bin ota_ble_v1.0.1.bin --port /dev/ttyUSB2

  # Probe the AT bridge
  ./ota_ble.py check-c6 --port /dev/ttyUSB2

  # Flash the C6 with the high-MTU build (different download port)
  ./ota_ble.py flash-c6 --variant mtu517 --port /dev/ttyUSB1

Requires: pip install pyserial
"""

import argparse
import os
import struct
import subprocess
import sys
import time

_TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
if _TOOLS_DIR not in sys.path:
    sys.path.insert(0, _TOOLS_DIR)

# pylint: disable=import-error,wrong-import-position
import _ble_at as _at  # noqa: E402
from _fw_common import resolve_bin, TOOLS_DIR  # noqa: E402

# ---------------------------------------------------------------------------
# Protocol constants — must match esp_ota_service_source_ble.c and the Android APP.
# ---------------------------------------------------------------------------
OTA_SVC_UUID        = '8018'
OTA_RECV_FW_UUID    = '8020'
OTA_COMMAND_UUID    = '8022'

# 16-bit UUIDs are sometimes reported by ESP-AT in their 128-bit base form.
BASE_UUID_128_TAIL  = '00001000800000805f9b34fb'
OTA_SVC_UUID_128    = '0000' + OTA_SVC_UUID     + BASE_UUID_128_TAIL
OTA_RECV_FW_UUID_128 = '0000' + OTA_RECV_FW_UUID + BASE_UUID_128_TAIL
OTA_COMMAND_UUID_128 = '0000' + OTA_COMMAND_UUID + BASE_UUID_128_TAIL

# CCCD descriptor UUID (Bluetooth assigned number 0x2902).
CCCD_UUID           = '2902'
CCCD_UUID_128       = '0000' + CCCD_UUID + BASE_UUID_128_TAIL

# COMMAND opcodes.
OP_START            = 0x01
OP_STOP             = 0x02
OP_ACK_PREFIX       = 0x03

# Sector + ACK sizes.
SECTOR_SIZE         = 4096
ACK_LEN             = 20

# Default baud rates / scan params come straight from the sibling script.
DEFAULT_BAUD        = _at.DEFAULT_BAUD
HIGH_SPEED_BAUD     = _at.HIGH_SPEED_BAUD
SCAN_DURATION_SEC   = _at.SCAN_DURATION_SEC
NAME_SCAN_FAST_SEC  = _at.NAME_SCAN_FAST_SEC
NO_FILTER_SCAN_SEC  = _at.NO_FILTER_SCAN_SEC
MAX_DEVICES_TO_PROBE = _at.MAX_DEVICES_TO_PROBE
DEFAULT_MTU         = _at.DEFAULT_MTU

CMD_ACK_TIMEOUT     = 8.0   # generous: first start ack may be delayed by host init
SECTOR_ACK_TIMEOUT  = 8.0


# ---------------------------------------------------------------------------
# CRC16-CCITT helper (poly 0x1021, init 0x0000) — must match the device side.
# ---------------------------------------------------------------------------
def crc16_ccitt(data: bytes) -> int:
    crc = 0
    for b in data:
        crc ^= (b << 8) & 0xFFFF
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


# ---------------------------------------------------------------------------
# UUID matching: ESP-AT may return 16-bit UUIDs as 4-char "8018" or as the
# corresponding 128-bit base form "00008018-0000-1000-8000-00805f9b34fb".
# Compare both.
# ---------------------------------------------------------------------------
def _uuid_equiv(reported: str, expected16: str, expected128: str) -> bool:
    norm = _at.uuid_normalize(reported) if isinstance(reported, str) else reported
    return norm == expected16 or norm == expected128


# ---------------------------------------------------------------------------
# GATT discovery: AT+BLEGATTCCHAR returns char rows ("char") AND descriptor
# rows ("desc"). We need the descriptor index of the CCCD (0x2902) for both
# RECV_FW and COMMAND so we can enable indications via AT+BLEGATTCWR.
# ---------------------------------------------------------------------------
def at_ble_gattc_char_with_desc(ser, conn_index, srv_index):
    lines = _at.send_at(ser, f'AT+BLEGATTCCHAR={conn_index},{srv_index}')
    chars = []   # list of (char_index, char_uuid)
    descs = []   # list of (char_index, desc_index, desc_uuid)
    for line in lines:
        if not line.startswith('+BLEGATTCCHAR:'):
            continue
        rest = line[len('+BLEGATTCCHAR:'):].strip()
        parts = rest.split(',', 5)
        if len(parts) < 5:
            continue
        kind = parts[0].strip().strip('"').lower()
        try:
            if kind == 'char':
                if len(parts) < 6:
                    continue
                char_index = int(parts[3])
                char_uuid  = _at.uuid_normalize(parts[4].strip('"'))
                chars.append((char_index, char_uuid))
            elif kind == 'desc':
                char_index = int(parts[3])
                desc_index = int(parts[4])
                desc_uuid  = _at.uuid_normalize(parts[5].strip('"')) if len(parts) >= 6 else ''
                descs.append((char_index, desc_index, desc_uuid))
        except (ValueError, IndexError):
            continue
    return chars, descs


def find_iot_service_and_chars(ser, conn_index=0):
    """Return dict with svc/recv_fw/command indices and CCCD desc indices."""
    services = _at.at_ble_gattc_prim_srv(ser, conn_index)
    time.sleep(1.5)
    srv_index = None
    for idx, uuid in services:
        if _uuid_equiv(uuid, OTA_SVC_UUID, OTA_SVC_UUID_128):
            srv_index = idx
            break
    if srv_index is None:
        raise RuntimeError('BLE OTA service 0x8018 not found on device')

    chars, descs = at_ble_gattc_char_with_desc(ser, conn_index, srv_index)
    time.sleep(1.5)

    recv_fw_idx = None
    command_idx = None
    for char_index, char_uuid in chars:
        if _uuid_equiv(char_uuid, OTA_RECV_FW_UUID, OTA_RECV_FW_UUID_128):
            recv_fw_idx = char_index
        elif _uuid_equiv(char_uuid, OTA_COMMAND_UUID, OTA_COMMAND_UUID_128):
            command_idx = char_index
    if recv_fw_idx is None or command_idx is None:
        raise RuntimeError(f'OTA chars missing (recv_fw={recv_fw_idx}, command={command_idx})')

    # Find CCCD descriptor index for each. Prefer matching by UUID 0x2902;
    # fall back to "first descriptor of the char" since ESP-AT does not always
    # populate the descriptor UUID column.
    def _cccd_for(char_idx):
        # Prefer explicit 0x2902 match.
        for c, d, u in descs:
            if c == char_idx and (u == CCCD_UUID or u == CCCD_UUID_128):
                return d
        for c, d, _u in descs:
            if c == char_idx:
                return d
        return None

    recv_fw_cccd = _cccd_for(recv_fw_idx)
    command_cccd = _cccd_for(command_idx)
    if recv_fw_cccd is None or command_cccd is None:
        raise RuntimeError(
            f'CCCD descriptor not found '
            f'(recv_fw_cccd={recv_fw_cccd}, command_cccd={command_cccd})'
        )

    return {
        'svc':          srv_index,
        'recv_fw':      recv_fw_idx,
        'command':      command_idx,
        'recv_fw_cccd': recv_fw_cccd,
        'command_cccd': command_cccd,
    }


# ---------------------------------------------------------------------------
# Connect + discover for one MAC. Returns (handles, mtu) on success, or None.
# ---------------------------------------------------------------------------
def try_connect_iot(ser, mac, conn_index=0):
    try:
        _at.at_ble_conn(ser, mac, conn_index)
        mtu = _at.at_ble_set_mtu(ser, conn_index, 512)
        _at.at_ble_set_conn_interval(ser, conn_index)
        time.sleep(1.2)
        handles = find_iot_service_and_chars(ser, conn_index)
        return handles, mtu
    except Exception:
        try:
            _at.send_at(ser, f'AT+BLEDISCONN={conn_index}', timeout_sec=3)
        except Exception:
            pass
        time.sleep(0.5)
        return None, DEFAULT_MTU


# ---------------------------------------------------------------------------
# CCCD enable for indications: write 0x02 0x00 to the CCCD descriptor.
# AT+BLEGATTCWR=conn,srv,char,desc,length\r\n  >  raw 2 bytes  > OK
# ---------------------------------------------------------------------------
def at_ble_gattc_wr_desc(ser, conn_index, srv_index, char_index, desc_index, data,
                         timeout_sec=5.0):
    length = len(data)
    ser.reset_input_buffer()
    ser.write(
        f'AT+BLEGATTCWR={conn_index},{srv_index},{char_index},{desc_index},{length}\r\n'
        .encode('utf-8')
    )
    deadline = time.monotonic() + timeout_sec
    buf = b''
    got_prompt = False
    while time.monotonic() < deadline:
        if ser.in_waiting:
            buf += ser.read(ser.in_waiting)
            if b'>' in buf:
                got_prompt = True
                break
        else:
            time.sleep(0.005)
    if not got_prompt:
        raise RuntimeError("BLEGATTCWR (desc) did not return '>' prompt")
    ser.write(data)
    lines = _at.read_lines(ser, timeout_sec=timeout_sec)
    if lines and lines[-1] != 'OK':
        raise RuntimeError(f'BLEGATTCWR (desc) failed: {lines}')


def enable_indications(ser, handles, conn_index=0):
    print('  Enabling indications on RECV_FW (0x8020) and COMMAND (0x8022)...')
    at_ble_gattc_wr_desc(ser, conn_index, handles['svc'], handles['recv_fw'],
                         handles['recv_fw_cccd'], bytes([0x02, 0x00]))
    time.sleep(0.2)
    at_ble_gattc_wr_desc(ser, conn_index, handles['svc'], handles['command'],
                         handles['command_cccd'], bytes([0x02, 0x00]))
    time.sleep(0.2)


# ---------------------------------------------------------------------------
# Indication / notification reader.
# ESP-AT URC format:
#   +BLEGATTCNTFY:<conn_index>,<srv_index>,<char_index>,<len>,<value...>
# <value> is raw binary so we cannot rely on line splitting. Buffer the
# stream and parse one URC at a time.
# ---------------------------------------------------------------------------
class IndicationReader:
    # ESP-AT URC names vary across firmware versions:
    #   +INDICATE:     ATT indication (CCCD value 0x02) — current ESP-AT
    #   +BLEGATTCIND:  ATT indication (older / vendor builds)
    #   +NOTIFY:       ATT notification (CCCD value 0x01)
    #   +BLEGATTCNTFY: ATT notification (older / vendor builds)
    # All four use the same payload framing:
    #   <conn>,<srv>,<char>,<len>,<raw_bytes>
    # We accept all of them; the firmware's "char" index for indications does
    # not necessarily match the index returned by AT+BLEGATTCCHAR, so the
    # caller must verify ACK contents instead of relying on the index.
    URCS = (b'+INDICATE:', b'+NOTIFY:',
            b'+BLEGATTCIND:', b'+BLEGATTCNTFY:')

    def __init__(self, ser):
        self.ser = ser
        self.buf = bytearray()

    def _find_first_urc(self):
        best = -1
        best_len = 0
        for tag in self.URCS:
            idx = self.buf.find(tag)
            if idx >= 0 and (best < 0 or idx < best):
                best = idx
                best_len = len(tag)
        return best, best_len

    def _parse_one(self):
        idx, urc_len = self._find_first_urc()
        if idx < 0:
            # Drop preamble that does not contain any URC marker (keep last
            # bytes in case a partial marker straddles the boundary).
            keep = max(len(t) for t in self.URCS)
            if len(self.buf) > 1024:
                del self.buf[:-keep]
            return None
        scan = self.buf[idx + urc_len:]
        commas = []
        for i, b in enumerate(scan):
            if b == 0x2C:  # ','
                commas.append(i)
                if len(commas) == 4:
                    break
        if len(commas) < 4:
            return None
        try:
            header = scan[:commas[3]].decode('ascii', 'replace')
            parts = header.split(',')
            char_idx = int(parts[2])
            data_len = int(parts[3])
        except (ValueError, IndexError):
            del self.buf[:idx + 1]
            return self._parse_one()
        value_start = commas[3] + 1
        value_end   = value_start + data_len
        if len(scan) < value_end:
            return None
        data = bytes(scan[value_start:value_end])
        consumed = idx + urc_len + value_end
        del self.buf[:consumed]
        return char_idx, data

    def wait(self, _expected_char_idx, timeout_sec):
        # ESP-AT may report a different "char" index for indications than the
        # one returned by AT+BLEGATTCCHAR (handle-vs-index numbering). Since
        # the OTA protocol has only one outstanding ACK at a time and the
        # caller verifies the ACK payload, we ignore the expected char index.
        deadline = time.monotonic() + timeout_sec
        while time.monotonic() < deadline:
            if self.ser.in_waiting:
                self.buf += self.ser.read(self.ser.in_waiting)
            parsed = self._parse_one()
            if parsed is None:
                if not self.ser.in_waiting:
                    time.sleep(0.005)
                continue
            _char_idx, data = parsed
            return data
        return None


# ---------------------------------------------------------------------------
# Binary-safe AT+BLEGATTCWR helper.
#
# The shared `at_ble_gattc_wr` in _ble_at.py reads the post-data
# response with a line-oriented parser that splits on \r/\n. That corrupts
# the +BLEGATTCNTFY URC (whose payload is raw binary and may legitimately
# contain 0x0A/0x0D bytes), and the URC bytes are dropped silently.
#
# Here we read the post-data stream as raw bytes until "\r\nOK\r\n" or
# "\r\nERROR\r\n" arrives, then push *every* byte received during the wait
# into the IndicationReader buffer. That way the indication ACK that races
# with OK is never lost.
# ---------------------------------------------------------------------------
def gattc_wr_capture(ser, indications, conn_index, srv_index, char_index, data,
                     timeout_sec=8.0):
    length = len(data)
    ser.reset_input_buffer()
    ser.write(
        f'AT+BLEGATTCWR={conn_index},{srv_index},{char_index},,{length}\r\n'
        .encode('utf-8')
    )
    deadline = time.monotonic() + timeout_sec
    pre = bytearray()
    while time.monotonic() < deadline:
        if ser.in_waiting:
            pre += ser.read(ser.in_waiting)
            if b'>' in pre:
                break
        else:
            time.sleep(0.005)
    if b'>' not in pre:
        raise RuntimeError(
            f"BLEGATTCWR did not return '>' prompt (got: {bytes(pre)[:120]!r})"
        )
    # Anything before the prompt that is not part of the prompt line could
    # contain leftover URCs from prior writes. Forward it.
    indications.buf += bytes(pre)
    ser.write(data)
    post = bytearray()
    end_ok    = b'\r\nOK\r\n'
    end_err   = b'\r\nERROR\r\n'
    deadline2 = time.monotonic() + timeout_sec
    while time.monotonic() < deadline2:
        if ser.in_waiting:
            post += ser.read(ser.in_waiting)
            if end_ok in post or end_err in post:
                break
        else:
            time.sleep(0.005)
    indications.buf += bytes(post)
    if end_err in post:
        raise RuntimeError('BLEGATTCWR returned ERROR after data')
    if end_ok not in post:
        raise RuntimeError(
            f'BLEGATTCWR no OK after data (got: {bytes(post)[:120]!r})'
        )


def verify_ack(ack_bytes, label):
    if ack_bytes is None or len(ack_bytes) != ACK_LEN:
        raise RuntimeError(f'{label}: ACK missing or wrong length '
                           f'(got {0 if ack_bytes is None else len(ack_bytes)} bytes)')
    crc = crc16_ccitt(ack_bytes[:18])
    rx  = ack_bytes[18] | (ack_bytes[19] << 8)
    if crc != rx:
        raise RuntimeError(f'{label}: CRC mismatch (calc=0x{crc:04x}, rx=0x{rx:04x})')


# ---------------------------------------------------------------------------
# Send a 4 KB sector as N packets (last packet uses pkt_seq = 0xFF).
# ---------------------------------------------------------------------------
def send_sector(ser, handles, conn_index, indications, sector_index,
                sector_data, packet_payload):
    sec_lo = sector_index & 0xFF
    sec_hi = (sector_index >> 8) & 0xFF

    offset = 0
    pkt_seq = 0
    while offset < len(sector_data):
        is_last = (offset + packet_payload) >= len(sector_data)
        chunk = sector_data[offset:offset + packet_payload]
        offset += len(chunk)
        seq_byte = 0xFF if is_last else pkt_seq
        header = bytes([sec_lo, sec_hi, seq_byte])
        gattc_wr_capture(ser, indications, conn_index,
                         handles['svc'], handles['recv_fw'], header + chunk)
        if not is_last:
            pkt_seq += 1

    ack = indications.wait(handles['recv_fw'], SECTOR_ACK_TIMEOUT)
    verify_ack(ack, f'sector {sector_index} ack')
    if ack[2] != 0x00:
        raise RuntimeError(
            f'sector {sector_index}: device reported error '
            f'(status=0x{ack[2]:02x})'
        )


# ---------------------------------------------------------------------------
# Main upload routine.
# ---------------------------------------------------------------------------
def upload_iot(ser, path, device_name, conn_index=0, scan_duration_sec=None):
    with open(path, 'rb') as f:
        firmware = f.read()
    size = len(firmware)
    if size == 0:
        raise RuntimeError('firmware file is empty')

    _at.at_ble_init(ser)

    name_scan_sec = NAME_SCAN_FAST_SEC
    print(f'Scanning for "{device_name}" ({name_scan_sec} s)...')
    mac = _at.scan_and_get_mac(ser, name_filter=device_name,
                               scan_duration_sec=name_scan_sec)
    handles = None
    mtu = DEFAULT_MTU
    if mac:
        print(f'  Found {mac}, connecting and discovering OTA service 0x8018...')
        handles, mtu = try_connect_iot(ser, mac, conn_index)
        if handles is None:
            print('  No OTA service on that MAC, falling back to no-filter scan')

    if handles is None:
        _at.send_at_blescan_stop(ser)
        time.sleep(0.6)
        _at.at_ble_init(ser)
        no_filter_sec = NO_FILTER_SCAN_SEC
        print(f'  Scanning all BLE devices ({no_filter_sec} s)...')
        macs = _at.scan_no_filter(ser, scan_duration_sec=no_filter_sec)
        macs_sorted = sorted(macs, key=lambda x: -x[1])[:MAX_DEVICES_TO_PROBE]
        print(f'  Probing {len(macs_sorted)} candidate(s)')
        for m, rssi in macs_sorted:
            print(f'  Trying {m} (rssi={rssi})...')
            handles, mtu = try_connect_iot(ser, m, conn_index)
            if handles is not None:
                mac = m
                print(f'  OTA service 0x8018 found on {m}')
                break
        if handles is None and scan_duration_sec:
            mac = _at.scan_and_get_mac(ser, name_filter=device_name,
                                       scan_duration_sec=scan_duration_sec)
            if mac:
                handles, mtu = try_connect_iot(ser, mac, conn_index)

    if handles is None:
        raise RuntimeError(f'No BLE device exposing OTA service 0x8018 '
                           f'(name "{device_name}") found.')

    # Listener for +BLEGATTCNTFY URCs.
    indications = IndicationReader(ser)

    enable_indications(ser, handles, conn_index)

    # Per-packet payload: 3-byte protocol header lives inside the GATT write,
    # so payload = MTU - 3 (ATT header) - 3 (our header). Cap to a sector size
    # so we never need to span sectors in one write.
    packet_payload = max(1, min(SECTOR_SIZE, mtu - 6))
    n_sectors = (size + SECTOR_SIZE - 1) // SECTOR_SIZE
    print(f'OTA service idx={handles['svc']}, '
          f'recv_fw idx={handles['recv_fw']}, command idx={handles['command']}')
    print(f'Negotiated MTU={mtu}, packet payload={packet_payload} B, '
          f'sector={SECTOR_SIZE} B, total sectors={n_sectors}')

    # CMD_START with 4-byte LE size on COMMAND char.
    print(f'Sending CMD_START, fw_size={size}')
    start_payload = struct.pack('<BBI', OP_START, 0x00, size)
    gattc_wr_capture(ser, indications, conn_index,
                     handles['svc'], handles['command'], start_payload)
    ack = indications.wait(handles['command'], CMD_ACK_TIMEOUT)
    verify_ack(ack, 'CMD_START ack')
    if ack[0] != OP_ACK_PREFIX or ack[2] != 0x01:
        raise RuntimeError(f'CMD_START ack unexpected: {ack[:6].hex()}')
    print('  CMD_START ack OK')

    start_t   = time.monotonic()
    last_t    = start_t
    last_sent = 0
    sent_bytes = 0
    for sec in range(n_sectors):
        offset = sec * SECTOR_SIZE
        sector_data = firmware[offset:offset + SECTOR_SIZE]
        sec_t0 = time.monotonic()
        send_sector(ser, handles, conn_index, indications, sec,
                    sector_data, packet_payload)
        sec_dt = time.monotonic() - sec_t0
        sent_bytes += len(sector_data)

        now = time.monotonic()
        if (sec == 0 or sec == n_sectors - 1
                or now - last_t >= 1.0):
            elapsed   = now - start_t
            inst_rate = (sent_bytes - last_sent) / max(now - last_t, 1e-3)
            avg_rate  = sent_bytes / elapsed if elapsed > 0 else 0
            eta       = (size - sent_bytes) / avg_rate if avg_rate > 0 else 0
            pct       = sent_bytes * 100.0 / size
            print(f'  [{pct:5.1f}%] sec {sec + 1}/{n_sectors} '
                  f'({sent_bytes}/{size} B) sec_dt={sec_dt * 1000:.0f}ms '
                  f'inst={inst_rate:6.0f} B/s avg={avg_rate:6.0f} B/s '
                  f'ETA={eta:5.0f}s',
                  flush=True)
            last_t    = now
            last_sent = sent_bytes

    # CMD_STOP on COMMAND char.
    print('Sending CMD_STOP')
    gattc_wr_capture(ser, indications, conn_index,
                     handles['svc'], handles['command'], bytes([OP_STOP, 0x00]))
    ack = indications.wait(handles['command'], CMD_ACK_TIMEOUT)
    verify_ack(ack, 'CMD_STOP ack')
    if ack[0] != OP_ACK_PREFIX or ack[2] != 0x02:
        raise RuntimeError(f'CMD_STOP ack unexpected: {ack[:6].hex()}')
    print('  CMD_STOP ack OK; device will reboot on successful upgrade.')
    total = time.monotonic() - start_t
    print(f'Upload finished: {size} bytes in {total:.1f}s '
          f'(avg {size / total:.0f} B/s, {n_sectors} sectors)')


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

AT_DIR = TOOLS_DIR / 'esp_at_esp32c6'

C6_AT_VARIANTS = {
    'factory': AT_DIR / 'factory' / 'factory_ESP32C6-4MB.bin',
    'mtu517':  AT_DIR / 'esp_at_c6_mtu517.bin',
}


def _open_and_handshake(port: str, baud: int, high_baud: int):
    """Open the AT serial, run AT handshake, optionally switch to high_baud.

    Policy: the C6 ESP-AT bridge normally sits at 115200 (its ROM default and
    what every other tool expects). Only the bulk OTA transfer temporarily
    bumps the UART to `high_baud` for throughput. The caller is responsible
    for putting the baud back via `_restore_baud` when it is done."""
    ser = _at.open_serial(port, baud)

    deadline = time.monotonic() + 12
    while time.monotonic() < deadline:
        try:
            _at.send_at(ser, 'AT', timeout_sec=2)
            break
        except Exception:
            time.sleep(0.5)
    else:
        ser.close()
        raise RuntimeError(f'AT not ready on {port}')

    try:
        _at.send_at(ser, 'ATE0', timeout_sec=1)
    except Exception:
        pass

    if high_baud and high_baud != baud:
        try:
            ser.write(f'AT+UART_CUR={high_baud},8,1,0,0\r\n'.encode())
            time.sleep(0.1)
            ser.close()
            ser = _at.open_serial(port, high_baud)
            time.sleep(0.05)
            _at.send_at(ser, 'AT', timeout_sec=2)
            print(f'  Serial switched to {high_baud} baud '
                  f'(serial ceiling now {high_baud * 0.858 / 1024:.1f} KB/s)')
        except Exception as e:
            print(f'  (Baud switch to {high_baud} failed: {e}; falling back to {baud})')
            try:
                ser.close()
            except Exception:
                pass
            ser = _at.open_serial(port, baud)
            _at.send_at(ser, 'AT', timeout_sec=2)
    return ser


def _handshake_ok(ser, timeout_sec: float = 1.5) -> bool:
    """Send 'AT' and return True only if we literally read 'OK' back.

    Unlike `_at.send_at`, this does not treat an empty response as success,
    which matters when we are probing an unknown/wrong baud rate."""
    try:
        ser.reset_input_buffer()
        ser.write(b'AT\r\n')
    except Exception:
        return False
    deadline = time.monotonic() + timeout_sec
    buf = b''
    while time.monotonic() < deadline:
        try:
            chunk = ser.read(ser.in_waiting or 1)
        except Exception:
            return False
        if chunk:
            buf += chunk
            if b'\r\nOK\r\n' in buf or buf.endswith(b'OK\r\n'):
                return True
        else:
            time.sleep(0.02)
    return False


def _restore_baud(port: str, current_baud: int, target_baud: int,
                  failed: bool) -> None:
    """Put the C6 ESP-AT bridge back at `target_baud` (default 115200).

    Two code paths:
      * upload succeeded -> send AT+UART_CUR at current_baud, verify OK at
        target_baud. Non-destructive; BLE state is preserved.
      * upload failed    -> C6 is likely stuck in "busy p..." from a half
        sent BLEGATTCWR. Issue AT+RST at current_baud; the C6 reboots and
        ESP-AT always comes up at 115200, which is what we want anyway.

    Best-effort: never raises. Any leftover junk is just logged."""
    if not target_baud:
        return
    if current_baud == target_baud and not failed:
        return

    try:
        ser = _at.open_serial(port, current_baud)
    except Exception as e:
        print(f'  (baud restore skipped: cannot reopen @ {current_baud}: {e})')
        return

    try:
        if failed:
            # Harder recovery: AT+RST -> C6 reboots -> default 115200.
            try:
                ser.write(b'AT+RST\r\n')
                time.sleep(0.2)
            except Exception:
                pass
            ser.close()
            # Give the C6 time to actually boot + print "ready".
            time.sleep(2.5)
        else:
            try:
                ser.write(f'AT+UART_CUR={target_baud},8,1,0,0\r\n'.encode())
                time.sleep(0.25)
            finally:
                ser.close()
    except Exception:
        try:
            ser.close()
        except Exception:
            pass

    # Verify: the C6 must actually reply OK at target_baud.
    try:
        ser2 = _at.open_serial(port, target_baud)
    except Exception as e:
        print(f'  (baud restore: cannot open @ {target_baud}: {e})')
        return
    try:
        # When we just reset, the very first AT can race the "ready" banner.
        # Give it two tries.
        ok = _handshake_ok(ser2, timeout_sec=1.5) or _handshake_ok(ser2, 1.5)
    finally:
        ser2.close()
    if ok:
        how = 'reset' if failed else 'UART_CUR'
        print(f'  Serial restored to {target_baud} baud (via {how}).')
    else:
        print(f'  (baud restore: no OK @ {target_baud}; run '
              f'`ota_ble.py check-c6 --port {port}` to recover.)')


def _cmd_upload(args) -> int:
    try:
        bin_path = resolve_bin('ota_ble', args.bin)
    except FileNotFoundError as e:
        print(f'[error] {e}', file=sys.stderr)
        return 1
    print(f'[bin] {bin_path}')

    try:
        ser = _open_and_handshake(args.port, args.baud, args.high_baud)
    except Exception as e:
        print(f'[error] open {args.port}: {e}', file=sys.stderr)
        return 2

    active_baud = (args.high_baud
                   if args.high_baud and args.high_baud != args.baud
                   else args.baud)
    upload_failed = True
    try:
        upload_iot(ser, str(bin_path), device_name=args.device,
                   scan_duration_sec=args.scan_duration)
        print('Done.')
        upload_failed = False
        return 0
    except Exception as e:
        print(f'Error: {e}', file=sys.stderr)
        return 2
    finally:
        try:
            ser.close()
        except Exception:
            pass
        # Always put the C6 back at the default baud so later tools (and a
        # fresh `check-c6` from this script) see it at 115200.
        _restore_baud(args.port, active_baud, args.baud,
                      failed=upload_failed)


def _probe_at_once(port: str, baud: int) -> str:
    """Send a single 'AT\\r\\n' at <baud> and return whatever the C6 replied
    within ~0.5 s. Empty string means silence at that baud."""
    try:
        ser = _at.open_serial(port, baud)
    except Exception:
        return ''
    try:
        ser.reset_input_buffer()
        ser.write(b'AT\r\n')
        time.sleep(0.5)
        return ser.read(ser.in_waiting or 1).decode(errors='replace')
    finally:
        ser.close()


def _cmd_check_c6(args) -> int:
    """Probe the C6 ESP-AT bridge and print AT+GMR.

    Tries --baud first (default 115200 - what ESP-AT boots into). If that
    fails, scans a small list of common high-speed baud rates, because a
    previous `upload` run may have left the C6 UART at 921600/460800 without
    reverting. When found at a non-default baud we optionally send
    AT+UART_CUR=<baud>,... to pin it back to --baud so later scripts work.
    """
    baud_to_try = [args.baud]
    for b in (921600, 460800, 230400, 115200):
        if b not in baud_to_try:
            baud_to_try.append(b)

    ok_baud: int | None = None
    for b in baud_to_try:
        out = _probe_at_once(args.port, b)
        tag = 'OK' if 'OK' in out else ('silent' if not out.strip() else 'noise')
        print(f'AT @ {b}: {tag}: {out!r}')
        if tag == 'OK':
            ok_baud = b
            break

    if ok_baud is None:
        print('\nC6 ESP-AT: NO RESPONSE - flash it first via `flash-c6`',
              file=sys.stderr)
        return 3

    try:
        ser = _at.open_serial(args.port, ok_baud)
    except Exception as e:
        print(f'[error] open {args.port} @ {ok_baud}: {e}', file=sys.stderr)
        return 2
    try:
        ser.write(b'AT+GMR\r\n')
        time.sleep(0.8)
        print('AT+GMR ->',
              ser.read(ser.in_waiting or 1).decode(errors='replace'))

        if ok_baud != args.baud and args.reset_baud:
            print(f'  (C6 was left at {ok_baud}; pinning back to '
                  f'{args.baud} via AT+UART_CUR)')
            ser.write(f'AT+UART_CUR={args.baud},8,1,0,0\r\n'.encode())
            time.sleep(0.3)
            ser.close()
            ser = _at.open_serial(args.port, args.baud)
            ser.write(b'AT\r\n')
            time.sleep(0.3)
            confirm = ser.read(ser.in_waiting or 1).decode(errors='replace')
            print(f'  handshake @ {args.baud}: {confirm!r}')
    finally:
        ser.close()

    print(f'\nC6 ESP-AT: OK @ {ok_baud} baud')
    return 0


def _cmd_flash_c6(args) -> int:
    variant_path = C6_AT_VARIANTS.get(args.variant)
    if variant_path is None:
        print(f'[error] unknown variant: {args.variant}', file=sys.stderr)
        return 1
    if not variant_path.is_file():
        print(f'[error] missing ESP-AT bin: {variant_path}', file=sys.stderr)
        return 1

    if not args.yes:
        ans = input(f'About to wipe C6 on {args.port} and flash '
                    f'ESP-AT ({args.variant}) - proceed? [y/N] ')
        if ans.strip().lower() not in ('y', 'yes'):
            print('aborted')
            return 0

    cmd = [
        sys.executable, '-m', 'esptool',
        '--chip', 'esp32c6',
        '-p', args.port,
        '-b', str(args.baud),
        '--before', 'default_reset',
        '--after', 'hard_reset',
        'write_flash', '0x0', str(variant_path),
    ]
    print('+ ' + ' '.join(cmd))
    try:
        subprocess.check_call(cmd)
    except subprocess.CalledProcessError as e:
        return e.returncode or 1
    print(f'\nDone. C6 will reboot running ESP-AT ({args.variant}).')
    print('After ~3s, run:  ota_ble.py check-c6')
    return 0


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description='BLE OTA driver for the ota_ble example.')
    sub = parser.add_subparsers(dest='cmd')

    # upload (also the default command for backwards compat)
    up = sub.add_parser('upload', help='Upload a firmware bin over BLE.')
    up.add_argument('--bin', default=None,
                    help='Path or filename inside firmware_samples/ '
                         '(default: newest ota_ble_v*.bin).')
    up.add_argument('--port', '-p', default='/dev/ttyUSB2',
                    help='AT serial port on the C6 (default: /dev/ttyUSB2).')
    up.add_argument('--device', '-d', default='OTA_BLE',
                    help='BLE device name to scan (default: OTA_BLE).')
    up.add_argument('--baud', '-b', type=int, default=DEFAULT_BAUD,
                    help='Serial baud rate (default: 115200).')
    up.add_argument('--high-baud', type=int, default=HIGH_SPEED_BAUD,
                    help='Switch C6 to this baud after AT handshake '
                         '(0 = keep --baud).')
    up.add_argument('--scan-duration', type=int, default=SCAN_DURATION_SEC,
                    help=f'BLE scan duration (default: {SCAN_DURATION_SEC}).')

    # check-c6
    ck = sub.add_parser('check-c6', help='Probe ESP-AT on the C6 bridge.')
    ck.add_argument('--port', '-p', default='/dev/ttyUSB2')
    ck.add_argument('--baud', '-b', type=int, default=DEFAULT_BAUD,
                    help='Preferred baud to talk to the C6 (default: '
                         f'{DEFAULT_BAUD}). If silent, common high-speed '
                         'baud rates are scanned too.')
    ck.add_argument('--reset-baud', dest='reset_baud', action='store_true',
                    default=True,
                    help='When the C6 is found at a non-default baud, send '
                         'AT+UART_CUR to pin it back to --baud (default on).')
    ck.add_argument('--no-reset-baud', dest='reset_baud',
                    action='store_false',
                    help='Leave the C6 at whatever baud it was found on.')

    # flash-c6
    fc = sub.add_parser('flash-c6', help='Flash the C6 with an ESP-AT image.')
    fc.add_argument('--variant', choices=sorted(C6_AT_VARIANTS.keys()),
                    default='mtu517',
                    help='Which ESP-AT image to flash (default: mtu517).')
    fc.add_argument('--port', '-p', default='/dev/ttyUSB1',
                    help='C6 download port - usually different from the '
                         'AT UART (default: /dev/ttyUSB1).')
    fc.add_argument('--baud', '-b', type=int, default=921600)
    fc.add_argument('--yes', '-y', action='store_true',
                    help='Skip the confirmation prompt.')

    return parser


def main() -> int:
    parser = _build_parser()
    args = parser.parse_args()

    if args.cmd is None:
        parser.print_help()
        return 1

    if args.cmd == 'upload':
        return _cmd_upload(args)
    if args.cmd == 'check-c6':
        return _cmd_check_c6(args)
    if args.cmd == 'flash-c6':
        return _cmd_flash_c6(args)

    parser.error(f'unknown subcommand: {args.cmd}')
    return 1


if __name__ == '__main__':
    raise SystemExit(main())
