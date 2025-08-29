#!/usr/bin/env python3
"""
Shared ESP-AT / BLE helper module (internal).

Provides low-level helpers used by `ota_ble.py` (and historically by the
retired `ble_ota_upload_at.py` upload script, which is now archived under
`tools/old/`) to talk to an ESP32 running ESP-AT BLE-client firmware on a
serial port. The ESP-AT module acts as a PC-side BLE central: the host sends
AT commands, and the module performs scan/connect/GATT writes.

Requires: pip install pyserial

This file is not intended to be invoked directly; import it from the other
scripts in this folder.
"""

import argparse
import re
import struct
import sys
import time

# Legacy OTA GATT UUIDs (matched an older BLE OTA source implementation). Kept here because the legacy
# upload helpers in this module still reference them; ota_ble.py does
# not use these.
OTA_SVC_UUID_STR = '12345678-1234-5678-1234-56789abcdef0'
OTA_CHR_CONTROL_UUID_STR = '12345678-1234-5678-1234-56789abcdef1'
OTA_CHR_DATA_UUID_STR = '12345678-1234-5678-1234-56789abcdef2'

def uuid_normalize(u):
    s = u.replace('-', '').lower().strip()
    if s.startswith('0x'):
        s = s[2:]
    return s

OTA_SVC_UUID = uuid_normalize(OTA_SVC_UUID_STR)
# ESP-AT may return 128-bit UUID in different byte order (e.g. 56781234...)
OTA_SVC_UUID_AT = '5678123456781234123456789abcdef0'
OTA_CHR_CONTROL_UUID = uuid_normalize(OTA_CHR_CONTROL_UUID_STR)
OTA_CHR_CONTROL_UUID_AT = '5678123456781234123456789abcdef1'
OTA_CHR_DATA_UUID = uuid_normalize(OTA_CHR_DATA_UUID_STR)
OTA_CHR_DATA_UUID_AT = '5678123456781234123456789abcdef2'

OTA_CMD_START = 0x01
OTA_CMD_END = 0x02
# Default fallback values; the real chunk size is computed at runtime from the
# actually negotiated ATT MTU (= MTU - 3). 20 bytes is the safe fallback that
# fits the BLE default MTU of 23.
DEFAULT_MTU = 23
DEFAULT_CHUNK_SIZE = DEFAULT_MTU - 3  # = 20
# Delay between data chunks so S3 can drain its stream buffer. We scale this
# below by the actually negotiated MTU so total throughput stays sane.
CHUNK_DELAY_SEC = 0.08

# Default baud for initial handshake; the script will optionally negotiate
# a higher rate before the data transfer phase via AT+UART_CUR.
DEFAULT_BAUD = 115200
# If non-zero, switch C6 to this baud after AT handshake and before BLE ops.
# 921600 is stable on most C6 + USB-UART bridges; set to 0 to keep 115200.
HIGH_SPEED_BAUD = 921600
SCAN_DURATION_SEC = 8
# Fast name scan when boards are close; fallback no-filter scan duration
NAME_SCAN_FAST_SEC = 3
NO_FILTER_SCAN_SEC = 5
MAX_DEVICES_TO_PROBE = 10
CMD_TIMEOUT = 2.0
WRITE_DATA_TIMEOUT = 30.0


def read_serial_lines(ser, deadline, line_callback):
    """
    Read from serial and pass complete lines to line_callback(text).
    Splits on \\r\\n or \\r or \\n so we don't miss responses that use only \\r.
    Returns when line_callback returns True (e.g. saw +BLESCANDONE) or deadline passed.
    """
    buf = b''
    while time.monotonic() < deadline:
        if ser.in_waiting:
            buf += ser.read(ser.in_waiting)
        elif buf:
            pass
        else:
            time.sleep(0.05)
            continue
        while buf:
            for sep in (b'\r\n', b'\r', b'\n'):
                if sep in buf:
                    line, _, buf = buf.partition(sep)
                    break
            else:
                break
            if not line:
                continue
            try:
                text = line.decode('utf-8', errors='replace').strip()
            except Exception:
                continue
            if text and line_callback(text):
                return True
        if not ser.in_waiting:
            time.sleep(0.005)
    return False


