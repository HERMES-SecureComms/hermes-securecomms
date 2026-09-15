"""Pure fail-closed policy; no state changes, clocks, logging, or hardware I/O."""

from .models import AuthState, PolicyContext, PttState, TxDecision


def evaluate(context: PolicyContext) -> TxDecision:
    if context.device.system_healthy is not True:
        return TxDecision.DENY_SYSTEM_FAILURE
    if context.device.revoked is not False:
        return TxDecision.DENY_DEVICE_REVOKED
    if (
        not isinstance(context.auth_state, AuthState)
        or not isinstance(context.ptt_state, PttState)
        or type(context.now_ms) is not int
        or context.now_ms < 0
    ):
        return TxDecision.DENY_INVALID_STATE
    if context.auth_state is AuthState.SYSTEM_FAILURE:
        return TxDecision.DENY_SYSTEM_FAILURE
    if context.credential is not None:
        if context.credential.revoked is not False:
            return TxDecision.DENY_CREDENTIAL_REVOKED
        if context.credential.authorized is not True:
            return TxDecision.DENY_CREDENTIAL_UNAUTHORIZED
    if context.auth_state not in (AuthState.AUTHORIZED, AuthState.GRACE_PERIOD):
        if context.auth_state not in (AuthState.UNAUTHENTICATED, AuthState.REVOKED):
            return TxDecision.DENY_INVALID_STATE
        if isinstance(context.denial_reason, TxDecision) and context.denial_reason not in (
            TxDecision.ALLOW, TxDecision.DENY_PTT_RELEASED
        ):
            return context.denial_reason
        return TxDecision.DENY_INVALID_STATE
    if context.credential is None:
        return TxDecision.DENY_NO_CREDENTIAL
    if context.session_valid is not True:
        return TxDecision.DENY_REAUTHENTICATION_REQUIRED
    if context.auth_state is AuthState.GRACE_PERIOD:
        if (
            context.attached_credential_id is not None
            or type(context.grace_deadline_ms) is not int
            or context.grace_deadline_ms < 0
        ):
            return TxDecision.DENY_INVALID_STATE
        if context.now_ms >= context.grace_deadline_ms:
            return TxDecision.DENY_CREDENTIAL_EXPIRED
    elif (
        context.attached_credential_id != context.credential.credential_id
        or context.grace_deadline_ms is not None
    ):
        return TxDecision.DENY_INVALID_STATE
    if context.ptt_state is PttState.RELEASED:
        return TxDecision.DENY_PTT_RELEASED
    if context.ptt_state is not PttState.PRESSED:
        return TxDecision.DENY_INVALID_STATE
    return TxDecision.ALLOW
