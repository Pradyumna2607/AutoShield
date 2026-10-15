import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from trigger_logic import Config, Event, Reading, State, TriggerLogic  # noqa: E402


def calm():
    return Reading(1010.0, 28.0, 55.0, 0.9)


def storm(m):
    return Reading(1010.0 - 0.12 * m, 28.0 - 0.15 * m, 90.0, 0.2)


def test_calm_never_triggers():
    t = TriggerLogic()
    assert all(t.step(calm()) is Event.NONE for _ in range(500))
    assert t.state is State.IDLE


def test_storm_sequence():
    t = TriggerLogic()
    first = {}
    for m in range(80):
        e = t.step(storm(m))
        if e is not Event.NONE:
            first.setdefault(e, m)
    assert first[Event.ALERT_RAISED] >= 30
    assert first[Event.PRE_DEPLOY_NOTIFY] > first[Event.ALERT_RAISED]
    assert first[Event.DEPLOY] == first[Event.PRE_DEPLOY_NOTIFY] + 2
    assert t.state is State.DEPLOYED


def test_no_alert_without_humidity():
    t = TriggerLogic()
    for m in range(80):
        r = storm(m)
        r.humidity_pct = 60
        assert t.step(r) is Event.NONE


def test_rain_sensor_triggers_during_warmup():
    t = TriggerLogic()
    r = calm()
    r.rain_detected = True
    assert t.step(r) is Event.PRE_DEPLOY_NOTIFY


def test_cancel_and_lockout():
    t = TriggerLogic()
    r = calm()
    r.rain_detected = True
    assert t.step(r) is Event.PRE_DEPLOY_NOTIFY
    assert t.cancel()
    assert all(t.step(r) is Event.NONE for _ in range(30))
    assert t.step(r) is Event.PRE_DEPLOY_NOTIFY


def test_hold_until_retracted():
    t = TriggerLogic(Config(predeploy_cycles=0))
    r = calm()
    r.rain_detected = True
    assert t.step(r) is Event.DEPLOY
    assert all(t.step(calm()) is Event.NONE for _ in range(100))
    assert t.state is State.DEPLOYED
    t.retracted()
    assert t.state is State.IDLE


def test_ldr_can_be_disabled():
    t = TriggerLogic(Config(use_ldr=False))
    alert = False
    for m in range(50):
        r = storm(m)
        r.light_norm = 1.0
        alert |= t.step(r) is Event.ALERT_RAISED
    assert alert
