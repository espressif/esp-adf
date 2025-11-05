import pytest
from pytest_embedded import Dut

@pytest.mark.esp32
@pytest.mark.esp32s3
@pytest.mark.ADF_EXAMPLE_GENERIC
def test_vad(dut: Dut)-> None:
    dut.expect(r'Initialize VAD handle', timeout=10)
