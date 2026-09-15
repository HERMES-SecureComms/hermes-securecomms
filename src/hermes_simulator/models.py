"""Immutable domain values and policy input snapshots."""

from dataclasses import dataclass
from enum import StrEnum

from .clock import require_milliseconds

DEFAULT_GRACE_PERIOD_MS = 1500
DEFAULT_DEVICE_ID = "HPTT-001"


class AuthState(StrEnum):
    UNAUTHENTICATED = "UNAUTHENTICATED"
    AUTHORIZED = "AUTHORIZED"
    GRACE_PERIOD = "GRACE_PERIOD"
    REVOKED = "REVOKED"
    SYSTEM_FAILURE = "SYSTEM_FAILURE"


class PttState(StrEnum):
    RELEASED = "RELEASED"
    PRESSED = "PRESSED"


class TxDecision(StrEnum):
    ALLOW = "ALLOW"
    DENY_NO_CREDENTIAL = "DENY_NO_CREDENTIAL"
    DENY_UNKNOWN_CREDENTIAL = "DENY_UNKNOWN_CREDENTIAL"
    DENY_CREDENTIAL_EXPIRED = "DENY_CREDENTIAL_EXPIRED"
    DENY_DEVICE_REVOKED = "DENY_DEVICE_REVOKED"
    DENY_SYSTEM_FAILURE = "DENY_SYSTEM_FAILURE"
    DENY_CREDENTIAL_REVOKED = "DENY_CREDENTIAL_REVOKED"
    DENY_CREDENTIAL_UNAUTHORIZED = "DENY_CREDENTIAL_UNAUTHORIZED"
    DENY_REAUTHENTICATION_REQUIRED = "DENY_REAUTHENTICATION_REQUIRED"
    DENY_PTT_RELEASED = "DENY_PTT_RELEASED"
    DENY_INVALID_STATE = "DENY_INVALID_STATE"


def require_identifier(value: object, name: str) -> str:
    if not isinstance(value, str) or not value.strip() or any(
        character.isspace() or not character.isprintable() for character in value
    ):
        raise ValueError(f"{name} must be a non-empty identifier without whitespace")
    return value


@dataclass(frozen=True)
class Credential:
    credential_id: str
    operator_id: str
    authorized: bool = True
    revoked: bool = False

    def __post_init__(self) -> None:
        require_identifier(self.credential_id, "credential_id")
        require_identifier(self.operator_id, "operator_id")
        if type(self.authorized) is not bool or type(self.revoked) is not bool:
            raise ValueError("credential authorized/revoked must be booleans")


@dataclass(frozen=True)
class Device:
    device_id: str = DEFAULT_DEVICE_ID
    revoked: bool = False
    system_healthy: bool = True

    def __post_init__(self) -> None:
        require_identifier(self.device_id, "device_id")
        if type(self.revoked) is not bool or type(self.system_healthy) is not bool:
            raise ValueError("device revoked/system_healthy must be booleans")


@dataclass(frozen=True)
class Configuration:
    device_id: str = DEFAULT_DEVICE_ID
    grace_period_ms: int = DEFAULT_GRACE_PERIOD_MS
    credentials: tuple[Credential, ...] = (
        Credential("GLOVE-001", "OPERATOR-001"),
    )

    def __post_init__(self) -> None:
        require_identifier(self.device_id, "device_id")
        require_milliseconds(self.grace_period_ms, "grace_period_ms")
        ids = [credential.credential_id for credential in self.credentials]
        if len(ids) != len(set(ids)):
            raise ValueError("duplicate credential_id in configuration")


@dataclass(frozen=True)
class PolicyContext:
    device: Device
    credential: Credential | None
    attached_credential_id: str | None
    auth_state: AuthState
    ptt_state: PttState
    session_valid: bool
    grace_deadline_ms: int | None
    now_ms: int
    denial_reason: TxDecision