def open_serial(port, baud=DEFAULT_BAUD):
    try:
        import serial
    except ImportError:
        print('Install pyserial: pip install pyserial', file=sys.stderr)
        sys.exit(1)
    ser = serial.Serial(port, baud, timeout=0.2, write_timeout=5)
    # Avoid resetting C6 on open (e.g. USB-UART pulls DTR/RTS)
    try:
        ser.setDTR(False)
        ser.setRTS(False)
    except Exception:
        pass
    return ser


def read_lines(ser, timeout_sec=CMD_TIMEOUT, until_ok=True, until_error=True):
    """Read AT response lines until OK/ERROR or timeout.
    Uses tight 5ms polling so we return as soon as the response arrives without
    blocking on the serial timeout (which is 0.2s and would dominate latency)."""
    deadline = time.monotonic() + timeout_sec
    lines = []
    buf = b''
    while time.monotonic() < deadline:
        try:
            if ser.in_waiting:
                buf += ser.read(ser.in_waiting)
            else:
                time.sleep(0.005)
                continue
        except Exception:
            time.sleep(0.005)
            continue
        while buf:
            for sep in (b'\r\n', b'\r', b'\n'):
                if sep in buf:
                    line, _, buf = buf.partition(sep)
                    break
            else:
                break
            if not line:
                continue
            try:
                text = line.decode('utf-8', errors='replace').strip()
            except Exception:
                text = ''
            if not text:
                continue
            lines.append(text)
            if until_ok and text == 'OK':
                return lines
            if until_error and text == 'ERROR':
                return lines
    return lines


def send_at(ser, cmd, timeout_sec=CMD_TIMEOUT, expect_ok=True):
    if not cmd.endswith('\r\n'):
        cmd = cmd + '\r\n'
    ser.write(cmd.encode('utf-8'))
    lines = read_lines(ser, timeout_sec=timeout_sec)
    if expect_ok and lines and lines[-1] != 'OK':
        raise RuntimeError(f'AT command failed: {cmd.strip()} -> {lines}')
    return lines


def at_ble_init(ser):
    send_at(ser, 'AT+BLEINIT=1')
    time.sleep(0.5)


def at_ble_scan(ser, name_filter='OTA_Device', duration=SCAN_DURATION_SEC):
    # AT+BLESCAN=1,<duration>,2,"<name>"  (1=enable, duration sec, 2=NAME filter)
    send_at(ser, f'AT+BLESCAN=1,{duration},2,"{name_filter}"', timeout_sec=duration + 5)
    return None


def parse_scan_lines(lines):
    # +BLESCAN:<addr>,<rssi>,<adv_data>,<scan_rsp_data>,<addr_type>
    for line in lines:
        if line.startswith('+BLESCAN:'):
            rest = line[len('+BLESCAN:'):].strip()
            parts = rest.split(',', 4)
            if len(parts) >= 1 and parts[0]:
                addr = parts[0].strip('"')
                return addr
    return None


def parse_scan_lines_all(lines, max_macs=20):
    """Return list of (mac, rssi) from all +BLESCAN lines (for fallback discover)."""
    result = []
    seen = set()
    for line in lines:
        if line.startswith('+BLESCAN:'):
            rest = line[len('+BLESCAN:'):].strip()
            parts = rest.split(',', 4)
            if len(parts) >= 2 and parts[0]:
                addr = parts[0].strip('"')
                if addr in seen:
                    continue
                seen.add(addr)
                try:
                    rssi = int(parts[1]) if parts[1] else -128
                except ValueError:
                    rssi = -128
                result.append((addr, rssi))
                if len(result) >= max_macs:
                    break
    return result


def send_at_blescan_stop(ser, retries=4):
    """Send AT+BLESCAN=0; retry when C6 returns busy."""
    for _ in range(retries):
        ser.reset_input_buffer()
        ser.write(b'AT+BLESCAN=0\r\n')
        lines = read_lines(ser, timeout_sec=3)
        if lines and lines[-1] == 'OK':
            return
        time.sleep(0.8)
    raise RuntimeError('AT+BLESCAN=0 failed (busy)')


