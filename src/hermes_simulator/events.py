"""Structured, immutable events; presentation is the CLI's responsibility."""

from dataclasses import asdict, dataclass
from enum import StrEnum


class EventType(StrEnum):
    CREDENTIAL_ATTACHED = "CREDENTIAL_ATTACHED"
    AUTH_SUCCESS = "AUTH_SUCCESS"
    AUTH_FAILED = "AUTH_FAILED"
    CREDENTIAL_LOST = "CREDENTIAL_LOST"
    GRACE_STARTED = "GRACE_STARTED"
    AUTH_EXPIRED = "AUTH_EXPIRED"
    AUTHORIZATION_INVALIDATED = "AUTHORIZATION_INVALIDATED"
    PTT_PRESSED = "PTT_PRESSED"
    PTT_RELEASED = "PTT_RELEASED"
    TX_ALLOWED = "TX_ALLOWED"
    TX_DENIED = "TX_DENIED"
    TX_DISABLED = "TX_DISABLED"
    DEVICE_REVOKED = "DEVICE_REVOKED"
    DEVICE_RESTORED = "DEVICE_RESTORED"
    CREDENTIAL_REVOKED = "CREDENTIAL_REVOKED"
    SYSTEM_FAULT = "SYSTEM_FAULT"
    SYSTEM_RECOVERED = "SYSTEM_RECOVERED"
    SYSTEM_REBOOT = "SYSTEM_REBOOT"
    TIME_ADVANCED = "TIME_ADVANCED"


@dataclass(frozen=True)
class Event:
    timestamp: int
    event_type: EventType
    device_id: str
    credential_id: str | None = None
    operator_id: str | None = None
    reason: str | None = None
    milliseconds: int | None = None

    def to_dict(self) -> dict[str, object]:
        return asdict(self)

