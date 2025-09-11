import pytest
from pytest_embedded import Dut

@pytest.mark.esp32
@pytest.mark.esp32s3
@pytest.mark.esp32p4
@pytest.mark.ADF_EXAMPLE_GENERIC
def test_dueros(dut: Dut)-> None:
    dut.expect(r'Returned from app_main', timeout=20)
    dut.expect(r'Got ip', timeout=100)
