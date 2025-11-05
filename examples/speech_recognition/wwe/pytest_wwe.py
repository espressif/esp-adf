import pytest
from pytest_embedded import Dut

@pytest.mark.esp32
@pytest.mark.esp32s3
@pytest.mark.esp32p4
@pytest.mark.ADF_EXAMPLE_GENERIC
def test_wwe(dut: Dut)-> None:
    dut.expect(r'Successfully load srmodels', timeout=10)
    dut.expect(r'Returned from app_main', timeout=20)
