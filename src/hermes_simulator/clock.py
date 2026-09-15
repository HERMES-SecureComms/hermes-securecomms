"""Deterministic milliseconds; no wall-clock reads or sleeps."""

from typing import Protocol


class Clock(Protocol):
    @property
    def now_ms(self) -> int: ...

    def advance(self, milliseconds: int) -> None: ...


def require_milliseconds(value: object, name: str = "milliseconds") -> int:
    if type(value) is not int or value < 0:
        raise ValueError(f"{name} must be a non-negative integer")
    return value


class VirtualClock:
    def __init__(self, start_ms: int = 0) -> None:
        self._now_ms = require_milliseconds(start_ms, "start_ms")

    @property
    def now_ms(self) -> int:
        return self._now_ms

    def advance(self, milliseconds: int) -> None:
        self._now_ms += require_milliseconds(milliseconds)


class FakeClock(VirtualClock):
    """Explicit test clock with the same deterministic contract."""

