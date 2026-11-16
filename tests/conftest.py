import importlib.util
from pathlib import Path
import pytest
ROOT=Path(__file__).resolve().parents[1]
@pytest.fixture(scope='session')
def host():
    spec=importlib.util.spec_from_file_location('host_build',ROOT/'tools/build_host.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module.build()
