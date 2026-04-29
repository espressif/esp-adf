import pytest
from pytest_embedded import Dut

@pytest.mark.esp32s3
@pytest.mark.ADF_EXAMPLE_GENERIC
@pytest.mark.parametrize(
    'config',
    [
        'defaults',
    ],
    indirect=True,
)
def test_esp_coze(dut: Dut) -> None:
    dut.run_all_single_board_cases(timeout=3000)
