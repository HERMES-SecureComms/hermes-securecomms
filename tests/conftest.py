from pathlib import Path

import pytest

from hermes_simulator.clock import FakeClock
from hermes_simulator.models import Configuration, Credential
from hermes_simulator.simulator import Simulator

ROOT = Path(__file__).resolve().parents[1]


@pytest.fixture
def clock() -> FakeClock:
    return FakeClock()


@pytest.fixture
def sim(clock: FakeClock) -> Simulator:
    return Simulator(Configuration(credentials=(
        Credential("GLOVE-001", "OPERATOR-001"),
        Credential("GLOVE-002", "OPERATOR-002"),
        Credential("BLOCKED", "OPERATOR-003", authorized=False),
    )), clock)

