/* UI + main loop */
(function () {
  'use strict';
  const S = Sim, C = S.C, $ = id => document.getElementById(id), f2 = (x, n) => Number(x).toFixed(n == null ? 2 : n);
  let sys, calFit = null, calTrials = null;

  // ---------- 3D scene ----------
  const scene = new Scene3D($('view'));
  const vb = $('views');
  [['iso', '3D'], ['front', 'Front'], ['top', 'Top'], ['sump', 'Sump'], ['flush', 'Flush line'], ['ft', 'FT-01'], ['control', 'PCB']].forEach(([k, n]) => {
    const b = document.createElement('button'); b.className = 'btn sm'; b.textContent = n; b.onclick = () => scene.setView(k); vb.appendChild(b);
  });

  // ---------- rig lifecycle ----------
  function newSys() {
    sys = new S.System({ plant: { bv2: +$('bv2').value / 100 }, cfg: cfgFromUI(), autoOperator: $('cOp').checked });
    sys.plant.fault = $('fault').value; sys.ctrl.sw = false; logN = 0; $('log').innerHTML = '';
  }
  function cfgFromUI() {
    const c = { case1Seconds: +$('c1t').value || 60 };
    if ($('coefSel').value === 'cal' && calFit) { c.coefA = calFit.a; c.coefB = calFit.b; }
    return c;
  }
  function syncCfg() { Object.assign(sys.ctrl.cfg, cfgFromUI()); sys.autoOperator = $('cOp').checked; sys.plant.bv2 = +$('bv2').value / 100; sys.plant.fault = $('fault').value; }

  $('bStart').onclick = () => { syncCfg(); sys.ctrl.sw = true; };
  $('bStop').onclick = () => { sys.ctrl.sw = false; };
  $('bReset').onclick = () => { newSys(); demo.on = false; };
  let paused = false; const demo = { on: false, injected: false, last: '' };
  $('bPause').onclick = () => { paused = !paused; $('bPause').textContent = paused ? '▶ Resume' : '⏸ Pause'; };
  $('bDemo').onclick = () => {
    if (!calFit) calFit = S.calibrate().fit;                      // calibrate FT-01 first, as the brief requires
    $('coefSel').value = 'cal'; $('c1t').value = 45; $('speed').value = '10'; $('fault').value = 'none'; $('cOp').checked = true;
    newSys(); paused = false; $('bPause').textContent = '⏸ Pause'; syncCfg(); sys.ctrl.sw = true;
    demo.on = true; demo.injected = false; demo.last = ''; scene.setView('iso');
  };
  const STEPS = [['C1_RUN', '1 · Case 1 recirculation'], ['C1_HOLD', '2 · Float hold (P-01 off)'], ['C2_WAIT_BV', '3 · Isolate + open BV-01'], ['C2_FLUSH', '4 · Flush, 3 pumps'], ['DONE', '5 · Auto stop']];
  const seen = {};
  $('steps').innerHTML = STEPS.map(s => '<div class="st" data-s="' + s[0] + '">' + s[1] + '</div>').join('');
  function stepper() {
    const st = sys.ctrl.state; if (st === 'OFF' && sys.t < 0.2) Object.keys(seen).forEach(k => delete seen[k]);
    if (STEPS.some(s => s[0] === st)) seen[st] = 1;
    document.querySelectorAll('.st').forEach(e => { const k = e.dataset.s; e.classList.toggle('on', k === st); e.classList.toggle('done', k !== st && !!seen[k] && !(k === 'C1_HOLD' && st === 'C1_RUN' && false)); e.classList.toggle('bad', st === 'FAULT' && k === 'C2_FLUSH'); });
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
  $('bv2').oninput = e => { $('bv2v').textContent = f2(e.target.value, 0) + ' %'; };
  $('coefSel').onchange = () => { if ($('coefSel').value === 'cal' && !calFit) { alert('Run the calibration in the "Calibration" tab first.'); $('coefSel').value = 'ds'; } };
  scene.onPick = id => { if (id === 'switch') sys.ctrl.sw = !sys.ctrl.sw; else if (id === 'bv1') sys.plant.bv1Open = !sys.plant.bv1Open; else if (id === 'inject') sys.plant.inject(1.5); else if (id === 'bv2') { /* use slider */ } };
  $('bCsv').onclick = () => {
    let s = 't_s,flow_meas_Lmin,flow_true_Lmin,total_L,sump_L,tank_L,flush_L,state\n';
    sys.rec.forEach(r => { s += [r.t, r.q, r.qTrue, r.total, r.sump, r.tank, r.flush, r.state].map(v => typeof v === 'number' ? v.toFixed(4) : v).join(',') + '\n'; });
    const a = document.createElement('a'); a.href = URL.createObjectURL(new Blob([s], { type: 'text/csv' })); a.download = 'flow_vs_time.csv'; a.click();
  };
  $('bShot').onclick = () => { scene.renderer.render(scene.scene, scene.camera); const a = document.createElement('a'); a.href = scene.renderer.domElement.toDataURL('image/png'); a.download = 'rig-3d.png'; a.click(); };

  // ---------- tabs ----------
  const TABS = [['run', 'Simulator'], ['pid', 'P&ID'], ['cal', 'Calibration (T1.2, T2)'], ['theory', 'Theory (T3)'], ['test', 'Tests'], ['spec', 'Spec']];
  TABS.forEach(([id, n], i) => { const b = document.createElement('button'); b.className = 'tab' + (i ? '' : ' on'); b.textContent = n; b.onclick = () => showTab(id); b.dataset.t = id; $('tabs').appendChild(b); });
  function showTab(id) { document.querySelectorAll('.tab').forEach(b => b.classList.toggle('on', b.dataset.t === id)); document.querySelectorAll('.pane').forEach(p => p.classList.toggle('on', p.id === 'p-' + id)); $('app').classList.toggle('wide', id === 'pid'); if (id === 'run') drawCharts(); }
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
    $('lFL').classList.toggle('on', p.floatOn); $('sFL').textContent = p.floatOn ? 'HIGH' : 'low';
    $('sMix').textContent = Math.round(p.cSump * 100); $('lFT').classList.toggle('on', p.cSump < 0.98);
    $('bv1s').textContent = p.bv1Open ? 'OPEN' : 'closed';
    const bn = $('banner'); let msg = c.prompt, cls = '';
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

  // ---------- P&ID ----------
  function pid() {
    const line = (pts, o) => `<polyline points="${pts.map(p => p.join(',')).join(' ')}" fill="none" stroke="${(o && o.c) || '#4da3ff'}" stroke-width="${(o && o.w) || 3}" ${(o && o.d) ? 'stroke-dasharray="6 4"' : ''} ${(o && o.arrow) ? 'marker-end="url(#ar)"' : ''}/>`;
    const tank = (x, y, w, h, t1, t2) => `<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="6" fill="#16283d" stroke="#8fb6e8" stroke-width="2"/><text x="${x + w / 2}" y="${y + h / 2}" text-anchor="middle" font-weight="700">${t1}</text><text class="mut" x="${x + w / 2}" y="${y + h / 2 + 16}" text-anchor="middle">${t2}</text>`;
    const pump = (x, y, t) => `<circle cx="${x}" cy="${y}" r="20" fill="#1b2a3a" stroke="#7bd88f" stroke-width="2.5"/><polygon points="${x - 9},${y - 11} ${x - 9},${y + 11} ${x + 13},${y}" fill="#7bd88f"/><text x="${x}" y="${y + 38}" text-anchor="middle">${t}</text>`;
    const valve = (x, y, t, col) => `<polygon points="${x - 14},${y - 10} ${x - 14},${y + 10} ${x + 14},${y - 10} ${x + 14},${y + 10}" fill="#1b2a3a" stroke="${col || '#f0a44b'}" stroke-width="2.5"/><text x="${x}" y="${y + 28}" text-anchor="middle">${t}</text>`;
    let s = `<svg viewBox="0 0 1000 620" width="100%" style="max-width:1150px;background:#0f161e;border:1px solid #2a3646;border-radius:10px"><defs><marker id="ar" markerWidth="6" markerHeight="6" refX="5" refY="3" markerUnits="userSpaceOnUse" orient="auto"><path d="M0,0 L9,4.5 L0,9 Z" fill="#4da3ff" transform="scale(1.6)"/></marker></defs>`;
    s += tank(30, 190, 110, 200, 'T-01', '10 L process tank') + tank(30, 470, 110, 100, 'F-01', 'flush 4.2 L') + tank(470, 190, 110, 200, 'SMP-01', 'sump 4.5 L');
    // main lines (8 mm ID)
    s += line([[140, 340], [185, 340]]) + line([[215, 340], [275, 340]]) + line([[275, 340], [325, 340]]);
    s += line([[365, 340], [400, 340], [400, 120], [525, 120], [525, 190]], { arrow: 1 });                                 // P-01 -> sump top
    s += line([[580, 360], [615, 360]]) + line([[645, 360], [690, 360]]) + line([[710, 360], [740, 360]]) + line([[770, 360], [825, 360]]) + line([[855, 360], [935, 360], [935, 60], [85, 60], [85, 190]], { arrow: 1 });
    s += line([[140, 520], [185, 520]], { c: '#9fe9ff' }) + line([[215, 520], [245, 520]], { c: '#9fe9ff' }) + line([[285, 520], [300, 520], [300, 340]], { c: '#9fe9ff', arrow: 1 });
    // devices
    s += valve(200, 340, 'OV-01 (ON/OFF)', '#f0a44b') + pump(345, 340, 'P-01 · 50 % PWM');
    s += valve(630, 360, 'BV-03', '#d4af37') + pump(725, 360, 'P-02 · 100 %') + valve(785, 360, 'BV-02', '#d4af37');
    s += valve(200, 520, 'BV-01 (manual)', '#e23b3b') + pump(265, 520, 'P-03 · flush');
    s += `<circle cx="300" cy="340" r="4" fill="#fff"/><text x="262" y="322" class="mut" text-anchor="end">equal tee</text>`;
    s += `<circle cx="840" cy="360" r="17" fill="#241c05" stroke="#ffd24a" stroke-width="2.5"/><text x="840" y="365" text-anchor="middle" font-weight="700">FT</text><text x="840" y="393" text-anchor="middle">FT-01</text><text class="mut" x="840" y="408" text-anchor="middle">flow transmitter</text>`;
    s += `<circle cx="600" cy="285" r="15" fill="#241c05" stroke="#b26cff" stroke-width="2.5"/><text x="600" y="290" text-anchor="middle" font-weight="700" style="font-size:10px">LSH</text><text x="622" y="289" class="mut">FSW-01 float</text><line x1="580" y1="285" x2="585" y2="285" stroke="#b26cff" stroke-width="2"/>`;
    // controller
    s += `<rect x="330" y="450" width="640" height="150" rx="10" fill="#10241a" stroke="#36e06a" stroke-width="2"/><text x="650" y="474" text-anchor="middle" font-weight="700">CUSTOM PCB · ESP32 · 12 V DC (SMPS)</text>`;
    s += `<text class="mut" x="350" y="500">Inputs: FLOW_SENSOR pulse (f) · FLOAT_SWITCH · ON/OFF switch</text><text class="mut" x="350" y="520">Outputs: U2/U3/U4 pump-driver PWM+EN · SOLENOID_VALVE</text><text class="mut" x="350" y="546">Logic: Case 1 (P1 50 %, P2 100 %, float interlock) → Case 2 (P1+P2+P3, stop at 4.2 L)</text><text class="mut" x="350" y="566">Q = a·f + b  → L/min, mL/s, totalizer  →  serial / dashboard</text>`;
    const sig = (x1, y1, x2, y2, c, lab) => `<polyline points="${x1},${y1} ${x1},${(y1 + y2) / 2} ${x2},${(y1 + y2) / 2} ${x2},${y2}" fill="none" stroke="${c}" stroke-width="1.6" stroke-dasharray="5 4"/>`;
    s += sig(840, 378, 900, 450, '#ffd24a') + sig(345, 362, 420, 450, '#7bd88f') + sig(725, 382, 760, 450, '#7bd88f') + sig(265, 542, 380, 450, '#7bd88f') + sig(200, 352, 470, 450, '#f0a44b') + sig(600, 300, 640, 450, '#b26cff');
    // annotations
    s += `<text x="150" y="330" class="mut">Ø8 mm</text><text x="440" y="112" class="mut">Case 1: recirculation ↗</text><text x="450" y="52" class="mut">return line to T-01 (measured by FT-01)</text><text x="310" y="430" class="mut" style="fill:#9fe9ff">Case 2 flush path</text>`;
    s += `<rect x="30" y="590" width="10" height="4" fill="#4da3ff"/><text x="46" y="596" class="mut">process fluid</text><rect x="140" y="590" width="10" height="4" fill="#9fe9ff"/><text x="156" y="596" class="mut">flush water</text><line x1="250" y1="592" x2="272" y2="592" stroke="#b6c3d6" stroke-dasharray="5 4"/><text x="278" y="596" class="mut">electrical signal</text>`;
    s += `</svg>`;
    $('pidBox').innerHTML = s;
  }
  pid();

  // ---------- calibration tab ----------
  const coefNow = () => (calFit ? { a: calFit.a, b: calFit.b } : { a: 1 / C.kDatasheet, b: 0 });
  $('bQ12').onclick = () => {
    const co = coefNow();
    let h = `<table><tr><th>Set Q</th><th>BV-02 %</th><th>f Hz</th><th>v theory</th><th>v measured</th><th>dev.</th><th>Re</th><th>Δp / m</th></tr>`;
    [1, 3, 6].forEach(Q => { const r = S.flowPoint(Q, co), vt = S.velocity(Q), va = S.velocity(r.Qmeas), Re = S.reynolds(Q); h += `<tr><td>${Q} L/min</td><td>${f2(r.valve * 100, 1)}</td><td>${f2(r.freq, 2)}</td><td>${f2(vt, 3)} m/s</td><td>${f2(va, 3)} m/s</td><td>${f2((va - vt) / vt * 100, 1)} %</td><td>${f2(Re, 0)} (${S.regime(Re)})</td><td>${f2(S.dpPerMetre(Q) / 1000, 2)} kPa</td></tr>`; });
    h += `</table><p class="note">v<sub>th</sub> = Q/A, A = π(0.008)²/4 = 5.03×10⁻⁵ m². ${calFit ? 'Using calibrated FT-01.' : 'Using datasheet coefficient (uncalibrated) – run calibration to remove the systematic error.'} 1 L/min sits in the transitional band (Re≈2650), so FT-01 rotor start-up/low-flow accuracy is worst there; 6 L/min gives ≈2 m/s and ≈7 kPa/m friction loss, which is why P-02 cannot deliver 6 L/min once long tubing or a partly closed valve is added.</p>`;
    $('q12').innerHTML = h;
  };
  $('bSweep').onclick = () => {
    const co = coefNow(), sw = S.valveSweep([0.25, 0.5, 0.75, 1.0], co);
    $('sweepTbl').innerHTML = `<table><tr><th>Valve position</th><th>Flow rate (FT-01)</th><th>True</th></tr>` + sw.map(r => `<tr><td>${r.valve * 100} %</td><td>${f2(r.Q)} L/min</td><td>${f2(r.Qtrue)} L/min</td></tr>`).join('') + `</table><p class="note">The paper lists 25/50/75/100 % in the text but "70 %" in the table row; 75 % is used.</p>`;
    chart($('chSweep'), { xmin: 0, xmax: 100, ymin: 0, ymax: 7, ylabel: 'Flow L/min', xlabel: 'valve position %', series: [{ color: '#4da3ff', data: sw.map(r => [r.valve * 100, r.Q]) }, { color: '#4da3ff', pts: 1, data: sw.map(r => [r.valve * 100, r.Q]) }] });
  };
  $('bCal').onclick = () => {
    const { trials, fit } = S.calibrate(); calTrials = trials; calFit = null; window._pendingFit = fit;
    let h = `<table><tr><th>Trial</th><th>Pulses</th><th>Time (s)</th><th>Freq (Hz)</th><th>Volume (L)</th><th>Actual (L/min)</th></tr>` + trials.map((t, i) => `<tr><td>${i + 1}</td><td>${t.pulses}</td><td>${t.time}</td><td>${f2(t.freq)}</td><td>${f2(t.volume, 3)}</td><td>${f2(t.Q)}</td></tr>`).join('') + `</table>`;
    $('calTbl').innerHTML = h;
    const fmax = Math.max.apply(null, trials.map(t => t.freq)) * 1.05;
    chart($('chCal'), { xmin: 0, xmax: fmax, ymin: 0, ymax: 7, ylabel: 'Q L/min', xlabel: 'f Hz', xd: 0, series: [{ name: 'fit', color: '#36e06a', data: [[0, fit.b], [fmax, fit.a * fmax + fit.b]] }, { name: 'trials', color: '#4da3ff', pts: 1, data: trials.map(t => [t.freq, t.Q]) }] });
    const before = S.coefError({ a: 1 / C.kDatasheet, b: 0 }, trials);
    $('calRep').innerHTML = `<h3>Report</h3><table><tr><td>R² =</td><td><b>${f2(fit.r2, 5)}</b></td></tr><tr><td><i>a</i> =</td><td><b>${f2(fit.a, 4)}</b> L/min per Hz</td></tr><tr><td><i>b</i> =</td><td><b>${f2(fit.b, 3)}</b> L/min</td></tr><tr><td>Maximum calibration error =</td><td><b>${f2(fit.maxErr)} %</b></td></tr><tr><td>Average calibration error =</td><td><b>${f2(fit.avgErr)} %</b></td></tr></table><p class="note">Equation: <span class="eq">Q = ${f2(fit.a, 4)}·f ${fit.b < 0 ? '−' : '+'} ${f2(Math.abs(fit.b), 3)}</span>. Datasheet factor (Q = f/7.5) would have given max ${f2(before.max)} % / avg ${f2(before.avg)} % error.</p>`;
    $('bApply').disabled = false;
  };
  $('bApply').onclick = () => { calFit = window._pendingFit; $('coefSel').value = 'cal'; $('bApply').disabled = true; $('bApply').textContent = 'Applied ✓'; setTimeout(() => $('bApply').textContent = 'Apply to controller', 1500); };

  // ---------- theory tab ----------
  function theory() {
    const V = +$('thV').value, Q = +$('thQ').value, T = S.residence(V, Q);
    $('thRes').innerHTML = `<div class="card"><div class="k">T<sub>r</sub> = V / Q = ${V} L ÷ ${Q} L/min</div><div class="v">${f2(T, 1)} s <span class="u">(${f2(T / 60, 3)} min)</span></div></div>
    <p class="note">The paper writes T<sub>r</sub> = Q/V, which has units of 1/time; residence time is V/Q (0.5 / 2.5 = 0.2 min = <b>12 s</b>).</p>
    <ol style="padding-left:18px;margin:6px 0">
    <li><b>Physical meaning:</b> average time a fluid element needs to pass through that tubing volume, i.e. the delay between something happening at the inlet (valve step, pump start, bubble) and it reaching the outlet/sensor.</li>
    <li><b>Volume doubled, Q constant:</b> T doubles → ${f2(S.residence(2 * V, Q), 1)} s. The system reacts more slowly.</li>
    <li><b>Q doubled, V constant:</b> T halves → ${f2(S.residence(V, 2 * Q), 1)} s. Faster response.</li>
    <li><b>Why it matters for a sudden change:</b> the measured flow lags the cause by ≈T<sub>r</sub> plus sensor filtering; readings taken sooner than ~3T<sub>r</sub> after a change are still transient. Do not diagnose a fault from the first seconds of data, and account for the delay when tuning stop conditions (e.g. flush end) or correlating sump level with FT-01.</li></ol>`;
  }
  ['thV', 'thQ'].forEach(i => $(i).oninput = theory); theory();
  function faults() {
    let h = `<table><tr><th>Cond.</th><th>Pump</th><th>Valve</th><th>FT-01 pulse</th><th>Flow</th><th>Most likely condition</th><th></th></tr>`;
    S.FAULTS.forEach(f => {
      const sg = S.faultSignature(f.sim[0], f.sim[1]);
      h += `<tr><td><b>${f.id}</b></td><td>${f.pump}</td><td>${f.valve}</td><td>${f.pulse}<div class="note">sim: ${sg.pulse}, I<sub>P2</sub> ${f2(sg.pumpCurrent)} A, true ${f2(sg.trueFlow)} L/min</div></td><td>${f.flow}</td><td><b>${f.cause}</b><div class="note">${f.detail}</div></td><td><button class="btn sm" data-f="${f.id}">Run in 3D</button></td></tr>`;
    });
    h += `</table><h3>Hardest to distinguish with FT-01 alone</h3><p><b>Condition C</b> (pump ON, valve open, zero pulses, zero indicated flow) – sensor failure, blockage and pump failure all give the same FT-01 output. (E is second-hardest: air ingress vs. loose sensor contact.)</p>
    <p><b>Extra information to separate them:</b></p><ul style="padding-left:18px;margin:4px 0"><li><b>Pump-driver current / feedback</b> – high ⇒ blockage/dead-head, near zero ⇒ dead pump, normal-low ⇒ pump turning but sensor or no-prime.</li><li><b>Sump and T-01 level trend</b> – level still falling/rising ⇒ liquid <i>is</i> moving ⇒ FT-01 failed; static ⇒ real no-flow.</li><li>Independent measurement: timed jug (V/t) or second flow meter; upstream/downstream pressure switch.</li><li>Sensor self-check: supply voltage/pull-up level on FLOW_SENSOR pin (stuck high/low = wiring), manual rotor spin.</li><li>Visual: bubbles, kinked tube, closed BV-03/BV-02, float-switch state.</li></ul>`;
    $('faultTbl').innerHTML = h;
    $('faultTbl').querySelectorAll('button[data-f]').forEach(b => b.onclick = () => {
      const f = S.FAULTS.find(x => x.id === b.dataset.f); newSys(); $('fault').value = f.sim[0] === 'none' && !f.sim[1] ? 'pump' : f.sim[0]; sys.plant.fault = $('fault').value; sys.ctrl.sw = true; $('speed').value = '5'; showTab('run');
    });
  }
  faults();

  // ---------- tests tab ----------
  $('bTests').onclick = () => {
    $('tOut').innerHTML = '<p class="note">Running headless simulations…</p>'; $('tSum').textContent = '';
    setTimeout(() => {
      const t0 = performance.now(), res = SimTests.runAll(), np = res.filter(r => r.pass).length;
      $('tSum').textContent = np + '/' + res.length + ' passed · ' + Math.round(performance.now() - t0) + ' ms';
      $('tOut').innerHTML = `<table><tr><th>ID</th><th>Requirement / result</th><th></th></tr>` + res.map(r => `<tr><td><b>${r.id}</b></td><td>${r.req}<div class="note">${r.detail}</div></td><td class="${r.pass ? 'pass' : 'fail'}">${r.pass ? 'PASS' : 'FAIL'}</td></tr>`).join('') + `</table>`;
    }, 30);
  };

  // ---------- spec tab ----------
  $('specOut').innerHTML = `
  <h3>Requirement traceability</h3>
  <table><tr><th>Requirement (question paper)</th><th>Where</th></tr>
  <tr><td>Case 1: closed loop T-01 → OV-01 → P-01 → sump → P-02 → FT-01 → T-01; P-01 50 %, P-02 100 %</td><td>Controller C1_RUN · TC-01, 02</td></tr>
  <tr><td>Calibrate FT-01; Q = a·f + b; R², a, b, max/avg error</td><td>Calibration tab · TC-03, 04</td></tr>
  <tr><td>Sudden external fluid: P-01 stops, P-02 keeps running, P-01 restarts</td><td>Float interlock C1_HOLD · TC-05</td></tr>
  <tr><td>Flow vs time (mL/s and L/min), total litres returned to 10 L tank, share code/screenshots</td><td>Charts, totalizer, CSV/PNG export · TC-06</td></tr>
  <tr><td>Case 2: automatic hand-over, OV-01 isolates T-01, manual BV-01 before P-03 starts, all three pumps together</td><td>C2_WAIT_BV → C2_FLUSH · TC-07, 08</td></tr>
  <tr><td>System stops as soon as the 4.2 L flush tank finishes, no manual action besides BV-01</td><td>Flush totalizer stop · TC-08, 16</td></tr>
  <tr><td>Start with the supplied ON/OFF switch; safe stop</td><td>Switch model · TC-10</td></tr>
  <tr><td>Q1.1 labelled P&amp;ID</td><td>P&amp;ID tab</td></tr>
  <tr><td>Q1.2 1/3/6 L/min theoretical vs actual velocity</td><td>Calibration tab · TC-11</td></tr>
  <tr><td>Q2.1 valve position vs flow (25/50/75/100 %) and graph</td><td>Calibration tab · TC-12</td></tr>
  <tr><td>Q2.3 five trials: pulses, time, f, volume, actual flow</td><td>Calibration tab · TC-13</td></tr>
  <tr><td>Q3.1 residence time and four questions</td><td>Theory tab · TC-14</td></tr>
  <tr><td>Q3.2 fault conditions A–E, hardest case, extra data</td><td>Theory tab · TC-15</td></tr></table>
  <h3>Given parameters used</h3><p>D = 8 mm, ρ = 1000 kg/m³, μ = 1×10⁻³ Pa·s, range 1–6 L/min, target 3 L/min, tank 10 L, sump ≈4.5 L, flush 4.2 L, 12 V DC.</p>
  <h3>Modelling assumptions (change in sim.js)</h3><ul style="padding-left:18px">
  <li>Each pump gives 6 L/min at 100 % duty; PWM is averaged (P-01 at 50 % ⇒ 3 L/min). BV-02 throttles P-02 to ≈3 L/min (28 %) so both pumps balance.</li>
  <li>FT-01 true response f = 7.30·Q − 0.45 (Hz), i.e. it is <i>not</i> the datasheet 7.5 Hz/(L/min) – hence the need to calibrate.</li>
  <li>One float switch in the sump trips at 3.0 L (reset 0.15 L lower); P-01 restarts 3 s after it clears.</li>
  <li>Case 1 lasts a configurable time (default 60 s) before automatic hand-over.</li>
  <li>The flush stop uses the FT-01 totalizer (no level sensor on F-01 in the hardware list). BV-01 has no sensor, so the controller allows 10 s after closing OV-01 for the operator to open it, and trips on no-flow if it was forgotten.</li>
  <li>Flush water ends up in T-01 (the only return path in the hardware list); it is not routed to drain.</li></ul>
  <h3>Discrepancies in the paper</h3><ul style="padding-left:18px"><li>T<sub>r</sub> = Q/V is written inverted; V/Q is used.</li><li>Valve table says 70 %, text says 75 %.</li><li>P&amp;ID asks for "needle valves"; hardware list has ball valves.</li></ul>
  <h3>Controls</h3><p class="note">Left-drag orbit · right-drag / Shift-drag pan · wheel zoom. Click the ON/OFF switch, BV-01 handle or the amber jug in the 3D view.</p>`;
  // optional hash hooks for demos / headless checks: #tab=cal&click=bCal,bSweep&speed=20&start&inject=45&view=front
  (function () {
    const h = new URLSearchParams(location.hash.slice(1)); if (![...h.keys()].length) return;
    if (h.get('speed')) $('speed').value = h.get('speed'); if (h.get('c1t')) $('c1t').value = h.get('c1t');
    if (h.get('view')) scene.setView(h.get('view'));
    if (h.has('start')) sys.ctrl.sw = true;
    if (h.get('ff')) { const T = +h.get('ff'), ti = h.get('inject') ? +h.get('inject') : -1; let inj = false; $('speed').value = '1'; while (sys.t < T) { if (!inj && ti >= 0 && sys.t >= ti) { sys.plant.inject(1.5); inj = true; } sys.step(); } $('speed').value = h.get('speed') || '1'; drawCharts(); }
    if (h.get('click')) h.get('click').split(',').forEach(id => $(id).click());
    if (h.get('tab')) showTab(h.get('tab'));
  })();
})();
