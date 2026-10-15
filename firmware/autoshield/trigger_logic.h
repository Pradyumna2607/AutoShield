// AutoShield three-stage rainfall trigger logic (hardware independent).
//
// Implements Section 4.2 of the AutoShield paper. This header has no Arduino
// dependencies so it can be unit-tested on a PC (see firmware/test/).
//
// One call to TriggerLogic::step() corresponds to one 60-second evaluation
// cycle, so every time window below is expressed in cycles (1 cycle = 60 s).
#pragma once
#include <stdint.h>

namespace autoshield {

struct Config {
  // Stage 1 (Alert) -- values from the paper
  float    pressure_drop_hpa      = 2.0f;   // drop over the window below
  uint8_t  pressure_window_cycles = 30;     // 30 minutes
  float    humidity_pct           = 80.0f;  // RH must exceed this
  float    ldr_cloud_threshold    = 0.40f;  // normalised light (0=dark,1=bright) below this = cloudy
  bool     use_ldr                = true;
  // Stage 2 (Deploy) -- values from the paper
  float    temp_drop_c            = 1.5f;   // drop over the window below
  uint8_t  temp_window_cycles     = 15;     // 15 minutes
  uint16_t predeploy_cycles       = 2;      // 2-minute farmer notification; 0 = deploy immediately
  // ASSUMPTIONS (not specified in the paper -- tune with field data)
  uint8_t  stage1_persist_cycles  = 3;      // consecutive Stage-1 cycles that count as "persisting"
  uint16_t cancel_lockout_cycles  = 30;     // suppress re-trigger after a farmer cancels
};

struct Reading {
  float pressure_hpa;
  float temp_c;
  float humidity_pct;
  float light_norm;     // 0.0 (dark) .. 1.0 (bright)
  bool  rain_detected;  // rain sensor (FC-37) fired
};

enum class State : uint8_t { Idle, Alert, PreDeploy, Deployed };
enum class Event : uint8_t { None, AlertRaised, AlertCleared, PreDeployNotify, Deploy };

class TriggerLogic {
 public:
  static const uint8_t kMaxPressureBuf = 61;  // supports windows up to 60 cycles
  static const uint8_t kMaxTempBuf     = 61;

  explicit TriggerLogic(const Config& cfg = Config()) : cfg_(cfg) { reset(); }

  void reset() {
    state_ = State::Idle;
    p_n_ = t_n_ = p_i_ = t_i_ = 0;
    persist_ = 0; countdown_ = 0; lockout_ = 0;
  }

  State state() const { return state_; }

  // Call once every 60 s with the latest sensor reading.
  Event step(const Reading& r) {
    push(p_buf_, p_i_, p_n_, psize(), r.pressure_hpa);
    push(t_buf_, t_i_, t_n_, tsize(), r.temp_c);

    if (state_ == State::Deployed) return Event::None;  // Stage 3: hold until manual retract
    if (state_ == State::PreDeploy) {
      if (countdown_ > 0) --countdown_;
      if (countdown_ == 0) { state_ = State::Deployed; return Event::Deploy; }
      return Event::None;
    }
    if (lockout_ > 0) { --lockout_; return Event::None; }

    const bool warm   = (p_n_ >= psize()) && (t_n_ >= tsize());
    const float p_old = p_buf_[p_i_];  // oldest sample (window cycles ago)
    const float t_old = t_buf_[t_i_];
    const bool stage1 = warm &&
        (p_old - r.pressure_hpa) > cfg_.pressure_drop_hpa &&
        r.humidity_pct > cfg_.humidity_pct &&
        (!cfg_.use_ldr || r.light_norm < cfg_.ldr_cloud_threshold);
    const bool temp_drop = warm && (t_old - r.temp_c) > cfg_.temp_drop_c;

    if (state_ == State::Idle) {
      if (r.rain_detected) return begin_deploy();
      if (stage1) { state_ = State::Alert; persist_ = 1; return Event::AlertRaised; }
      return Event::None;
    }
    // State::Alert
    if (stage1) { if (persist_ < 255) ++persist_; } else { persist_ = 0; }
    // Interpretation: (Stage 1 persists AND temperature drop) OR rain detected.
    if ((persist_ >= cfg_.stage1_persist_cycles && temp_drop) || r.rain_detected)
      return begin_deploy();
    if (!stage1) { state_ = State::Idle; return Event::AlertCleared; }
    return Event::None;
  }

  // Farmer cancels during the pre-deployment countdown. Returns true if cancelled.
  bool cancel() {
    if (state_ != State::PreDeploy) return false;
    state_ = State::Idle; persist_ = 0; countdown_ = 0;
    lockout_ = cfg_.cancel_lockout_cycles;
    return true;
  }

  // Call after the canopy has been manually retracted (leaves Stage 3 hold).
  void retracted() { if (state_ == State::Deployed) { state_ = State::Idle; persist_ = 0; } }

 private:
  Event begin_deploy() {
    persist_ = 0;
    if (cfg_.predeploy_cycles == 0) { state_ = State::Deployed; return Event::Deploy; }
    state_ = State::PreDeploy; countdown_ = cfg_.predeploy_cycles;
    return Event::PreDeployNotify;
  }
  uint8_t psize() const { return cfg_.pressure_window_cycles + 1; }
  uint8_t tsize() const { return cfg_.temp_window_cycles + 1; }
  static void push(float* buf, uint8_t& idx, uint8_t& n, uint8_t size, float v) {
    buf[idx] = v; idx = (idx + 1) % size; if (n < size) ++n;
  }

  Config   cfg_;
  State    state_;
  float    p_buf_[kMaxPressureBuf];
  float    t_buf_[kMaxTempBuf];
  uint8_t  p_n_, t_n_, p_i_, t_i_;
  uint8_t  persist_;
  uint16_t countdown_, lockout_;
};

}  // namespace autoshield