def scan_no_filter(ser, scan_duration_sec=10):
    """Scan without name filter; return list of (mac, rssi)."""
    send_at_blescan_stop(ser)
    time.sleep(0.6)  # let C6 stop scan before starting a new one
    send_at(ser, 'AT+BLESCANPARAM=1,0,0,100,50', timeout_sec=2)
    time.sleep(0.2)
    ser.reset_input_buffer()
    cmd = f'AT+BLESCAN=1,{scan_duration_sec}\r\n'
    ser.write(cmd.encode('utf-8'))
    lines = []
    deadline = time.monotonic() + scan_duration_sec + 8

    def collect(text):
        if text:
            lines.append(text)
        return '+BLESCANDONE' in text

    read_serial_lines(ser, deadline, collect)
    # Some C6 firmware may send scan results after +BLESCANDONE; read a bit more
    time.sleep(1.0)
    if ser.in_waiting:
        buf = ser.read(ser.in_waiting)
        for sep in (b'\r\n', b'\r', b'\n'):
            buf = buf.replace(sep, b'\n')
        for line in buf.split(b'\n'):
            t = line.decode('utf-8', errors='replace').strip()
            if t and t not in lines:
                lines.append(t)
    send_at_blescan_stop(ser)
    return parse_scan_lines_all(lines, max_macs=50)


def try_connect_and_has_ota_service(ser, mac, conn_index=0):
    """Connect to mac, discover services/chars; return (True, srv_idx, ctrl_idx, data_idx, mtu)
    if OTA found (stays connected), else (False, None, None, None, DEFAULT_MTU)."""
    try:
        at_ble_conn(ser, mac, conn_index)
        mtu = at_ble_set_mtu(ser, conn_index, 512)
        at_ble_set_conn_interval(ser, conn_index)
        time.sleep(1.2)
        services = at_ble_gattc_prim_srv(ser, conn_index)
        time.sleep(0.8)
        srv_index = None
        for idx, uuid in services:
            u = uuid_normalize(uuid) if isinstance(uuid, str) else uuid
            if u == OTA_SVC_UUID or u == OTA_SVC_UUID_AT:
                srv_index = idx
                break
        if srv_index is None:
            raise ValueError('no OTA service')
        chars = at_ble_gattc_char(ser, conn_index, srv_index)
        time.sleep(0.8)
        ctrl_idx = None
        data_idx = None
        for char_index, char_uuid in chars:
            u = uuid_normalize(char_uuid) if isinstance(char_uuid, str) else char_uuid
            if u == OTA_CHR_CONTROL_UUID or u == OTA_CHR_CONTROL_UUID_AT:
                ctrl_idx = char_index
            if u == OTA_CHR_DATA_UUID or u == OTA_CHR_DATA_UUID_AT:
                data_idx = char_index
        if ctrl_idx is None or data_idx is None:
            raise ValueError('OTA char not found')
        return (True, srv_index, ctrl_idx, data_idx, mtu)
    except Exception:
        pass
    try:
        send_at(ser, f'AT+BLEDISCONN={conn_index}', timeout_sec=3)
    except Exception:
        pass
    time.sleep(0.5)
    return (False, None, None, None, DEFAULT_MTU)


def scan_and_get_mac(ser, name_filter='OTA_Device', scan_duration_sec=None):
    if scan_duration_sec is None:
        scan_duration_sec = NAME_SCAN_FAST_SEC
    at_ble_init(ser)
    # Active scan so we get scan response (device name is often in scan_rsp)
    send_at(ser, 'AT+BLESCANPARAM=1,0,0,100,50', timeout_sec=2)
    time.sleep(0.2)
    send_at_blescan_stop(ser)  # stop any previous scan
    time.sleep(0.3)
    ser.reset_input_buffer()
    # Start scan with filter; OK may come before +BLESCAN (per ESP-AT doc), so only stop on +BLESCANDONE
    cmd = f'AT+BLESCAN=1,{scan_duration_sec},2,"{name_filter}"\r\n'
    ser.write(cmd.encode('utf-8'))
    lines = []
    deadline = time.monotonic() + scan_duration_sec + 6

    def collect_line(text):
        if text:
            lines.append(text)
        return '+BLESCANDONE' in text

    read_serial_lines(ser, deadline, collect_line)
    mac = parse_scan_lines(lines)
    send_at_blescan_stop(ser)
    return mac


