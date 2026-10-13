/* UI + main loop for 3D Rig Simulation & Cases Flow */
(function () {
  'use strict';
  const S = Sim, C = S.C, $ = id => document.getElementById(id), f2 = (x, n) => Number(x).toFixed(n == null ? 2 : n);
  let sys, calFit = null;

  // Calibrate on startup to ensure high precision
  calFit = S.calibrate().fit;

  // ---------- 3D scene ----------
  const scene = new Scene3D($('view'));
  const vb = $('views');
  [['iso', '3D Isometric'], ['front', 'Front'], ['top', 'Top'], ['sump', 'Sump'], ['flush', 'Flush Line'], ['ft', 'FT-01 Sensor'], ['control', 'Control PCB']].forEach(([k, n]) => {
    const b = document.createElement('button'); b.className = 'btn sm'; b.textContent = n; b.onclick = () => scene.setView(k); vb.appendChild(b);
  });

  // ---------- rig lifecycle ----------
  function newSys() {
    sys = new S.System({ plant: { bv2: 0.283 }, cfg: cfgFromUI(), autoOperator: $('cOp').checked });
    sys.plant.fault = 'none'; sys.ctrl.sw = false; logN = 0; $('log').innerHTML = '';
  }
  function cfgFromUI() {
    const c = { case1Seconds: 60 };
    if (calFit) { c.coefA = calFit.a; c.coefB = calFit.b; }
    return c;
  }
  function syncCfg() {
    Object.assign(sys.ctrl.cfg, cfgFromUI());
    sys.autoOperator = $('cOp').checked;
    sys.plant.bv2 = 0.283;
    sys.plant.fault = 'none';
  }

  $('bStart').onclick = () => { syncCfg(); sys.ctrl.sw = true; };
  $('bStop').onclick = () => { sys.ctrl.sw = false; };
  $('bReset').onclick = () => { newSys(); demo.on = false; };
  let paused = false; const demo = { on: false, injected: false, last: '' };
  $('bPause').onclick = () => { paused = !paused; $('bPause').textContent = paused ? '▶ Resume' : '⏸ Pause'; };
  $('bDemo').onclick = () => {
    newSys(); paused = false; $('bPause').textContent = '⏸ Pause'; $('speed').value = '5'; $('cOp').checked = true; syncCfg(); sys.ctrl.sw = true;
    demo.on = true; demo.injected = false; demo.last = ''; scene.setView('iso');
  };
  const STEPS = [['C1_RUN', '1 · Case 1 recirculation'], ['C1_HOLD', '2 · Float hold (P-01 off)'], ['C2_WAIT_BV', '3 · Isolate + open BV-01'], ['C2_FLUSH', '4 · Flush, 3 pumps'], ['DONE', '5 · Auto stop']];
  const seen = {};
  $('steps').innerHTML = STEPS.map(s => '<div class="st" data-s="' + s[0] + '">' + s[1] + '</div>').join('');
  function stepper() {
    const st = sys.ctrl.state; if (st === 'OFF' && sys.t < 0.2) Object.keys(seen).forEach(k => delete seen[k]);
    if (STEPS.some(s => s[0] === st)) seen[st] = 1;
    document.querySelectorAll('.st').forEach(e => { const k = e.dataset.s; e.classList.toggle('on', k === st); e.classList.toggle('done', k !== st && !!seen[k]); e.classList.toggle('bad', st === 'FAULT' && k === 'C2_FLUSH'); });
  }
  function runDemo() {
    if (!demo.on) return; const st = sys.ctrl.state;
    if (!demo.injected && st === 'C1_RUN' && sys.t > 20) { sys.plant.inject(1.5); demo.injected = true; scene.setView('sump'); }
    const view = { C1_RUN: demo.injected ? 'sump' : 'iso', C1_HOLD: 'sump', C2_WAIT_BV: 'flush', C2_FLUSH: 'front', DONE: 'iso', FAULT: 'iso' }[st];
    if (view && demo.last !== st) { demo.last = st; if (st !== 'C1_RUN' || !demo.injected) scene.setView(view); }
    if (st === 'DONE' || st === 'FAULT') demo.on = false;
  }
  $('bInject').onclick = () => sys.plant.inject(1.5);
  $('bBV1').onclick = () => { sys.plant.bv1Open = !sys.plant.bv1Open; };
  $('cLabels').onchange = e => scene.showLabels(e.target.checked);
  scene.onPick = id => { if (id === 'switch') sys.ctrl.sw = !sys.ctrl.sw; else if (id === 'bv1') sys.plant.bv1Open = !sys.plant.bv1Open; else if (id === 'inject') sys.plant.inject(1.5); };
  $('bCsv').onclick = () => {
    let s = 't_s,flow_meas_Lmin,flow_true_Lmin,total_L,sump_L,tank_L,flush_L,state\n';
    sys.rec.forEach(r => { s += [r.t, r.q, r.qTrue, r.total, r.sump, r.tank, r.flush, r.state].map(v => typeof v === 'number' ? v.toFixed(4) : v).join(',') + '\n'; });
    const a = document.createElement('a'); a.href = URL.createObjectURL(new Blob([s], { type: 'text/csv' })); a.download = 'flow_vs_time.csv'; a.click();
  };
  $('bShot').onclick = () => { scene.renderer.render(scene.scene, scene.camera); const a = document.createElement('a'); a.href = scene.renderer.domElement.toDataURL('image/png'); a.download = 'rig-3d.png'; a.click(); };

  // ---------- tabs ----------
  const TABS = [['run', '3D Rig Controls'], ['cases', 'Cases Explanation']];
  TABS.forEach(([id, n], i) => { const b = document.createElement('button'); b.className = 'tab' + (i ? '' : ' on'); b.textContent = n; b.onclick = () => showTab(id); b.dataset.t = id; $('tabs').appendChild(b); });
  function showTab(id) { document.querySelectorAll('.tab').forEach(b => b.classList.toggle('on', b.dataset.t === id)); document.querySelectorAll('.pane').forEach(p => p.classList.toggle('on', p.id === 'p-' + id)); if (id === 'run') drawCharts(); }
  showTab('run');

  // ---------- charts ----------
  function chart(cv, o) {
    const dpr = devicePixelRatio || 1, w = cv.clientWidth, h = cv.clientHeight; if (!w) return;
    cv.width = w * dpr; cv.height = h * dpr; const g = cv.getContext('2d'); g.scale(dpr, dpr);
    const L = 42, R = o.y2 ? 46 : 10, Tp = 12, B = 24, pw = w - L - R, ph = h - Tp - B;
    let x0 = o.xmin, x1 = o.xmax, y0 = o.ymin, y1 = o.ymax;
    const X = x => L + (x - x0) / (x1 - x0 || 1) * pw, Y = y => Tp + ph - (y - y0) / (y1 - y0 || 1) * ph;
    g.font = '11px system-ui'; g.fillStyle = '#8fa0b6'; g.strokeStyle = '#243040'; g.lineWidth = 1;
    for (let i = 0; i <= 4; i++) { const v = y0 + (y1 - y0) * i / 4, y = Y(v); g.beginPath(); g.moveTo(L, y); g.lineTo(L + pw, y); g.stroke(); g.textAlign = 'right'; g.fillText(v.toFixed(o.yd == null ? 1 : o.yd), L - 5, y + 4); if (o.y2) { g.textAlign = 'left'; g.fillText((v * o.y2).toFixed(0), L + pw + 5, y + 4); } }
    for (let i = 0; i <= 4; i++) { const v = x0 + (x1 - x0) * i / 4; g.textAlign = 'center'; g.fillText(v.toFixed(o.xd == null ? 0 : o.xd), X(v), h - 8); }
    g.textAlign = 'left'; if (o.ylabel) g.fillText(o.ylabel, 4, 10); if (o.y2label) { g.textAlign = 'right'; g.fillText(o.y2label, w - 2, 10); }
    if (o.xlabel) { g.textAlign = 'right'; g.fillText(o.xlabel, L + pw, h - 8 - 11); }
    (o.hlines || []).forEach(hl => { g.strokeStyle = hl.color; g.setLineDash([5, 4]); g.beginPath(); g.moveTo(L, Y(hl.y)); g.lineTo(L + pw, Y(hl.y)); g.stroke(); g.setLineDash([]); });
    g.save(); g.beginPath(); g.rect(L, Tp - 2, pw, ph + 4); g.clip();
    (o.series || []).forEach(s => {
      g.strokeStyle = s.color; g.fillStyle = s.color; g.lineWidth = s.w || 2; if (s.dash) g.setLineDash(s.dash);
      if (s.pts) s.data.forEach(p => { g.beginPath(); g.arc(X(p[0]), Y(p[1]), 4, 0, 7); g.fill(); });
      else { g.beginPath(); s.data.forEach((p, i) => i ? g.lineTo(X(p[0]), Y(p[1])) : g.moveTo(X(p[0]), Y(p[1]))); g.stroke(); }
      g.setLineDash([]);
    });
    g.restore();
    let lx = L + 6; g.textAlign = 'left'; (o.series || []).forEach(s => { if (!s.name) return; g.fillStyle = s.color; g.fillRect(lx, Tp + 4, 10, 3); g.fillStyle = '#c9d6e8'; g.fillText(s.name, lx + 14, Tp + 9); lx += 22 + g.measureText(s.name).width; });
  }
  function drawCharts() {
    if (!sys || !$('p-run').classList.contains('on')) return;
    const r = sys.rec; if (r.length < 2) { chart($('chFlow'), { xmin: 0, xmax: 60, ymin: 0, ymax: 7, ylabel: 'L/min', y2: 1000 / 60, y2label: 'mL/s', xlabel: 's', hlines: [{ y: 3, color: '#36e06a' }] }); chart($('chLvl'), { xmin: 0, xmax: 60, ymin: 0, ymax: 10, ylabel: 'L' }); return; }
    const xmax = Math.max(60, r[r.length - 1].t), xmin = Math.max(0, xmax - 240);
    const v = r.filter(p => p.t >= xmin);
    chart($('chFlow'), { xmin, xmax, ymin: 0, ymax: 7, ylabel: 'L/min', y2: 1000 / 60, y2label: 'mL/s', xlabel: 's', hlines: [{ y: 3, color: '#2c8a4d' }],
      series: [{ name: 'FT-01 measured', color: '#4da3ff', data: v.map(p => [p.t, p.q]) }, { name: 'true', color: '#8fa0b6', w: 1, dash: [3, 3], data: v.map(p => [p.t, p.qTrue]) }] });
    chart($('chLvl'), { xmin, xmax, ymin: 0, ymax: 10, ylabel: 'L', xlabel: 's', hlines: [{ y: C.sumpFloat, color: '#a5732a' }],
      series: [{ name: 'T-01', color: '#4da3ff', data: v.map(p => [p.t, p.tank]) }, { name: 'sump', color: '#ffb020', data: v.map(p => [p.t, p.sump]) }, { name: 'flush', color: '#7be8ff', data: v.map(p => [p.t, p.flush]) }] });
  }

  // ---------- live UI refresh ----------
  let logN = 0;
  function refresh() {
    const p = sys.plant, c = sys.ctrl;
    $('hState').textContent = c.state; $('hQ').textContent = f2(c.qMeas); $('hQs').textContent = f2(c.qMeas * 1000 / 60, 1); $('hTot').textContent = f2(c.total); $('hT').textContent = f2(sys.t, 1);
    $('vQ').textContent = f2(c.qMeas); $('vQs').textContent = f2(c.qMeas * 1000 / 60, 1) + ' mL/s · v = ' + f2(S.velocity(c.qMeas), 2) + ' m/s';
    $('vF').textContent = f2(c.freq, 1); $('vN').textContent = p.pulses + ' pulses';
    $('vTot').textContent = f2(c.total); $('vTrue').textContent = 'true ' + f2(p.volTrue) + ' L · Case-1 ' + f2(c.totalC1) + ' L';
    [['T', p.tank, C.tankCap], ['S', p.sump, C.sumpCap], ['F', p.flush, C.flushVol]].forEach(([k, v, m]) => { $('lv' + k).textContent = f2(v); $('b' + k).style.width = Math.min(100, v / m * 100) + '%'; });
    [0, 1, 2].forEach(i => { $('dP' + (i + 1)).textContent = Math.round(p.duty[i] * 100); $('lP' + (i + 1)).classList.toggle('on', p.duty[i] > 0); $('iP' + (i + 1)).textContent = p.cur[i] > 0 ? f2(p.cur[i]) + ' A' : ''; });
    $('lOV').classList.toggle('on', p.ov); $('sOV').textContent = p.ov ? 'open' : 'closed';
    $('lFL').classList.toggle('on', p.ls); $('sFL').textContent = p.ls ? 'HIGH (trip)' : 'low';
    $('lFT').classList.toggle('on', p.flushMix > 0.05); $('sMix').textContent = Math.round(p.flushMix * 100);
    $('bv1s').textContent = p.bv1Open ? 'OPEN' : 'closed';
    const bn = $('banner'); let msg = c.prompt, cls = 'warn';
    if (c.state === 'FAULT') cls = 'bad'; else if (c.state === 'DONE') cls = 'ok';
    if (!msg && p.injectQueue > 0) msg = 'External fluid entering the sump…';
    if (!msg && c.state === 'C1_HOLD') msg = 'FLOAT HIGH – Pump-01 stopped, Pump-02 draining the sump';
    if (!msg && p.p3Dead > 0.5 && p.duty[2] > 0) { msg = 'Pump-03 running with BV-01 closed (dead-head)!'; cls = 'bad'; }
    bn.style.display = msg ? 'block' : 'none'; bn.textContent = msg || ''; bn.className = cls;
    const ev = c.events; while (logN < ev.length) { const e = ev[logN++]; const d = document.createElement('div'); d.textContent = '[' + f2(e.t, 1).padStart(6) + ' s] ' + e.m; $('log').appendChild(d); $('log').scrollTop = 1e9; }
  }

  // ---------- main loop ----------
  newSys(); scene.showLabels(true);
  let last = performance.now(), acc = 0, chT = 0, blinkT = 0;
  function frame(now) {
    const dt = Math.min(0.1, (now - last) / 1000); last = now;
    syncCfg();
    acc += dt * (+$('speed').value); let n = 0;
    if (paused) acc = 0;
    while (acc >= sys.dt && n < 600) { sys.step(); acc -= sys.dt; n++; }
    runDemo(); stepper();
    if (acc > 5) acc = 0;
    blinkT += dt;
    scene.update(sys, dt, Math.floor(blinkT * 4) % 2 === 0 || (sys.plant.freq > 0.3 && Math.floor(blinkT * Math.min(sys.plant.freq, 12)) % 2 === 0));
    refresh(); chT += dt; if (chT > 0.25) { chT = 0; drawCharts(); }
    requestAnimationFrame(frame);
  }
  requestAnimationFrame(frame);
})();
