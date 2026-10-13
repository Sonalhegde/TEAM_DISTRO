/* Lam Research Challenge 3.0 – Stage 2: plant + controller model (no DOM, runs in browser and Node) */
(function (root) {
  'use strict';

  // ---------- constants (from the problem statement + assumptions marked "assumed") ----------
  const C = {
    D: 0.008, rho: 1000, mu: 1e-3,           // given: 8 mm pipe, water
    qMin: 1, qMax: 6, target: 3,             // given: 1-6 L/min, 3 L/min nominal
    tankCap: 10, sumpCap: 4.5, flushVol: 4.2, // given: 10 L tank, ~4.5 L sump, 4.2 L flush tank
    sumpFloat: 3.0, floatHyst: 0.15,         // assumed: float-switch trips at 3.0 L, resets 0.15 L lower
    pumpMax: 6.0,                            // assumed: each pump delivers 6 L/min at 100 % duty, free discharge
    kTrue: 7.30, f0: 0.45,                   // assumed: true FT-01 response  f = 7.30*Q - 0.45  (Hz, L/min)
    kDatasheet: 7.5,                         // datasheet-style coefficient used before calibration
    valveK: 2.0,                             // assumed: ball-valve throttle curve  1-exp(-k x)
    bv2Default: 0.283,                       // ~3 L/min at 100 % Pump-02
    initTank: 5.0, initSump: 2.0
  };
  C.A = Math.PI * C.D * C.D / 4;

  const clamp = (x, a, b) => Math.max(a, Math.min(b, x));
  function rng(seed) { let a = seed >>> 0; return function () { a |= 0; a = a + 0x6D2B79F5 | 0; let t = Math.imul(a ^ a >>> 15, 1 | a); t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t; return ((t ^ t >>> 14) >>> 0) / 4294967296; }; }
  function gauss(r) { const u = 1 - r(), v = r(); return Math.sqrt(-2 * Math.log(u)) * Math.cos(2 * Math.PI * v); }

  const valveFactor = x => { x = clamp(x, 0, 1); return (1 - Math.exp(-C.valveK * x)) / (1 - Math.exp(-C.valveK)); };
  const valveForFraction = fr => -Math.log(1 - clamp(fr, 0, 1) * (1 - Math.exp(-C.valveK))) / C.valveK;
  const freqOf = q => q < 0.05 ? 0 : Math.max(0, C.kTrue * q - C.f0);

  // ---------- plant ----------
  function Plant(o) {
    o = o || {};
    this.r = rng(o.seed || 7);
    this.bench = !!o.bench;                       // bench = unlimited source/sink (calibration jig)
    this.t = 0;
    this.tank = o.tank != null ? o.tank : C.initTank;
    this.sump = o.sump != null ? o.sump : C.initSump;
    this.flush = o.flush != null ? o.flush : C.flushVol;
    this.cSump = 1; this.cTank = 1;               // process-fluid fraction (1 = process fluid, 0 = flush water)
    this.duty = [0, 0, 0]; this.ov = false;
    this.bv1Open = !!o.bv1Open;                   // manual flush ball valve
    this.bv2 = o.bv2 != null ? o.bv2 : C.bv2Default; // recirculation throttle ball valve (0..1)
    this.bv3Open = true;                          // sump outlet ball valve
    this.fault = 'none';                          // none | valve | blockage | sensor | pump | air
    this.q1 = 0; this.q2 = 0; this.q3 = 0; this.qTank = 0;
    this.pulses = 0; this.pulseAcc = 0; this.freq = 0;
    this.floatOn = this.sump >= C.sumpFloat;
    this.spill = { sump: 0, tank: 0 };
    this.p3Dry = 0; this.p3Dead = 0; this.p2Dry = 0;
    this.volTrue = 0;                             // true volume through FT-01 (L)
    this.airOn = true; this.airT = 0;
    this.injectQueue = 0;
    this.cur = [0, 0, 0];                         // pump currents (A)
  }
  Plant.prototype.inject = function (litres) { this.injectQueue += litres; };
  Plant.prototype.step = function (dt) {
    const s = dt / 60, d = this.duty, r = this.r;
    this.t += dt;
    if (this.injectQueue > 0) { const a = Math.min(this.injectQueue, 1.0 * dt); this.sump += a; this.injectQueue -= a; }

    // Pump-03 (flush) – draws from flush tank through BV-01, discharges into equal tee
    let q3max = 0;
    if (d[2] > 0) {
      if (!this.bv1Open) this.p3Dead += dt;
      else if (this.flush <= 1e-4) this.p3Dry += dt;
      else q3max = Math.min(C.pumpMax * d[2] * (1 + 0.01 * gauss(r)), this.flush / s);
    }
    // Pump-01 – draws from tee (tank via OV-01 and/or Pump-03), pushes to sump
    const q1cmd = C.pumpMax * d[0] * (d[0] > 0 ? 1 + 0.01 * gauss(r) : 1);
    const tankAvail = (this.ov && (this.tank > 1e-3 || this.bench)) ? Infinity : 0;
    let q1 = Math.min(q1cmd, tankAvail + q3max);
    const q3 = Math.min(q3max, q1);
    let qT = q1 - q3;
    if (!this.bench && qT > this.tank / s) { qT = this.tank / s; q1 = q3 + qT; }

    // Pump-02 – sump outlet -> BV-03 -> Pump-02 -> BV-02 -> FT-01 -> tank
    let f = this.bv3Open ? valveFactor(this.bv2) : 0;
    if (this.fault === 'valve') f *= 0.4;
    else if (this.fault === 'blockage') f = 0;
    else if (this.fault === 'pump') f = 0;
    else if (this.fault === 'air') {
      this.airT -= dt;
      if (this.airT <= 0) { this.airOn = !this.airOn; this.airT = 0.5 + 1.5 * r(); }
      f *= this.airOn ? 0.85 : 0.03;
    }
    let q2t = C.pumpMax * d[1] * f * (d[1] > 0 ? 1 + 0.012 * gauss(r) : 1);
    const avail = this.bench ? Infinity : (this.sump + q1 * s) / s;
    if (q2t > avail + 1e-9) { if (d[1] > 0) this.p2Dry += dt; }
    q2t = Math.max(0, Math.min(q2t, avail));
    this.q2 += (q2t - this.q2) * (1 - Math.exp(-dt / 0.5));
    const q2 = Math.min(this.q2, avail);
    this.q2 = q2;

    if (!this.bench) {
      const cin = q1 > 1e-9 ? qT * this.cTank / q1 : this.cSump;
      this.cSump = clamp(this.cSump + q1 * (cin - this.cSump) / Math.max(this.sump, 0.15) * s, 0, 1);
      this.sump += (q1 - q2) * s;
      if (this.sump > C.sumpCap) { this.spill.sump += this.sump - C.sumpCap; this.sump = C.sumpCap; }
      if (this.sump < 0) this.sump = 0;
      this.cTank = clamp(this.cTank + q2 * (this.cSump - this.cTank) / Math.max(this.tank, 0.5) * s, 0, 1);
      this.tank += (q2 - qT) * s;
      if (this.tank > C.tankCap) { this.spill.tank += this.tank - C.tankCap; this.tank = C.tankCap; }
      if (this.tank < 0) this.tank = 0;
      this.flush = Math.max(0, this.flush - q3 * s);
    }
    this.q1 = q1; this.q2 = q2; this.q3 = q3; this.qTank = qT;
    this.volTrue += q2 * s;

    // FT-01 pulses
    const fq = this.fault === 'sensor' ? 0 : freqOf(q2) * (1 + 0.006 * gauss(r));
    this.freq = Math.max(0, fq);
    this.pulseAcc += this.freq * dt;
    const n = Math.floor(this.pulseAcc); this.pulses += n; this.pulseAcc -= n;

    // float switch (normally-open, closes when sump level >= trip level; hysteresis)
    if (!this.floatOn && this.sump >= C.sumpFloat) this.floatOn = true;
    else if (this.floatOn && this.sump <= C.sumpFloat - C.floatHyst) this.floatOn = false;

    // pump currents (A) – used only as auxiliary diagnostic signal
    this.cur[0] = q1cmd > 0 ? 0.35 + 0.6 * d[0] : 0;
    this.cur[2] = d[2] > 0 ? (this.bv1Open ? (this.flush > 1e-4 ? 0.35 + 0.6 * d[2] : 0.2) : 1.1) : 0;
    let i2 = d[1] > 0 ? 0.35 + 0.6 * d[1] * (0.6 + 0.4 * f) : 0;
    if (d[1] > 0) {
      if (this.fault === 'blockage') i2 *= 1.35; else if (this.fault === 'pump') i2 = 0.02;
      else if (this.fault === 'air') i2 *= this.airOn ? 0.9 : 0.55; else if (q2t <= 1e-6 && avail < 0.01) i2 *= 0.5;
    }
    this.cur[1] = i2;
  };

  // ---------- controller (models the ESP32 firmware) ----------
  const DEFAULT_CFG = {
    case1Seconds: 60,        // Case-1 run time before automatic hand-over to Case-2
    duty1: 0.5, duty2: 1.0, duty3: 0.5,
    floatDebounce: 0.5, resumeDelay: 3.0,
    valveWait: 10,           // seconds given to the operator to open BV-01 (no sensor on it)
    flushMargin: 0.01,       // stop when measured flush volume >= (1-margin)*4.2 L
    lowFlow: 0.5, lowFlowTime: 4, grace: 6,
    coefA: 1 / C.kDatasheet, coefB: 0
  };
  function Controller(plant, cfg) {
    this.p = plant; this.cfg = Object.assign({}, DEFAULT_CFG, cfg || {});
    this.sw = false; this.state = 'OFF'; this.tState = 0; this.t = 0;
    this.hist = []; this.qRaw = 0; this.qMeas = 0; this.freq = 0;
    this.total = 0; this.totalC1 = 0; this.flushMeas = 0;
    this.hiT = 0; this.loT = 0; this.c1T = 0; this.graceT = 0; this.lowT = 0;
    this.prompt = ''; this.buzzer = false; this.events = []; this.fault = ''; this.done = false;
    this.flushStart = -1; this.flushStop = -1; this.overlap = 0; this.holdCount = 0;
  }
  Controller.prototype.log = function (m) { this.events.push({ t: this.t, m: m }); };
  Controller.prototype.enter = function (s, msg) { this.state = s; this.tState = 0; this.log(msg || s); };
  Controller.prototype.setOut = function (d1, d2, d3, ov) { const p = this.p; p.duty[0] = d1; p.duty[1] = d2; p.duty[2] = d3; p.ov = ov; };
  Controller.prototype.tick = function (dt) {
    const p = this.p, cf = this.cfg;
    this.t += dt; this.tState += dt;
    // --- FT-01 conversion: 1 s sliding window of pulses -> Hz -> Q = a f + b
    this.hist.push([this.t, p.pulses]);
    while (this.hist.length > 2 && this.t - this.hist[0][0] > 1.0) this.hist.shift();
    const h0 = this.hist[0], span = this.t - h0[0];
    this.freq = span >= 0.5 ? (p.pulses - h0[1]) / span : this.freq;
    this.qRaw = this.freq <= 0.01 ? 0 : Math.max(0, cf.coefA * this.freq + cf.coefB);
    this.qMeas += (this.qRaw - this.qMeas) * (1 - Math.exp(-dt / 0.5));
    // totalizer integrates from pulses directly (unfiltered)
    const dq = this.qRaw * dt / 60;
    if (this.state !== 'OFF') this.total += dq;
    // --- float debounce
    if (p.floatOn) { this.hiT += dt; this.loT = 0; } else { this.loT += dt; this.hiT = 0; }
    const floatHigh = this.hiT >= cf.floatDebounce, floatClear = this.loT >= cf.resumeDelay;

    this.buzzer = false;
    if (!this.sw) {                                // ON/OFF switch OFF -> everything stops, latch cleared
      if (this.state !== 'OFF') { this.enter('OFF', 'Switch OFF – all outputs disabled'); }
      this.setOut(0, 0, 0, false); this.prompt = ''; this.done = false; this.fault = '';
      return;
    }
    switch (this.state) {
      case 'OFF':
        this.total = 0; this.totalC1 = 0; this.flushMeas = 0; this.c1T = 0; this.graceT = 0; this.lowT = 0;
        this.holdCount = 0; this.done = false; this.fault = ''; this.flushStart = this.flushStop = -1; this.overlap = 0;
        this.enter('C1_RUN', 'Switch ON – CASE 1 start (OV-01 open, P1 50 %, P2 100 %)');
        break;
      case 'C1_RUN':
        this.setOut(cf.duty1, cf.duty2, 0, true);
        this.c1T += dt; this.graceT += dt; this.totalC1 += dq;
        if (floatHigh) { this.holdCount++; this.enter('C1_HOLD', 'FLOAT HIGH – Pump-01 stopped, Pump-02 keeps draining sump'); }
        else if (this.c1T >= cf.case1Seconds) { this.setOut(0, 0, 0, false); this.enter('C2_WAIT_BV', 'CASE 1 complete – stopping pumps, closing OV-01'); }
        break;
      case 'C1_HOLD':
        this.setOut(0, cf.duty2, 0, true);
        this.c1T += dt; this.graceT += dt; this.totalC1 += dq;
        if (floatClear) this.enter('C1_RUN', 'Sump level normal – Pump-01 restarted');
        break;
      case 'C2_WAIT_BV': {
        this.setOut(0, 0, 0, false);               // OV-01 OFF isolates the 10 L tank
        const left = Math.max(0, cf.valveWait - this.tState);
        this.prompt = 'OPEN MANUAL BALL VALVE BV-01 NOW – Pump-03 starts in ' + left.toFixed(0) + ' s';
        this.buzzer = true;
        if (this.tState >= cf.valveWait) { this.flushStart = this.t; this.graceT = 0; this.lowT = 0; this.enter('C2_FLUSH', 'CASE 2 – Pump-03 started; P1+P2 started; flush in progress'); }
        break;
      }
      case 'C2_FLUSH':
        this.prompt = '';
        // Pump-03 first (prime the tee), then P1 & P2 half a second later
        this.setOut(this.tState >= 0.5 ? cf.duty1 : 0, this.tState >= 0.5 ? cf.duty2 : 0, cf.duty3, false);
        this.graceT += dt;
        if (this.tState >= 0.5) { this.flushMeas += dq; this.overlap += dt; }
        if (this.flushMeas >= C.flushVol * (1 - cf.flushMargin)) {
          this.flushStop = this.t; this.done = true; this.enter('DONE', 'Flush volume reached (' + this.flushMeas.toFixed(2) + ' L) – all pumps OFF');
        }
        break;
      case 'DONE':
        this.setOut(0, 0, 0, false); this.prompt = 'SEQUENCE COMPLETE – switch OFF to reset';
        break;
      case 'FAULT':
        this.setOut(0, 0, 0, false); this.prompt = 'FAULT: ' + this.fault + ' – switch OFF to reset';
        this.buzzer = true;
        break;
    }
    // low-flow / dry-run trip
    if (this.state === 'C1_RUN' || this.state === 'C1_HOLD' || (this.state === 'C2_FLUSH' && this.tState >= 0.5)) {
      if (this.graceT > cf.grace) {
        if (this.qMeas < cf.lowFlow) this.lowT += dt; else this.lowT = 0;
        if (this.lowT >= cf.lowFlowTime) {
          this.fault = 'LOW FLOW / DRY RUN (' + (this.state === 'C2_FLUSH' ? 'no flush flow – is BV-01 open?' : 'no flow') + ')';
          this.setOut(0, 0, 0, false); this.enter('FAULT', 'FAULT – ' + this.fault);
        }
      }
    }
  };

  // ---------- system = controller + plant, fixed step ----------
  function System(o) {
    o = o || {};
    this.plant = new Plant(o.plant); this.ctrl = new Controller(this.plant, o.cfg);
    this.t = 0; this.dt = 0.05; this.autoOperator = o.autoOperator !== false; this.opDelay = 3;
    this.rec = [];  this.recEvery = 0.5; this._rt = 0;
  }
  System.prototype.step = function () {
    const c = this.ctrl, p = this.plant;
    if (this.autoOperator && c.state === 'C2_WAIT_BV' && c.tState >= this.opDelay) p.bv1Open = true;
    c.tick(this.dt); p.step(this.dt); this.t += this.dt;
    this._rt += this.dt;
    if (this._rt >= this.recEvery - 1e-9) {
      this._rt = 0;
      this.rec.push({ t: this.t, q: c.qMeas, qTrue: p.q2, total: c.total, sump: p.sump, tank: p.tank, flush: p.flush, p1: p.q1, p3: p.q3, state: c.state });
      if (this.rec.length > 20000) this.rec.shift();
    }
  };
  System.prototype.run = function (secs, until) {
    const n = Math.round(secs / this.dt);
    for (let i = 0; i < n; i++) { this.step(); if (until && until(this)) return true; }
    return false;
  };

  // ---------- bench experiments (Task 1.2, 2.1, 2.2, 2.3) ----------
  function benchTrial(o) {
    const p = new Plant({ bench: true, seed: o.seed || 11, bv2: o.valve });
    p.duty[1] = o.duty != null ? o.duty : 1; p.ov = false;
    const dt = 0.05; let t = 0;
    const settle = 6; for (; t < settle; t += dt) p.step(dt);
    const p0 = p.pulses, v0 = p.volTrue, T = o.seconds || 30;
    for (t = 0; t < T - 1e-9; t += dt) p.step(dt);
    const pulses = p.pulses - p0, volTrue = p.volTrue - v0;
    const r = rng((o.seed || 11) * 31 + 5);
    const volMeas = volTrue + 0.012 * gauss(r);      // measuring-jug reading error ~ ±12 mL (1σ)
    return { valve: o.valve, pulses, time: T, freq: pulses / T, volTrue, volume: volMeas, Q: volMeas / T * 60, Qtrue: volTrue / T * 60 };
  }
  function fitLinear(fs, Qs) {
    const n = fs.length, mf = fs.reduce((a, b) => a + b, 0) / n, mq = Qs.reduce((a, b) => a + b, 0) / n;
    let sxy = 0, sxx = 0, syy = 0;
    for (let i = 0; i < n; i++) { sxy += (fs[i] - mf) * (Qs[i] - mq); sxx += (fs[i] - mf) ** 2; syy += (Qs[i] - mq) ** 2; }
    const a = sxy / sxx, b = mq - a * mf;
    let ssr = 0; const errs = [];
    for (let i = 0; i < n; i++) { const qf = a * fs[i] + b; ssr += (Qs[i] - qf) ** 2; errs.push(Math.abs(qf - Qs[i]) / Qs[i] * 100); }
    return { a, b, r2: 1 - ssr / syy, errs, maxErr: Math.max.apply(null, errs), avgErr: errs.reduce((x, y) => x + y, 0) / n };
  }
  function calibrate(valves, seconds) {
    valves = valves || [0.08, 0.15, 0.283, 0.5, 1.0];
    const trials = valves.map((v, i) => benchTrial({ valve: v, seconds: seconds || 30, seed: 20 + i }));
    const fit = fitLinear(trials.map(t => t.freq), trials.map(t => t.Q));
    return { trials, fit };
  }
  // error of a coefficient pair against a set of bench trials
  function coefError(coef, trials) {
    const e = trials.map(t => Math.abs((coef.a * t.freq + coef.b) - t.Q) / t.Q * 100);
    return { max: Math.max.apply(null, e), avg: e.reduce((x, y) => x + y, 0) / e.length };
  }
  function valveSweep(positions, coef) {
    coef = coef || { a: 1 / C.kDatasheet, b: 0 };
    return positions.map((x, i) => {
      const t = benchTrial({ valve: x, seconds: 20, seed: 50 + i });
      return { valve: x, Q: coef.a * t.freq + coef.b, Qtrue: t.Qtrue, freq: t.freq };
    });
  }
  // operate at a set-point flow using the ball valve (pump 100 %) – returns measured values
  function flowPoint(Qset, coef) {
    const x = valveForFraction(Qset / C.pumpMax);
    const t = benchTrial({ valve: x, seconds: 30, seed: 90 + Math.round(Qset * 10) });
    const Qm = coef.a * t.freq + coef.b;
    return { Qset, valve: x, freq: t.freq, Qmeas: Qm, Qtrue: t.Qtrue, Qjug: t.Q };
  }

  // ---------- theory helpers ----------
  const velocity = Q => (Q / 1000 / 60) / C.A;                    // Q in L/min -> m/s
  const reynolds = Q => C.rho * velocity(Q) * C.D / C.mu;
  function dpPerMetre(Q) {                                        // Pa per metre of 8 mm pipe (smooth)
    const Re = reynolds(Q), fD = Re < 2300 ? 64 / Re : 0.316 / Math.pow(Re, 0.25);
    return fD / C.D * C.rho * velocity(Q) ** 2 / 2;
  }
  const regime = Re => Re < 2300 ? 'laminar' : Re < 4000 ? 'transitional' : 'turbulent';
  const residence = (Vlitres, Qlpm) => Vlitres / Qlpm * 60;       // seconds

  // ---------- fault signatures for Q3.2 ----------
  function faultSignature(fault, pumpOn) {
    const p = new Plant({ bench: true, seed: 3, bv2: fault === 'valve' ? 0.283 : C.bv2Default });
    p.duty[1] = pumpOn === false ? 0 : 1; p.fault = fault;
    const dt = 0.05, win = [], nomF = freqOf(3);
    let wAcc = 0, wT = 0, cur = 0, cn = 0, qsum = 0, qn = 0;
    for (let t = 0; t < 40; t += dt) {
      p.step(dt);
      if (t > 5) {
        wAcc += p.freq * dt; wT += dt; cur += p.cur[1]; cn++; qsum += p.q2; qn++;
        if (wT >= 1 - 1e-9) { win.push(wAcc / wT); wAcc = 0; wT = 0; }
      }
    }
    const mean = win.reduce((a, b) => a + b, 0) / win.length;
    const sd = Math.sqrt(win.reduce((a, b) => a + (b - mean) ** 2, 0) / win.length);
    const ratio = mean / nomF, cv = mean > 0 ? sd / mean : 0;
    // "true" flow (what a jug would collect) vs. what the sensor reports
    const trueQ = qsum / qn;
    let pulse = 'Normal';
    if (mean < 0.5) pulse = 'Zero'; else if (cv > 0.35) pulse = 'Intermittent'; else if (ratio < 0.85) pulse = 'Reduced';
    return { pulse, ratio, cv, pumpCurrent: cur / cn, trueFlow: trueQ };
  }

  const FAULTS = [
    { id: 'A', pump: 'ON', valve: 'Open', pulse: 'Normal', flow: 'Normal', sim: ['none', true],
      cause: 'Healthy operation', detail: 'Pump delivering, valve open, FT-01 pulse rate consistent with the commanded flow. No action.' },
    { id: 'B', pump: 'ON', valve: 'Partially closed', pulse: 'Reduced', flow: 'Reduced', sim: ['valve', true],
      cause: 'Valve restriction (partially closed ball valve, or partial line restriction)', detail: 'Pulse rate falls in proportion to real flow, so sensor and hydraulics agree. Pump current normal to slightly high; sump level slowly rises (P1 still feeding). Fix: re-open / re-trim BV-02.' },
    { id: 'C', pump: 'ON', valve: 'Open', pulse: 'Zero', flow: 'Zero', sim: ['sensor', true],
      cause: 'Ambiguous: FT-01 signal/sensor failure OR hydraulic blockage OR pump failure/lost prime', detail: 'Zero pulses only says "the sensor sees no rotation". A jammed/disconnected sensor with real flow, a fully blocked line and a dead pump all look identical on FT-01.' },
    { id: 'D', pump: 'OFF', valve: 'Open', pulse: 'Zero', flow: 'Zero', sim: ['none', false],
      cause: 'Pump not running (commanded OFF, driver/PWM-enable fault, lost 12 V, switch OFF)', detail: 'Zero flow is genuine and consistent with the pump state. Check controller state, driver enable, SMPS.' },
    { id: 'E', pump: 'ON', valve: 'Open', pulse: 'Intermittent', flow: 'Fluctuating', sim: ['air', true],
      cause: 'Unstable flow: air entrainment / cavitation / partially lost prime (low sump or leaking suction fitting); or an intermittent wiring/sensor contact', detail: 'Bursts of pulses separated by gaps. Pump current oscillates. Check sump level, suction-side fittings, tubing for bubbles, connector.' }
  ];

  root.Sim = {
    C, DEFAULT_CFG, Plant, Controller, System, valveFactor, valveForFraction, freqOf,
    benchTrial, fitLinear, calibrate, coefError, valveSweep, flowPoint,
    velocity, reynolds, dpPerMetre, regime, residence, faultSignature, FAULTS, gauss, rng, clamp
  };
  if (typeof module !== 'undefined') module.exports = root.Sim;
})(typeof window !== 'undefined' ? window : globalThis);
