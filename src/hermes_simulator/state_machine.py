"""Explicit domain transitions, independent of command parsing and printing."""

from dataclasses import replace

from .clock import Clock
from .events import Event, EventType
from .models import (
    AuthState, Configuration, Credential, Device, PolicyContext, PttState,
    TxDecision, require_identifier,
)
from .policy import evaluate


class StateMachine:
    def __init__(self, configuration: Configuration, clock: Clock) -> None:
        self.clock = clock
        self.configuration = configuration
        self.device = Device(configuration.device_id)
        self._credentials = {c.credential_id: c for c in configuration.credentials}
        self.attached_credential_id: str | None = None
        self._session_credential_id: str | None = None
        self.auth_state = AuthState.UNAUTHENTICATED
        self.ptt_state = PttState.RELEASED
        self.session_valid = False
        self.grace_deadline_ms: int | None = None
        self.denial_reason = TxDecision.DENY_NO_CREDENTIAL
        self._decision = TxDecision.DENY_NO_CREDENTIAL
        self._events: list[Event] = []

    @property
    def events(self) -> tuple[Event, ...]:
        return tuple(self._events)

    @property
    def credential(self) -> Credential | None:
        identity = self.attached_credential_id or self._session_credential_id
        return self._credentials.get(identity) if identity else None

    def context(self) -> PolicyContext:
        return PolicyContext(
            self.device, self.credential, self.attached_credential_id,
            self.auth_state, self.ptt_state, self.session_valid,
            self.grace_deadline_ms, self.clock.now_ms, self.denial_reason,
        )

    @property
    def decision(self) -> TxDecision:
        self.refresh()
        return self._decision

    def emit(
        self, event_type: EventType, reason: str | None = None,
        *, credential_id: str | None = None, milliseconds: int | None = None,
    ) -> None:
        identity = credential_id or self.attached_credential_id or self._session_credential_id
        credential = self._credentials.get(identity) if identity else None
        self._events.append(Event(
            self.clock.now_ms, event_type, self.device.device_id, identity,
            credential.operator_id if credential else None, reason, milliseconds,
        ))

    def _reconcile_tx(self, force_denial: bool = False, force_disabled: bool = False) -> None:
        previous = self._decision
        self._decision = evaluate(self.context())
        if self._decision is TxDecision.ALLOW:
            if previous is not TxDecision.ALLOW:
                self.emit(EventType.TX_ALLOWED)
        else:
            if previous is TxDecision.ALLOW or force_disabled:
                self.emit(EventType.TX_DISABLED, self._decision.value)
            if self.ptt_state is PttState.PRESSED and (
                previous != self._decision or force_denial
            ):
                self.emit(EventType.TX_DENIED, self._decision.value)

    def refresh(self) -> None:
        if (
            self.auth_state is AuthState.GRACE_PERIOD
            and self.grace_deadline_ms is not None
            and self.clock.now_ms >= self.grace_deadline_ms
        ):
            self._invalidate(TxDecision.DENY_CREDENTIAL_EXPIRED)
            self.emit(EventType.AUTH_EXPIRED, self.denial_reason.value)
        self._reconcile_tx()

    def _invalidate(self, reason: TxDecision) -> None:
        self.session_valid = False
        self.grace_deadline_ms = None
        self.denial_reason = reason
        if not self.device.system_healthy:
            self.auth_state = AuthState.SYSTEM_FAILURE
        elif self.device.revoked or reason is TxDecision.DENY_CREDENTIAL_REVOKED:
            self.auth_state = AuthState.REVOKED
        else:
            self.auth_state = AuthState.UNAUTHENTICATED

    def attach_credential(self, credential_id: str) -> None:
        require_identifier(credential_id, "credential_id")
        self.refresh()
        # Replacement never inherits an old credential's grace window.
        self.attached_credential_id = credential_id
        self._session_credential_id = None
        self._invalidate(TxDecision.DENY_REAUTHENTICATION_REQUIRED)
        self.emit(EventType.CREDENTIAL_ATTACHED)
        credential = self.credential
        if not self.device.system_healthy:
            reason = TxDecision.DENY_SYSTEM_FAILURE
        elif self.device.revoked:
            reason = TxDecision.DENY_DEVICE_REVOKED
        elif credential is None:
            reason = TxDecision.DENY_UNKNOWN_CREDENTIAL
        elif credential.revoked:
            reason = TxDecision.DENY_CREDENTIAL_REVOKED
        elif not credential.authorized:
            reason = TxDecision.DENY_CREDENTIAL_UNAUTHORIZED
        else:
            self._session_credential_id = credential_id
            self.session_valid = True
            self.auth_state = AuthState.AUTHORIZED
            self.emit(EventType.AUTH_SUCCESS)
            self._reconcile_tx()
            return
        self._invalidate(reason)
        self.emit(EventType.AUTH_FAILED, reason.value)
        self._reconcile_tx()

    def detach_credential(self) -> None:
        self.refresh()
        if self.attached_credential_id is None:
            return  # Repeated detach cannot extend an existing deadline.
        self.emit(EventType.CREDENTIAL_LOST)
        self.attached_credential_id = None
        if self.auth_state is AuthState.AUTHORIZED and self.session_valid:
            self.auth_state = AuthState.GRACE_PERIOD
            self.grace_deadline_ms = self.clock.now_ms + self.configuration.grace_period_ms
            self.emit(EventType.GRACE_STARTED, milliseconds=self.configuration.grace_period_ms)
        else:
            self._session_credential_id = None
            self._invalidate(TxDecision.DENY_NO_CREDENTIAL)
        self.refresh()

    def ptt_press(self) -> None:
        self.refresh()
        if self.ptt_state is PttState.PRESSED:
            return
        self.ptt_state = PttState.PRESSED
        self.emit(EventType.PTT_PRESSED)
        self._reconcile_tx(force_denial=True)

    def ptt_release(self) -> None:
        self.refresh()
        if self.ptt_state is PttState.RELEASED:
            return
        was_allowed = self._decision is TxDecision.ALLOW
        self.ptt_state = PttState.RELEASED
        self.emit(EventType.PTT_RELEASED)
        self._reconcile_tx()
        if not was_allowed:
            self.emit(EventType.TX_DISABLED, self._decision.value)

    def advance_time(self, milliseconds: int) -> None:
        self.clock.advance(milliseconds)
        self.emit(EventType.TIME_ADVANCED, milliseconds=milliseconds)
        self.refresh()

    def revoke_device(self) -> None:
        self.refresh()
        if self.device.revoked:
            return
        self.device = replace(self.device, revoked=True)
        self.emit(EventType.DEVICE_REVOKED)
        self._invalidate(TxDecision.DENY_DEVICE_REVOKED)
        self.emit(EventType.AUTHORIZATION_INVALIDATED, self.denial_reason.value)
        self._reconcile_tx(force_disabled=True)

    def restore_device(self) -> None:
        self.refresh()
        if not self.device.revoked:
            return
        self.device = replace(self.device, revoked=False)
        self._invalidate(TxDecision.DENY_REAUTHENTICATION_REQUIRED)
        self.emit(EventType.DEVICE_RESTORED, self.denial_reason.value)
        self._reconcile_tx()

    def revoke_credential(self, credential_id: str) -> None:
        require_identifier(credential_id, "credential_id")
        if credential_id not in self._credentials:
            raise ValueError(f"cannot revoke unregistered credential: {credential_id}")
        self.refresh()
        credential = self._credentials[credential_id]
        if credential.revoked:
            return
        self._credentials[credential_id] = replace(credential, revoked=True)
        self.emit(EventType.CREDENTIAL_REVOKED, credential_id=credential_id)
        if credential_id in (self.attached_credential_id, self._session_credential_id):
            self._invalidate(TxDecision.DENY_CREDENTIAL_REVOKED)
            self.emit(EventType.AUTHORIZATION_INVALIDATED, self.denial_reason.value)
        self._reconcile_tx()

    def system_fault(self) -> None:
        self.refresh()
        if not self.device.system_healthy:
            return
        self.device = replace(self.device, system_healthy=False)
        self._invalidate(TxDecision.DENY_SYSTEM_FAILURE)
        self.emit(EventType.SYSTEM_FAULT, self.denial_reason.value)
        self._reconcile_tx(force_disabled=True)

    def system_recover(self) -> None:
        self.refresh()
        if self.device.system_healthy:
            return
        self.device = replace(self.device, system_healthy=True)
        self._invalidate(TxDecision.DENY_REAUTHENTICATION_REQUIRED)
        self.emit(EventType.SYSTEM_RECOVERED, self.denial_reason.value)
        self._reconcile_tx()

    def system_reboot(self) -> None:
        self.refresh()
        self.device = replace(self.device, system_healthy=True)
        self.attached_credential_id = None
        self._session_credential_id = None
        self.ptt_state = PttState.RELEASED
        self._invalidate(TxDecision.DENY_NO_CREDENTIAL)
        self.emit(EventType.SYSTEM_REBOOT)
        self._reconcile_tx(force_disabled=True)
        # Revocation policy and audit history survive a simulated reboot.
