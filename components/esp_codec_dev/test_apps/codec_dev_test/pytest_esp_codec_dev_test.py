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
def test_esp_codec_dev(dut: Dut) -> None:
    case_list = ['esp codec dev test using S3 board',
                 'Record play overlap test',
                 'Playing while recording use TDM mode',
                 'esp codec dev test using S3 board with XTAL',
                 'esp codec dev API test',
                 'esp codec dev wrong argument test',
                 'esp codec dev feature should not support']
    dut.run_all_single_board_cases(name=case_list, timeout=3600)
