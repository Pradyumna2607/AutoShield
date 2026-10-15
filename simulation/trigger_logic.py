"""Python reference model of the AutoShield three-stage trigger logic.

Mirrors ``firmware/autoshield/trigger_logic.h`` (paper Section 4.2). One call to
:meth:`TriggerLogic.step` is one 60-second evaluation cycle.
"""
from __future__ import annotations

from collections import deque
from dataclasses import dataclass
from enum import Enum


@dataclass
class Config:
    # Stage 1 (Alert) - values from the paper
    pressure_drop_hpa: float = 2.0
    pressure_window_cycles: int = 30
    humidity_pct: float = 80.0
    ldr_cloud_threshold: float = 0.40
    use_ldr: bool = True
    # Stage 2 (Deploy) - values from the paper
    temp_drop_c: float = 1.5
    temp_window_cycles: int = 15
    predeploy_cycles: int = 2
    # Assumptions (not specified in the paper)
    stage1_persist_cycles: int = 3
    cancel_lockout_cycles: int = 30


@dataclass
class Reading:
    pressure_hpa: float
    temp_c: float
    humidity_pct: float
    light_norm: float  # 0 = dark, 1 = bright
    rain_detected: bool = False


class State(Enum):
    IDLE = "idle"
    ALERT = "alert"
    PRE_DEPLOY = "pre_deploy"
    DEPLOYED = "deployed"


class Event(Enum):
    NONE = "none"
    ALERT_RAISED = "alert_raised"
    ALERT_CLEARED = "alert_cleared"
    PRE_DEPLOY_NOTIFY = "pre_deploy_notify"
    DEPLOY = "deploy"


class TriggerLogic:
    def __init__(self, cfg: Config | None = None):
        self.cfg = cfg or Config()
        self.reset()

    def reset(self) -> None:
        self.state = State.IDLE
        self._p = deque(maxlen=self.cfg.pressure_window_cycles + 1)
        self._t = deque(maxlen=self.cfg.temp_window_cycles + 1)
        self._persist = 0
        self._countdown = 0
        self._lockout = 0

    def step(self, r: Reading) -> Event:
        c = self.cfg
        self._p.append(r.pressure_hpa)
        self._t.append(r.temp_c)

        if self.state is State.DEPLOYED:  # Stage 3: hold until manual retract
            return Event.NONE
        if self.state is State.PRE_DEPLOY:
            self._countdown = max(0, self._countdown - 1)
            if self._countdown == 0:
                self.state = State.DEPLOYED
                return Event.DEPLOY
            return Event.NONE
        if self._lockout > 0:
            self._lockout -= 1
            return Event.NONE

        warm = (len(self._p) == self._p.maxlen) and (len(self._t) == self._t.maxlen)
        stage1 = (
            warm
            and (self._p[0] - r.pressure_hpa) > c.pressure_drop_hpa
            and r.humidity_pct > c.humidity_pct
            and (not c.use_ldr or r.light_norm < c.ldr_cloud_threshold)
        )
        temp_drop = warm and (self._t[0] - r.temp_c) > c.temp_drop_c

        if self.state is State.IDLE:
            if r.rain_detected:
                return self._begin_deploy()
            if stage1:
                self.state, self._persist = State.ALERT, 1
                return Event.ALERT_RAISED
            return Event.NONE

        # State.ALERT
        self._persist = self._persist + 1 if stage1 else 0
        # Interpretation: (Stage 1 persists AND temperature drop) OR rain detected.
        if (self._persist >= c.stage1_persist_cycles and temp_drop) or r.rain_detected:
            return self._begin_deploy()
        if not stage1:
            self.state = State.IDLE
            return Event.ALERT_CLEARED
        return Event.NONE

    def cancel(self) -> bool:
        """Farmer cancels during the pre-deployment countdown."""
        if self.state is not State.PRE_DEPLOY:
            return False
        self.state, self._persist, self._countdown = State.IDLE, 0, 0
        self._lockout = self.cfg.cancel_lockout_cycles
        return True

    def retracted(self) -> None:
        """Call after a manual retract to leave the Stage 3 hold."""
        if self.state is State.DEPLOYED:
            self.state, self._persist = State.IDLE, 0

    def _begin_deploy(self) -> Event:
        self._persist = 0
        if self.cfg.predeploy_cycles == 0:
            self.state = State.DEPLOYED
            return Event.DEPLOY
        self.state, self._countdown = State.PRE_DEPLOY, self.cfg.predeploy_cycles
        return Event.PRE_DEPLOY_NOTIFY
