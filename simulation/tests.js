/* Automated test cases derived from the Stage-2 question paper. Each returns {id,title,req,pass,detail} */
(function (root) {
  'use strict';
  const S = root.Sim || require('./sim.js');
  const C = S.C;
  const fmt = (x, n) => Number(x).toFixed(n == null ? 2 : n);

  function newSys(cfg, plant) { return new S.System({ cfg, plant }); }
  function startSys(cfg, plant) { const s = newSys(cfg, plant); s.ctrl.sw = true; return s; }

  const cal = () => S.calibrate();

  const TESTS = [
    { id: 'TC-01', req: 'Case 1: closed-loop recirculation reaches 3 L/min (P1 50 %, P2 100 %)', run() {
      const cal_ = cal().fit, s = startSys({ coefA: cal_.a, coefB: cal_.b, case1Seconds: 999 });
      s.run(30); const c = s.ctrl, p = s.plant;
      const q = c.qMeas, ok = Math.abs(q - 3) / 3 <= 0.05 && c.state === 'C1_RUN' && p.duty[0] === 0.5 && p.duty[1] === 1 && p.ov;
      return { pass: ok, detail: `state ${c.state}, OV-01 ${p.ov ? 'open' : 'closed'}, duty P1/P2 = ${p.duty[0] * 100}/${p.duty[1] * 100} %, FT-01 = ${fmt(q)} L/min (target 3 ±5 %)` };
    } },
    { id: 'TC-02', req: 'Case 1: flow stays stable for the whole run, sump level bounded, no spill', run() {
      const cal_ = cal().fit, s = startSys({ coefA: cal_.a, coefB: cal_.b, case1Seconds: 999 });
      s.run(20); let mn = 9, mx = 0, qmn = 99, qmx = 0;
      for (let i = 0; i < 20 * 300; i++) { s.step(); mn = Math.min(mn, s.plant.sump); mx = Math.max(mx, s.plant.sump); qmn = Math.min(qmn, s.ctrl.qMeas); qmx = Math.max(qmx, s.ctrl.qMeas); }
      const ok = mn > 1.0 && mx < C.sumpFloat && s.plant.spill.sump === 0 && qmn > 2.7 && qmx < 3.3 && s.ctrl.state === 'C1_RUN';
      return { pass: ok, detail: `5 min run: sump ${fmt(mn)}–${fmt(mx)} L, flow ${fmt(qmn)}–${fmt(qmx)} L/min, spill ${fmt(s.plant.spill.sump)} L` };
    } },
    { id: 'TC-03', req: 'FT-01 calibration Q = a·f + b : R² ≥ 0.995, max error ≤ 5 %, avg ≤ 2 %', run() {
      const { trials, fit } = cal();
      const ok = fit.r2 >= 0.995 && fit.maxErr <= 5 && fit.avgErr <= 2;
      return { pass: ok, detail: `a = ${fmt(fit.a, 4)} L/min/Hz, b = ${fmt(fit.b, 3)} L/min, R² = ${fmt(fit.r2, 5)}, max err ${fmt(fit.maxErr)} %, avg err ${fmt(fit.avgErr)} % (${trials.length} points)` };
    } },
    { id: 'TC-04', req: 'Calibration improves accuracy versus the un-calibrated datasheet factor', run() {
      const { trials, fit } = cal();
      const before = S.coefError({ a: 1 / C.kDatasheet, b: 0 }, trials), after = S.coefError(fit, trials);
      return { pass: after.avg < before.avg && after.max < before.max, detail: `datasheet K = 7.5 Hz/(L/min): max ${fmt(before.max)} %, avg ${fmt(before.avg)} %  →  calibrated: max ${fmt(after.max)} %, avg ${fmt(after.avg)} %` };
    } },
    { id: 'TC-05', req: 'Case 1: sudden external fluid → Pump-01 stops, Pump-02 keeps running, Pump-01 restarts, process continues', run() {
      const cal_ = cal().fit, s = startSys({ coefA: cal_.a, coefB: cal_.b, case1Seconds: 999 });
      s.run(30); s.plant.inject(1.5);
      let p1Stopped = false, p2Ran = true, tStop = -1, tResume = -1, maxSump = 0, t0 = s.t;
      for (let i = 0; i < 20 * 90; i++) {
        s.step(); maxSump = Math.max(maxSump, s.plant.sump);
        if (s.ctrl.state === 'C1_HOLD') { p1Stopped = true; if (tStop < 0) tStop = s.t - t0; if (s.plant.duty[1] !== 1) p2Ran = false; }
        if (p1Stopped && s.ctrl.state === 'C1_RUN' && tResume < 0) tResume = s.t - t0;
      }
      s.run(20);
      const ok = p1Stopped && p2Ran && tResume > 0 && s.plant.spill.sump === 0 && Math.abs(s.ctrl.qMeas - 3) < 0.3 && s.ctrl.state === 'C1_RUN';
      return { pass: ok, detail: `P1 stopped ${fmt(tStop, 1)} s after injection, restarted at ${fmt(tResume, 1)} s; peak sump ${fmt(maxSump)} L (cap ${C.sumpCap}), spill ${fmt(s.plant.spill.sump)} L, flow after recovery ${fmt(s.ctrl.qMeas)} L/min` };
    } },
    { id: 'TC-06', req: 'Totalizer: total litres returned to 10 L tank matches true volume (±3 %)', run() {
      const cal_ = cal().fit, s = startSys({ coefA: cal_.a, coefB: cal_.b, case1Seconds: 999 });
      s.run(120); const tv = s.plant.volTrue, m = s.ctrl.total, e = (m - tv) / tv * 100;
      return { pass: Math.abs(e) <= 3, detail: `true ${fmt(tv)} L, totalized ${fmt(m)} L, error ${fmt(e)} %` };
    } },
    { id: 'TC-07', req: 'Case 1 → Case 2 hand-over is automatic: OV-01 closes and pumps stop before Pump-03 starts', run() {
      const cal_ = cal().fit, s = startSys({ coefA: cal_.a, coefB: cal_.b, case1Seconds: 40 });
      let seen = false, ok = false;
      for (let i = 0; i < 20 * 80; i++) { s.step(); if (s.ctrl.state === 'C2_WAIT_BV') { seen = true; ok = !s.plant.ov && s.plant.duty.every(d => d === 0); break; } }
      return { pass: seen && ok, detail: `reached C2_WAIT_BV at t = ${fmt(s.t, 1)} s; OV-01 ${s.plant.ov ? 'OPEN' : 'closed'}, duties ${s.plant.duty.join('/')}, prompt "${s.ctrl.prompt}"` };
    } },
    { id: 'TC-08', req: 'Case 2: with BV-01 opened, all three pumps run simultaneously; system stops as soon as the 4.2 L flush tank is empty', run() {
      const cal_ = cal().fit, s = startSys({ coefA: cal_.a, coefB: cal_.b, case1Seconds: 30 });
      let maxOverlap = 0, allThree = 0;
      for (let i = 0; i < 20 * 400 && s.ctrl.state !== 'DONE' && s.ctrl.state !== 'FAULT'; i++) { s.step(); if (s.plant.duty.every(d => d > 0)) allThree += s.dt; }
      s.run(3);
      const p = s.plant, c = s.ctrl;
      const ok = c.state === 'DONE' && allThree > 20 && p.p3Dry <= 2 && p.p3Dead === 0 && p.duty.every(d => d === 0) && p.flush < 0.15;
      return { pass: ok, detail: `end state ${c.state}; all 3 pumps ON for ${fmt(allThree, 1)} s; flush tank residue ${fmt(p.flush * 1000, 0)} mL; Pump-03 dry-run ${fmt(p.p3Dry, 1)} s; measured flush ${fmt(c.flushMeas)} L; all outputs OFF: ${p.duty.every(d => d === 0)}` };
    } },
    { id: 'TC-09', req: 'Case 2: BV-01 not opened (operator error) → controller detects no-flow and trips safely', run() {
      const cal_ = cal().fit, s = new S.System({ cfg: { coefA: cal_.a, coefB: cal_.b, case1Seconds: 20 }, autoOperator: false });
      s.ctrl.sw = true; s.run(140, x => x.ctrl.state === 'FAULT');
      const c = s.ctrl, p = s.plant, ok = c.state === 'FAULT' && p.duty.every(d => d === 0) && p.p3Dead < 60;
      return { pass: ok, detail: `state ${c.state} ("${c.fault}"); Pump-03 dead-headed for ${fmt(p.p3Dead, 1)} s before trip; sump ${fmt(p.sump)} L` };
    } },
    { id: 'TC-10', req: 'ON/OFF switch OFF stops every pump/valve within 0.5 s in any state', run() {
      const cal_ = cal().fit; const res = [];
      [['C1_RUN', 25], ['C1_HOLD', 0], ['C2_FLUSH', 0]].forEach(([st, t]) => {
        const s = startSys({ coefA: cal_.a, coefB: cal_.b, case1Seconds: 20 });
        if (st === 'C1_RUN') s.run(t);
        else if (st === 'C1_HOLD') { s.run(15); s.plant.inject(1.5); s.run(60, x => x.ctrl.state === 'C1_HOLD'); }
        else s.run(200, x => x.ctrl.state === 'C2_FLUSH' && x.ctrl.tState > 5);
        const was = s.ctrl.state; s.ctrl.sw = false; s.step(); s.step();
        res.push(was + '→' + s.ctrl.state + (s.plant.duty.every(d => d === 0) && !s.plant.ov ? ' ✓' : ' ✗'));
      });
      return { pass: res.every(r => r.endsWith('✓')), detail: res.join(', ') };
    } },
    { id: 'TC-11', req: 'Q1.2 – 1, 3 and 6 L/min reachable; theoretical vs measured velocity (8 mm ID)', run() {
      const fit = cal().fit; const rows = [1, 3, 6].map(Q => { const r = S.flowPoint(Q, fit), vt = S.velocity(Q), va = S.velocity(r.Qmeas); return { Q, vt, va, e: (va - vt) / vt * 100, Re: S.reynolds(Q) }; });
      return { pass: rows.every(r => Math.abs(r.e) <= 5), detail: rows.map(r => `${r.Q} L/min: v_th ${fmt(r.vt, 3)} m/s vs v_meas ${fmt(r.va, 3)} m/s (${fmt(r.e, 1)} %), Re ${fmt(r.Re, 0)} ${S.regime(r.Re)}`).join(' | ') };
    } },
    { id: 'TC-12', req: 'Task 2.1 – valve 25/50/75/100 % gives monotonic flow inside the 1–6 L/min range', run() {
      const fit = cal().fit, sw = S.valveSweep([0.25, 0.5, 0.75, 1.0], fit);
      const mono = sw.every((r, i) => i === 0 || r.Q > sw[i - 1].Q), inRange = sw.every(r => r.Q >= 1 && r.Q <= 6.2);
      return { pass: mono && inRange, detail: sw.map(r => `${r.valve * 100}% → ${fmt(r.Q)} L/min`).join(', ') };
    } },
    { id: 'TC-13', req: 'Task 2.3 – five flow conditions logged (pulses, time, f, volume, actual flow)', run() {
      const { trials, fit } = cal();
      return { pass: trials.length >= 5 && trials.every(t => t.pulses > 0 && t.volume > 0), detail: trials.map(t => `${fmt(t.Q)} L/min: N=${t.pulses}, f=${fmt(t.freq)} Hz`).join(' | ') };
    } },
    { id: 'TC-14', req: 'Q3.1 – residence time T = V/Q: 0.5 L @ 2.5 L/min = 12 s; V×2 → 24 s; Q×2 → 6 s', run() {
      const a = S.residence(0.5, 2.5), b = S.residence(1.0, 2.5), c = S.residence(0.5, 5);
      return { pass: Math.abs(a - 12) < 1e-9 && Math.abs(b - 24) < 1e-9 && Math.abs(c - 6) < 1e-9, detail: `T = ${fmt(a, 1)} s; doubled volume ${fmt(b, 1)} s; doubled flow ${fmt(c, 1)} s` };
    } },
    { id: 'TC-15', req: 'Q3.2 – simulated faults reproduce the observation table (A–E)', run() {
      const rows = S.FAULTS.map(f => { const sg = S.faultSignature(f.sim[0], f.sim[1]); return { id: f.id, exp: f.pulse, got: sg.pulse, ok: sg.pulse === f.pulse }; });
      return { pass: rows.every(r => r.ok), detail: rows.map(r => `${r.id}: expected ${r.exp}, simulated ${r.got} ${r.ok ? '✓' : '✗'}`).join(' | ') };
    } },
    { id: 'TC-16', req: 'Un-calibrated FT-01 causes Case-2 to over-run (Pump-03 runs dry) – calibration is required', run() {
      const s = startSys({ case1Seconds: 30 });   // datasheet coefficient
      s.run(400, x => x.ctrl.state === 'DONE' || x.ctrl.state === 'FAULT'); s.run(3);
      const c = s.ctrl, p = s.plant;
      return { pass: p.p3Dry > 2 || c.state !== 'DONE' || p.flush > 0.15, detail: `datasheet K: Pump-03 dry-run ${fmt(p.p3Dry, 1)} s, flush residue ${fmt(p.flush * 1000, 0)} mL, end ${c.state} (compare TC-08 with calibrated FT-01)` };
    } }
  ];

  function runAll(onEach) {
    return TESTS.map(t => { let r; try { r = t.run(); } catch (e) { r = { pass: false, detail: 'EXCEPTION ' + e.message }; } const out = Object.assign({ id: t.id, req: t.req }, r); if (onEach) onEach(out); return out; });
  }
  root.SimTests = { TESTS, runAll };
  if (typeof module !== 'undefined') module.exports = root.SimTests;
})(typeof window !== 'undefined' ? window : globalThis);