def at_ble_conn(ser, mac, conn_index=0):
    send_at(ser, f'AT+BLECONN={conn_index},"{mac}"', timeout_sec=35)


def at_ble_set_conn_interval(ser, conn_index=0):
    """Request a short BLE connection interval so GATT writes complete faster.
    AT+BLECONNPARAM=<conn_index>,<min_interval>,<max_interval>,<latency>,<timeout>
    All intervals in units of 1.25ms. min=6 → 7.5ms, max=12 → 15ms.
    If C6 returns ERROR (older AT firmware), continue with default interval."""
    try:
        send_at(ser, f'AT+BLECONNPARAM={conn_index},6,12,0,500', timeout_sec=5)
        time.sleep(0.3)  # allow peripheral to accept/reject the update
        print('  BLE connection interval requested: 7.5–15ms')
    except Exception as e:
        print(f'  (BLECONNPARAM not supported or failed: {e}; using default interval)')


def at_ble_set_mtu(ser, conn_index=0, mtu=512):
    """Trigger BLE ATT MTU exchange on ESP-AT.

    ESP-AT (NimBLE-based) compiles the MTU cap as a build-time macro
    (BLE_ATT_PREFERRED_MTU_VAL, typically 203 on the default C6 AT image).
    The command therefore only takes a conn_index; the stack itself negotiates
    the actual value. We issue the command, wait for the exchange, then read
    back the result. Returns the negotiated MTU as an int (DEFAULT_MTU on
    failure, 203 if the query reports nothing but we know the C6 default).
    """
    # Trigger MTU exchange — single-arg form only (no value parameter)
    try:
        send_at(ser, f'AT+BLECFGMTU={conn_index}', timeout_sec=5)
        time.sleep(0.8)  # allow exchange frames to complete before GATT ops
    except Exception as e:
        print(f'  (BLECFGMTU trigger failed: {e}, probing anyway)')

    # Read back the negotiated value
    # +BLECFGMTU:<conn_index>,<mtu>
    actual = DEFAULT_MTU
    try:
        lines = send_at(ser, 'AT+BLECFGMTU?', timeout_sec=2)
        for line in lines:
            if line.startswith('+BLECFGMTU:'):
                rest = line[len('+BLECFGMTU:'):].strip()
                parts = rest.split(',')
                if len(parts) >= 2:
                    try:
                        idx = int(parts[0])
                        val = int(parts[1])
                        if idx == conn_index and val >= 23:
                            actual = val
                            break
                    except ValueError:
                        pass
    except Exception:
        pass

    # The default ESP-AT C6 image compiles MTU=203; if the query returned
    # nothing (no active connection yet when queried) fall back to that known
    # value rather than the BLE minimum.
    if actual == DEFAULT_MTU:
        print('  (BLECFGMTU? returned no result; assuming C6 AT default MTU=203)')
        actual = 203

    return actual


def at_ble_gattc_prim_srv(ser, conn_index=0):
    lines = send_at(ser, f'AT+BLEGATTCPRIMSRV={conn_index}')
    # +BLEGATTCPRIMSRV:<conn_index>,<srv_index>,<srv_uuid>,<srv_type>
    result = []
    for line in lines:
        if line.startswith('+BLEGATTCPRIMSRV:'):
            rest = line[len('+BLEGATTCPRIMSRV:'):].strip()
            parts = rest.split(',', 3)
            if len(parts) >= 4:
                try:
                    idx = int(parts[1])
                    uuid = uuid_normalize(parts[2].strip('"'))
                    result.append((idx, uuid))
                except (ValueError, IndexError):
                    pass
    return result


