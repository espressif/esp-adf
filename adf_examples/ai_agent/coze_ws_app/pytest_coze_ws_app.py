import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32', 'esp32s3', 'esp32p4'], indirect=['target'])
def test_coze_ws_app(dut: Dut)-> None:
    dut.expect(r'example_connect: Connecting to', timeout=10)
