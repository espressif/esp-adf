# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import socket
import struct
import time
from urllib.parse import urlparse

import pytest
from pytest_embedded import Dut


MQTT_BROKER_URI = 'mqtt://broker.emqx.io'
MQTT_WAKE_TOPIC = '/gmf/audio_power_save/wakeup'
MQTT_WAKE_PAYLOAD = 'pytest_mqtt_wakeup'


def _mqtt_remaining_length(length: int) -> bytes:
    encoded = bytearray()
    while True:
        digit = length % 128
        length //= 128
        if length > 0:
            digit |= 0x80
        encoded.append(digit)
        if length == 0:
            return bytes(encoded)


def _mqtt_string(value: str) -> bytes:
    data = value.encode()
    return struct.pack('!H', len(data)) + data


def mqtt_publish(uri: str, topic: str, payload: str) -> None:
    parsed = urlparse(uri)
    host = parsed.hostname
    port = parsed.port or 1883
    client_id = f'audio_power_save_pytest_{int(time.time())}'

    variable_header = _mqtt_string('MQTT') + bytes([4, 2, 0, 30])
    connect_payload = _mqtt_string(client_id)
    connect = b'\x10' + _mqtt_remaining_length(len(variable_header) + len(connect_payload))
    connect += variable_header + connect_payload

    topic_bytes = _mqtt_string(topic)
    payload_bytes = payload.encode()
    publish = b'\x30' + _mqtt_remaining_length(len(topic_bytes) + len(payload_bytes))
    publish += topic_bytes + payload_bytes

    with socket.create_connection((host, port), timeout=10) as sock:
        sock.sendall(connect)
        connack = sock.recv(4)
        if connack != b'\x20\x02\x00\x00':
            raise RuntimeError(f'MQTT CONNACK failed: {connack!r}')
        sock.sendall(publish)
        sock.sendall(b'\xe0\x00')


def _run_wakeup_flow(dut: Dut) -> None:
    dut.expect(r'Connect Wi-Fi, ssid:.*, listen_interval:10', timeout=60)
    dut.expect(r'MQTT connected, broker=.*, keepalive=\d+ s', timeout=30)
    dut.expect(r'Player idle; waiting for wakeup \(UART/MQTT/GPIO/timer\) in automatic light sleep', timeout=30)
    time.sleep(3)
    dut.write('123456789ABCDE\r\n')
    dut.expect(r'Wakeup handled by UART', timeout=30)
    dut.expect(r'Player idle; waiting for wakeup \(UART/MQTT/GPIO/timer\) in automatic light sleep', timeout=30)
    time.sleep(3)
    mqtt_publish(MQTT_BROKER_URI, MQTT_WAKE_TOPIC, MQTT_WAKE_PAYLOAD)
    dut.expect(r'Wakeup handled by MQTT', timeout=30)
    dut.expect(r'Wakeup validation done', timeout=30)
    dut.expect(r'Example finished', timeout=30)


@pytest.mark.parametrize('target', ['esp32s3'], indirect=['target'])
def test_audio_power_save_wakeup(dut: Dut) -> None:
    _run_wakeup_flow(dut)