def at_ble_gattc_char(ser, conn_index, srv_index):
    lines = send_at(ser, f'AT+BLEGATTCCHAR={conn_index},{srv_index}')
    # +BLEGATTCCHAR:"char",<conn_index>,<srv_index>,<char_index>,<char_uuid>,<char_prop>
    result = []
    for line in lines:
        if '"char"' in line and line.startswith('+BLEGATTCCHAR:'):
            rest = line[len('+BLEGATTCCHAR:'):].strip()
            parts = rest.split(',', 5)
            if len(parts) >= 6:
                try:
                    char_index = int(parts[3])
                    char_uuid = uuid_normalize(parts[4].strip('"'))
                    result.append((char_index, char_uuid))
                except (ValueError, IndexError):
                    pass
    return result


def at_ble_gattc_wr(ser, conn_index, srv_index, char_index, data, timeout_sec=WRITE_DATA_TIMEOUT):
    # AT+BLEGATTCWR=conn,srv,char,,length
    length = len(data)
    for attempt in range(12):
        ser.reset_input_buffer()
        ser.write(f'AT+BLEGATTCWR={conn_index},{srv_index},{char_index},,{length}\r\n'.encode('utf-8'))
        # Do NOT sleep here — poll tightly (5ms) so we catch the ">" prompt as
        # soon as C6 replies (~10ms) without the old 300ms hard wait.
        deadline = time.monotonic() + min(timeout_sec, 15)
        buf = b''
        all_recv = b''
        got_prompt = False
        busy = False
        while time.monotonic() < deadline:
            try:
                if ser.in_waiting:
                    chunk = ser.read(ser.in_waiting)
                    buf += chunk
                    all_recv += chunk
                else:
                    time.sleep(0.005)
                    continue
            except Exception:
                time.sleep(0.005)
                continue
            while buf:
                for sep in (b'\r\n', b'\r', b'\n'):
                    if sep in buf:
                        line, _, buf = buf.partition(sep)
                        break
                else:
                    break
                if not line:
                    continue
                try:
                    text = line.decode('utf-8', errors='replace').strip()
                except Exception:
                    continue
                if text == 'ERROR':
                    raise RuntimeError('BLEGATTCWR returned ERROR before data')
                if 'busy' in text.lower():
                    busy = True
                if '>' in text or text.strip() == '>':
                    got_prompt = True
                    break
            if got_prompt:
                break
            if b'>' in buf:
                got_prompt = True
                break
        if got_prompt:
            break
        if busy and attempt < 11:
            print(f'    (C6 busy on write attempt {attempt + 1}, retrying in 4s)', file=sys.stderr)
            time.sleep(4.0)
            continue
        if not got_prompt:
            while ser.in_waiting:
                all_recv += ser.read(ser.in_waiting)
            raw = all_recv.decode('utf-8', errors='replace') if all_recv else '(no data)'
            raise RuntimeError("BLEGATTCWR did not receive '>' prompt (got: %r)" % (raw[:200],))
    # Send raw bytes
    ser.write(data)
    # Wait for OK
    lines = read_lines(ser, timeout_sec=timeout_sec)
    if lines and lines[-1] != 'OK':
        raise RuntimeError(f'BLEGATTCWR data response: {lines}')


def _uuid_match(u, standard, at_form):
    u = uuid_normalize(u) if isinstance(u, str) else u
    return u == standard or u == at_form


def find_esp_ota_service_and_chars(ser, conn_index=0):
    services = at_ble_gattc_prim_srv(ser, conn_index)
    time.sleep(1.5)  # let C6 finish service discovery before next GATT op
    srv_index = None
    for idx, uuid in services:
        if _uuid_match(uuid, OTA_SVC_UUID, OTA_SVC_UUID_AT):
            srv_index = idx
            break
    if srv_index is None:
        raise RuntimeError('OTA GATT service not found on device')
    chars = at_ble_gattc_char(ser, conn_index, srv_index)
    time.sleep(2.0)  # let C6 finish char discovery before first write
    ctrl_idx = None
    data_idx = None
    for char_index, char_uuid in chars:
        if _uuid_match(char_uuid, OTA_CHR_CONTROL_UUID, OTA_CHR_CONTROL_UUID_AT):
            ctrl_idx = char_index
        if _uuid_match(char_uuid, OTA_CHR_DATA_UUID, OTA_CHR_DATA_UUID_AT):
            data_idx = char_index
    if ctrl_idx is None or data_idx is None:
        raise RuntimeError('OTA Control or Data characteristic not found')
    return srv_index, ctrl_idx, data_idx


