# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32s3'], indirect=['target'])
def test_esp_config_manager_simple_example(dut: IdfDut) -> None:
    dut.expect(r'Storage backend: NVS \(3 config groups\)', timeout=60)
    dut.expect(r'\[TEST\] g0\.packed_layout PASS', timeout=60)
    dut.expect(r'\[TEST\] g0\.wipe_storage PASS', timeout=60)
    dut.expect(r'\[TEST\] g0\.load_defaults_after_wipe PASS', timeout=60)
    dut.expect(r'\[TEST\] g0\.save_load_roundtrip PASS', timeout=60)
    dut.expect(r'\[TEST\] g0\.corrupt_primary_blob PASS', timeout=60)
    dut.expect(r'\[TEST\] g0\.fallback_to_backup_crc PASS', timeout=60)
    dut.expect(r'\[TEST\] g0\.primary_repaired_after_load PASS', timeout=60)
    dut.expect(r'\[TEST\] g0\.merge_tail_new_fields PASS', timeout=60)
    dut.expect(r'\[TEST\] g1\.defaults PASS', timeout=60)
    dut.expect(r'\[TEST\] g1\.u32_roundtrip PASS', timeout=60)
    dut.expect(r'\[TEST\] g2\.defaults PASS', timeout=60)
    dut.expect(r'\[TEST\] g2\.u32_roundtrip PASS', timeout=60)
    dut.expect(r'======== selftest end ========', timeout=60)
    dut.expect(r'Done', timeout=60)
