import pytest

from pytest_embedded import Dut

@pytest.mark.esp32
@pytest.mark.esp32s3
@pytest.mark.esp32p4
@pytest.mark.ADF_EXAMPLE_GENERIC
def test_coze_ws_app(dut: Dut)-> None:
    dut.expect(r'example_connect: Connecting to', timeout=10)