def upload(ser, path, device_name='OTA_Device', conn_index=0, scan_duration_sec=None, chunk_delay_sec=None):
    if chunk_delay_sec is None:
        chunk_delay_sec = CHUNK_DELAY_SEC
    with open(path, 'rb') as f:
        data = f.read()
    size = len(data)
    if size == 0:
        raise RuntimeError(f'Firmware file is empty: {path}')

    at_ble_init(ser)

    # Fast path: short name scan first (best when boards are close)
    name_scan_sec = NAME_SCAN_FAST_SEC
    print(f'Scanning for "{device_name}" (%d s)...' % name_scan_sec)
    mac = scan_and_get_mac(ser, name_filter=device_name, scan_duration_sec=name_scan_sec)
    connected = False
    srv_index = ctrl_idx = data_idx = None
    mtu = DEFAULT_MTU
    if mac:
        print(f'  Found {mac}, connecting and checking for OTA service...')
        ok, srv_index, ctrl_idx, data_idx, mtu = try_connect_and_has_ota_service(ser, mac, conn_index)
        if ok:
            connected = True
            print(f'  OTA service found on {mac}')

    # Fallback: no-filter scan and probe devices by RSSI
    if not connected:
        send_at_blescan_stop(ser)
        time.sleep(0.6)
        at_ble_init(ser)  # reset BLE state after name-filter scan so no-filter scan works
        no_filter_sec = NO_FILTER_SCAN_SEC
        print('  No OTA device by name. Scanning all BLE devices (%d s)...' % no_filter_sec)
        macs = scan_no_filter(ser, scan_duration_sec=no_filter_sec)
        print(f'  Found {len(macs)} device(s) to probe.')
        if len(macs) == 0:
            print('  Hint: ensure S3 is powered and advertising (OTA_Device). Check C6 antenna and BLE init.')
        macs_sorted = sorted(macs, key=lambda x: -x[1])[:MAX_DEVICES_TO_PROBE]
        for m, rssi in macs_sorted:
            print(f'  Trying {m} (rssi={rssi})...')
            ok, srv_index, ctrl_idx, data_idx, mtu = try_connect_and_has_ota_service(ser, m, conn_index)
            if ok:
                mac = m
                connected = True
                print(f'  Found OTA service on {m}')
                break
        if not connected:
            longer = scan_duration_sec if scan_duration_sec is not None else SCAN_DURATION_SEC
            mac = scan_and_get_mac(ser, name_filter=device_name, scan_duration_sec=longer)
    if not mac:
        raise RuntimeError(f'No BLE device with name "{device_name}" or OTA service found.')

    if not connected:
        print(f'Connecting to {mac}...')
        at_ble_conn(ser, mac, conn_index)
        mtu = at_ble_set_mtu(ser, conn_index, 512)
        at_ble_set_conn_interval(ser, conn_index)
        time.sleep(2.5)
        srv_index, ctrl_idx, data_idx = find_esp_ota_service_and_chars(ser, conn_index)

    # Compute the actual chunk size from the negotiated ATT MTU (max payload
    # per Write Request is MTU - 3 bytes). If MTU exchange did not bring it
    # above 23, we have to fall back to 20-byte chunks; that is the only safe
    # value that the C6-AT bridge will not silently truncate.
    chunk_size = max(1, mtu - 3)
    # BLE connection interval is the real bottleneck; each AT+BLEGATTCWR
    # round-trip already takes 1-2 connection intervals (~20ms). No extra
    # delay is needed: the S3 stream buffer absorbs bursts, and C6 AT
    # signals back-pressure via "busy" which the write loop retries.
    local_chunk_delay = 0.0

    print(f'OTA service index={srv_index}, control char={ctrl_idx}, data char={data_idx}')
    print(f'Negotiated ATT MTU={mtu}, using chunk_size={chunk_size}, chunk_delay={local_chunk_delay:.3f}s')

    # Optional: one GATT read (e.g. device name from 0x1800) to let C6 finish internal GATT state
    try:
        send_at(ser, f'AT+BLEGATTCRD={conn_index},1,0', timeout_sec=5)
        time.sleep(1.0)
    except Exception:
        pass
    # CMD_START + size (4 bytes LE)
    start_payload = struct.pack('<BI', OTA_CMD_START, size)
    time.sleep(3.0)
    at_ble_gattc_wr(ser, conn_index, srv_index, ctrl_idx, start_payload)
    print(f'Sent OTA start, size={size}')

    offset = 0
    last_report_bytes = 0
    last_report_t = time.monotonic()
    start_t = last_report_t
    chunk_count = 0
    while offset < size:
        chunk = data[offset:offset + chunk_size]
        chunk_t0 = time.monotonic()
        at_ble_gattc_wr(ser, conn_index, srv_index, data_idx, chunk)
        chunk_dt = time.monotonic() - chunk_t0
        offset += len(chunk)
        chunk_count += 1
        time.sleep(local_chunk_delay)  # match S3 receive rate to avoid buffer drop
        # Print a status line at most ~once per second OR every 4 KiB, whichever
        # comes first. Always print the very first chunk and the very last.
        now = time.monotonic()
        bytes_since = offset - last_report_bytes
        time_since = now - last_report_t
        if (chunk_count == 1 or offset == size or bytes_since >= 4096 or time_since >= 1.0):
            elapsed = now - start_t
            pct = (offset * 100.0) / size if size else 0.0
            inst_rate = bytes_since / time_since if time_since > 0 else 0.0
            avg_rate = offset / elapsed if elapsed > 0 else 0.0
            eta_str = f'{(size - offset) / avg_rate:5.0f}s' if avg_rate > 0 else '  ?  s'
            print(f'  [{pct:5.1f}%] {offset}/{size} B'
                  f'  chunks={chunk_count} (last write={chunk_dt*1000:.0f}ms, payload={len(chunk)}B)'
                  f'  inst={inst_rate:6.0f} B/s avg={avg_rate:6.0f} B/s  ETA={eta_str}',
                  flush=True)
            last_report_bytes = offset
            last_report_t = now
    total_elapsed = time.monotonic() - start_t
    print(f'  Upload finished: {size} bytes in {total_elapsed:.1f}s '
          f'(avg {size/total_elapsed:.0f} B/s, {chunk_count} chunks @ {chunk_size} B/payload)',
          flush=True)

    at_ble_gattc_wr(ser, conn_index, srv_index, ctrl_idx, bytes([OTA_CMD_END]))
    print('Sent OTA end. Device will reboot when upgrade completes.')
    return True


def main():
    parser = argparse.ArgumentParser(
        description='BLE OTA upload via ESP-AT bridge (serial port)'
    )
    parser.add_argument('firmware', nargs='?', help='Path to firmware .bin file (required unless --scan-only)')
    parser.add_argument('--port', '-p', default='/dev/ttyUSB1', help='Serial port (default: /dev/ttyUSB1)')
    parser.add_argument('--device', '-d', default='OTA_Device', help='BLE device name to scan (default: OTA_Device)')
    parser.add_argument('--baud', '-b', type=int, default=DEFAULT_BAUD, help='Serial baud rate (default: 115200)')
    parser.add_argument('--scan-duration', type=int, default=SCAN_DURATION_SEC, help='BLE scan duration in seconds (default: %s)' % SCAN_DURATION_SEC)
    parser.add_argument('--chunk-delay', type=float, default=CHUNK_DELAY_SEC, help='Per-chunk delay (s) lower-bound; actual delay scales with negotiated MTU (default: %.2f)' % CHUNK_DELAY_SEC)
    parser.add_argument('--high-baud', type=int, default=HIGH_SPEED_BAUD,
                        help='Switch C6 to this baud after AT handshake (0 = keep --baud). '
                             'Default: %d. Serial ceiling = high_baud * 0.858 B/s' % HIGH_SPEED_BAUD)
    parser.add_argument('--scan-only', action='store_true', help='Only run BLE scan and print results, then exit')
    args = parser.parse_args()
    if not args.scan_only and not args.firmware:
        parser.error('firmware path is required unless --scan-only')

    ser = open_serial(args.port, args.baud)
    # Wait for AT ready (C6 may have been reset by previous open or is still booting)
    deadline = time.monotonic() + 12
    while time.monotonic() < deadline:
        try:
            send_at(ser, 'AT', timeout_sec=2)
            break
        except Exception:
            time.sleep(0.5)
    else:
        raise RuntimeError('AT not ready on %s' % args.port)
    try:
        send_at(ser, 'ATE0', timeout_sec=1)
    except Exception:
        pass

    # Optionally switch to a higher baud rate before BLE ops to increase
    # the serial throughput ceiling (115200 caps BLE OTA at ~9.9 KB/s).
    target_baud = args.high_baud
    if target_baud and target_baud != args.baud:
        try:
            # AT+UART_CUR changes baud immediately; the command is acknowledged
            # at the OLD rate, then C6 switches.  We must reopen at the new rate.
            ser.write(f'AT+UART_CUR={target_baud},8,1,0,0\r\n'.encode())
            time.sleep(0.1)   # let C6 switch before we reopen
            ser.close()
            ser = open_serial(args.port, target_baud)
            time.sleep(0.05)
            send_at(ser, 'AT', timeout_sec=2)
            print(f'  Serial switched to {target_baud} baud '
                  f'(serial ceiling now {target_baud * 0.858 / 1024:.1f} KB/s)')
        except Exception as e:
            print(f'  (Baud switch to {target_baud} failed: {e}; falling back to {args.baud})')
            try:
                ser.close()
            except Exception:
                pass
            ser = open_serial(args.port, args.baud)
            send_at(ser, 'AT', timeout_sec=2)

    if args.scan_only:
        at_ble_init(ser)
        send_at(ser, 'AT+BLESCANPARAM=1,0,0,100,50', timeout_sec=2)
        time.sleep(0.2)
        send_at_blescan_stop(ser)
        time.sleep(0.3)
        ser.reset_input_buffer()
        dur = args.scan_duration
        print('Scanning for %d seconds (no name filter)...' % dur)
        ser.write(('AT+BLESCAN=1,%d\r\n' % dur).encode('utf-8'))
        deadline = time.monotonic() + dur + 6

        def on_line(text):
            if text:
                print(text)
            return '+BLESCANDONE' in text

        read_serial_lines(ser, deadline, on_line)
        send_at_blescan_stop(ser)
        ser.close()
        print('Scan done.')
        return

    try:
        import serial
        serial_exc = serial.SerialException
    except ImportError:
        serial_exc = Exception

    upload_ok = False
    for attempt in range(2):
        try:
            upload(ser, args.firmware, device_name=args.device, scan_duration_sec=args.scan_duration, chunk_delay_sec=args.chunk_delay)
            upload_ok = True
            break
        except serial_exc as e:
            ser.close()
            if attempt == 0:
                print('Serial error, retrying once...', file=sys.stderr)
                time.sleep(1.5)
                ser = open_serial(args.port, args.baud)
                for _ in range(24):
                    try:
                        send_at(ser, 'AT', timeout_sec=2)
                        break
                    except Exception:
                        time.sleep(0.5)
                try:
                    send_at(ser, 'ATE0', timeout_sec=1)
                except Exception:
                    pass
            else:
                print(f'Serial error: {e}', file=sys.stderr)
                sys.exit(1)
        except RuntimeError as e:
            ser.close()
            print(f'Error: {e}', file=sys.stderr)
            sys.exit(2)
    ser.close()
    if upload_ok:
        print('Done.')


if __name__ == '__main__':
    main()
