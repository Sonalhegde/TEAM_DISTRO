/* 3D digital twin of the Stage-2 rig (Three.js r128).
   Faithfully matches the physical laboratory testbench layout:
   - 3x 12V Diaphragm Pumps with verified physical orientations & port routings:
     * Pump-01 (Top-Right): Mounted vertically, head at top, ports left (inlet) & right (outlet to Sump)
     * Pump-03 (Center): Mounted horizontally, head at left, ports top (outlet to Equal Tee) & bottom (inlet from BV-01)
     * Pump-02 (Bottom-Right): Mounted vertically, head at bottom, ports right (inlet from Sump) & left (outlet to FT-01)
   - Rectangular clear plastic tubs on dark risers (Process Tank T-01, Flush Tank F-01, Sump Tank SMP-01)
   - Accurate hydraulic plumbing loops (Equal Tee, BV-01 flush valve, FT-01 flow transmitter, BV-02 throttle valve)
   - SMPS, custom ESP32 blue PCB, 3x pump drivers, and realistic multi-color wiring harnesses
   - Strictly clean component names – ALL external measurements and graduation ticks removed.
*/
(function (root) {
  'use strict';
  const T = root.THREE, S = root.Sim, C = S.C;
  const TY = 0.55; // tube centerline height above the board

  function Scene3D(host) {
    const self = this;
    const w = host.clientWidth || 800, h = host.clientHeight || 600;
    const renderer = new T.WebGLRenderer({ antialias: true, alpha: false, powerPreference: 'high-performance' });
    renderer.setPixelRatio(Math.min(root.devicePixelRatio || 1, 2));
    renderer.setSize(w, h);
    renderer.shadowMap.enabled = true;
    renderer.shadowMap.type = T.PCFSoftShadowMap;
    renderer.toneMapping = T.ACESFilmicToneMapping;
    renderer.toneMappingExposure = 1.05;
    host.appendChild(renderer.domElement);

    const scene = new T.Scene();
    scene.background = new T.Color(0x11161d);
    scene.fog = new T.Fog(0x11161d, 35, 80);

    const camera = new T.PerspectiveCamera(45, w / h, 0.1, 200);
    this.renderer = renderer;
    this.scene = scene;
    this.camera = camera;
    this.host = host;

    // ----- Laboratory Workbench Lighting
    const hemi = new T.HemisphereLight(0xe8f0ff, 0x1c222b, 0.78);
    scene.add(hemi);

    const keySun = new T.DirectionalLight(0xfff8ee, 1.05);
    keySun.position.set(-8, 18, 10);
    keySun.castShadow = true;
    keySun.shadow.mapSize.set(2048, 2048);
    Object.assign(keySun.shadow.camera, { left: -18, right: 18, top: 14, bottom: -14, near: 1, far: 60 });
    keySun.shadow.bias = -0.0003;
    scene.add(keySun);

    const fillLight = new T.DirectionalLight(0x8ab2e8, 0.35);
    fillLight.position.set(12, 10, -6);
    scene.add(fillLight);

    const topSoft = new T.DirectionalLight(0xffffff, 0.25);
    topSoft.position.set(0, 15, 0);
    scene.add(topSoft);

    // ----- Helpers & Materials
    const std = (c, o) => new T.MeshStandardMaterial(Object.assign({ color: c, roughness: 0.55, metalness: 0.1 }, o || {}));
    const tubMat = new T.MeshPhysicalMaterial({
      color: 0xe8f4ff,
      transparent: true,
      opacity: 0.42,
      roughness: 0.12,
      metalness: 0.05,
      transmission: 0.6,
      ior: 1.45,
      side: T.DoubleSide,
      depthWrite: false
    });
    const add = (m, x, y, z, parent) => { m.position.set(x, y, z); (parent || scene).add(m); return m; };
    const cylM = (r, hh, mat, seg) => new T.Mesh(new T.CylinderGeometry(r, r, hh, seg || 24), mat);
    const boxM = (a, b, c, mat) => new T.Mesh(new T.BoxGeometry(a, b, c), mat);
    const sph = (r, mat) => new T.Mesh(new T.SphereGeometry(r, 16, 12), mat);

    this.pick = [];
    this.labels = [];

    // ----- Clean component name badge (Strictly component names, NO external measurements)
    function label(text, x, y, z, color) {
      const cv = document.createElement('canvas');
      cv.width = 512;
      cv.height = 96;
      const g = cv.getContext('2d');
      g.font = '700 34px system-ui, -apple-system, BlinkMacSystemFont, Segoe UI, Roboto, sans-serif';
      g.textAlign = 'center';
      g.textBaseline = 'middle';
      const textWidth = g.measureText(text).width;
      const wd = Math.min(490, textWidth + 44);

      g.fillStyle = 'rgba(16, 22, 31, 0.88)';
      g.strokeStyle = color || '#5ca3ff';
      g.lineWidth = 3.5;
      if (g.roundRect) {
        g.roundRect(256 - wd / 2, 14, wd, 68, 16);
      } else {
        g.rect(256 - wd / 2, 14, wd, 68);
      }
      g.fill();
      g.stroke();

      g.fillStyle = '#f0f5fc';
      g.fillText(text, 256, 48);

      const sp = new T.Sprite(new T.SpriteMaterial({
        map: new T.CanvasTexture(cv),
        transparent: true,
        depthTest: false
      }));
      sp.scale.set(2.4, 0.45, 1);
      sp.position.set(x, y, z);
      sp.renderOrder = 10;
      scene.add(sp);
      self.labels.push(sp);
      return sp;
    }

    // =========================================================================
    // 1. WORKBENCH TABLE & CENTRAL PLYWOOD MOUNTING BOARD
    // =========================================================================

    // Lab bench table top
    const tableTop = add(boxM(34, 0.4, 20, std(0xdbd1c1, { roughness: 0.75, metalness: 0.05 })), 0, -0.4, 0.5);
    tableTop.receiveShadow = true;

    // Table legs & frame
    const metalLeg = std(0x232930, { metalness: 0.7, roughness: 0.4 });
    [[-16.5, -9], [16.5, -9], [-16.5, 9.8], [16.5, 9.8]].forEach(([lx, lz]) => {
      add(boxM(0.7, 5, 0.7, metalLeg), lx, -2.9, lz);
    });

    // Laboratory back wall divider with dual AC electrical outlet switchboards
    add(boxM(34, 3.8, 0.3, std(0xc7bdab, { roughness: 0.8 })), 0, 1.7, -9.6);
    add(boxM(34.2, 0.25, 1.2, std(0xb3a896, { roughness: 0.85 })), 0, 3.65, -9.1);

    function wallSocket(x) {
      const g = new T.Group(); g.position.set(x, 1.3, -9.42); scene.add(g);
      g.add(boxM(2.2, 0.9, 0.08, std(0xf7fafc, { roughness: 0.4 })));
      [-0.55, 0.55].forEach(sx => {
        g.add(boxM(0.65, 0.55, 0.09, std(0x282c34, { roughness: 0.8 }))).position.x = sx;
        g.add(boxM(0.3, 0.25, 0.1, std(0xdde3ea))).position.set(sx, -0.1, 0.01);
      });
    }
    wallSocket(-4.5);
    wallSocket(8.5);

    // Floor below
    const floor = add(new T.Mesh(new T.PlaneGeometry(120, 120), std(0x0e141a, { roughness: 0.95 })), 0, -5.4, 0);
    floor.rotation.x = -Math.PI / 2;
    floor.receiveShadow = true;

    // Wooden Plywood Mounting Board (central baseboard)
    const boardW = 15.6, boardH = 0.24, boardD = 11.8;
    const woodTopMat = std(0xcaa170, { roughness: 0.65, metalness: 0.05 });
    const woodEdgeMat = std(0x8f6a42, { roughness: 0.8, metalness: 0.02 });
    const plywoodBoard = add(new T.Mesh(new T.BoxGeometry(boardW, boardH, boardD),
      [woodEdgeMat, woodEdgeMat, woodTopMat, woodEdgeMat, woodEdgeMat, woodEdgeMat]), 0.5, -0.08, 0.3);
    plywoodBoard.receiveShadow = true;
    plywoodBoard.castShadow = true;

    // Corner mounting screws
    const screwMat = std(0xdde2ea, { metalness: 0.85, roughness: 0.3 });
    [[-6.8, -5.2], [7.8, -5.2], [-6.8, 5.8], [7.8, 5.8]].forEach(([sx, sz]) => {
      add(cylM(0.12, 0.05, screwMat, 12), sx, 0.06, sz);
      add(cylM(0.06, 0.08, std(0x1a1a1a), 12), sx, 0.07, sz);
    });

    // =========================================================================
    // 2. RECTANGULAR STORAGE TANKS
    // =========================================================================
    function createRectTub(cx, cz, wX, hY, dZ, tag, hasRiser, color) {
      const g = new T.Group();
      const baseElevation = hasRiser ? 0.6 : 0.02;
      g.position.set(cx, baseElevation, cz);
      scene.add(g);

      if (hasRiser) {
        const riser = boxM(wX + 0.4, 0.58, dZ + 0.4, std(0x1f242b, { roughness: 0.9 }));
        riser.position.set(cx, 0.29, cz);
        riser.receiveShadow = true;
        riser.castShadow = true;
        scene.add(riser);
      }

      const wallThick = 0.1;
      const btm = boxM(wX, wallThick, dZ, tubMat);
      btm.position.y = wallThick / 2;
      g.add(btm);

      const wFront = boxM(wX, hY, wallThick, tubMat); wFront.position.set(0, hY / 2, dZ / 2); g.add(wFront);
      const wBack = boxM(wX, hY, wallThick, tubMat); wBack.position.set(0, hY / 2, -dZ / 2); g.add(wBack);
      const wLeft = boxM(wallThick, hY, dZ, tubMat); wLeft.position.set(-wX / 2, hY / 2, 0); g.add(wLeft);
      const wRight = boxM(wallThick, hY, dZ, tubMat); wRight.position.set(wX / 2, hY / 2, 0); g.add(wRight);

      const rim = boxM(wX + 0.26, 0.12, dZ + 0.26, tubMat);
      rim.position.y = hY;
      g.add(rim);

      // Brass bulkhead fitting on lower-right side
      const brassFitting = cylM(0.12, 0.32, std(0xc9a43a, { metalness: 0.8, roughness: 0.3 }), 14);
      brassFitting.rotation.z = Math.PI / 2;
      brassFitting.position.set(wX / 2 + 0.08, 0.32, dZ / 4);
      g.add(brassFitting);

      // Water fluid body
      const wmat = new T.MeshStandardMaterial({
        color: color || 0x1f75e8,
        transparent: true,
        opacity: 0.75,
        roughness: 0.18,
        metalness: 0.05
      });
      const water = boxM(wX - wallThick * 2, 1.0, dZ - wallThick * 2, wmat);
      water.position.y = 0.5;
      g.add(water);

      const surf = new T.Mesh(new T.PlaneGeometry(wX - wallThick * 2, dZ - wallThick * 2),
        new T.MeshStandardMaterial({ color: 0x98d4ff, transparent: true, opacity: 0.55, roughness: 0.06, side: T.DoubleSide }));
      surf.rotation.x = -Math.PI / 2;
      g.add(surf);

      label(tag, cx, baseElevation + hY + 0.75, cz, '#5ca3ff');

      const area = (wX - wallThick * 2) * (dZ - wallThick * 2);
      return { g, water, wmat, surf, area, h: hY, cx, cz, baseElevation };
    }

    // Process Tank (T-01) – Upper-Left on dark riser
    this.tank = createRectTub(-9.8, -3.2, 3.8, 2.8, 3.4, 'Process Tank (T-01)', true, 0x1f75e8);

    // Flush Tank (F-01) – Lower-Left on dark riser
    this.fl = createRectTub(-9.8, 3.4, 3.8, 2.4, 3.4, 'Flush Tank (F-01)', true, 0x76ddf7);

    // Sump Tank (SMP-01) – Right side on bench
    this.sump = createRectTub(10.6, -1.2, 4.0, 2.6, 3.6, 'Sump Tank (SMP-01)', false, 0x2278e6);

    // Float Switch (FSW-01) clamped onto the front-right rim of Sump Tank
    (function () {
      const g = new T.Group();
      g.position.set(12.4, 2.6, 0.4);
      scene.add(g);
      // Black clamp block
      g.add(boxM(0.3, 0.35, 0.5, std(0x181a1d)));
      // Horizontal float cylinder
      const fsBallMat = std(0xffa818, { emissive: 0x000000 });
      const fArm = cylM(0.06, 0.6, std(0x22262d), 10);
      fArm.rotation.x = Math.PI / 2;
      fArm.position.set(-0.25, -0.4, 0);
      g.add(fArm);

      self.floatBall = add(new T.Mesh(new T.CylinderGeometry(0.18, 0.18, 0.28, 16), fsBallMat), 12.1, 0.45, 0.4);
      label('Float Switch (FSW-01)', 11.2, 3.5, 0.4, '#ffae1a');
    })();

    // =========================================================================
    // 3. SMPS (12V DC POWER SUPPLY) – Top-Left of wooden board
    // =========================================================================
    (function () {
      const g = new T.Group();
      g.position.set(-4.8, 0.38, -3.0);
      scene.add(g);

      const smpsW = 2.4, smpsH = 0.74, smpsD = 3.8;
      const cv = document.createElement('canvas');
      cv.width = 512; cv.height = 512;
      const ctx = cv.getContext('2d');
      ctx.fillStyle = '#b6bec9'; ctx.fillRect(0, 0, 512, 512);

      ctx.fillStyle = '#1c2128';
      for (let y = 30; y < 480; y += 18) {
        for (let x = 30; x < 480; x += 18) {
          ctx.beginPath(); ctx.arc(x, y, 4.5, 0, Math.PI * 2); ctx.fill();
        }
      }
      ctx.fillStyle = '#ffbe1a';
      ctx.beginPath();
      ctx.moveTo(256, 170); ctx.lineTo(330, 290); ctx.lineTo(182, 290); ctx.closePath();
      ctx.fill();
      ctx.fillStyle = '#111'; ctx.font = 'bold 36px sans-serif'; ctx.textAlign = 'center'; ctx.fillText('!', 256, 276);
      ctx.font = 'bold 22px monospace'; ctx.fillText('12V DC', 256, 330);

      const smpsTex = new T.CanvasTexture(cv);
      const smpsTopMat = std(0xc0c8d4, { map: smpsTex, metalness: 0.75, roughness: 0.35 });
      const smpsBodyMat = std(0xb2bac6, { metalness: 0.8, roughness: 0.4 });
      g.add(new T.Mesh(new T.BoxGeometry(smpsW, smpsH, smpsD),
        [smpsBodyMat, smpsBodyMat, smpsTopMat, smpsBodyMat, smpsBodyMat, smpsBodyMat]));

      const termBlock = boxM(2.1, 0.28, 0.5, std(0x222830));
      termBlock.position.set(0, -0.22, 1.8);
      g.add(termBlock);
      for (let i = 0; i < 7; i++) {
        const sc = cylM(0.06, 0.08, screwMat, 10);
        sc.position.set(-0.75 + i * 0.25, 0.15, 0);
        termBlock.add(sc);
      }

      const acCord = cylM(0.08, 1.8, std(0x181a1d), 10);
      acCord.rotation.x = Math.PI / 2;
      acCord.position.set(-0.7, -0.15, -2.6);
      g.add(acCord);

      label('12V DC SMPS', -4.8, 1.45, -3.0, '#ff6e6e');
    })();

    // =========================================================================
    // 4. SOLENOID VALVE (OV-01) – Top line between SMPS and Equal Tee
    // =========================================================================
    (function () {
      const g = new T.Group();
      g.position.set(-1.8, 0.45, -3.8);
      scene.add(g);

      const body = boxM(0.65, 0.45, 0.45, std(0xc59a2c, { metalness: 0.7, roughness: 0.3 }));
      g.add(body);

      const coil = cylM(0.24, 0.55, std(0x1a1e24), 20); coil.position.y = 0.45; g.add(coil);
      const blueCap = cylM(0.25, 0.1, std(0x1a6cd4, { roughness: 0.4 }), 20); blueCap.position.y = 0.72; g.add(blueCap);

      const auxMod = boxM(0.65, 0.15, 0.65, std(0x734822)); auxMod.position.set(0.65, -0.15, 0.2); g.add(auxMod);
      auxMod.add(boxM(0.2, 0.2, 0.2, std(0x1e6fe0))).position.y = 0.14;

      const led = sph(0.06, new T.MeshStandardMaterial({ color: 0x222222, emissive: 0x000000 }));
      led.position.set(0, 0.82, 0);
      g.add(led);
      self.ov = { led };

      label('Solenoid Valve (OV-01)', -1.8, 1.6, -3.8, '#f0a44b');
    })();

    // =========================================================================
    // 5. ESP32 CONTROL BOARD – Bottom-Left of the board
    // =========================================================================
    (function () {
      const g = new T.Group();
      g.position.set(-4.4, 0.12, 2.8);
      scene.add(g);

      const pcbW = 3.6, pcbH = 0.12, pcbD = 2.6;
      const cv = document.createElement('canvas');
      cv.width = 512; cv.height = 384;
      const ctx = cv.getContext('2d');
      ctx.fillStyle = '#1657a8'; ctx.fillRect(0, 0, 512, 384);
      ctx.strokeStyle = '#2b7ce8'; ctx.lineWidth = 2.5;
      for (let i = 0; i < 18; i++) {
        ctx.beginPath();
        ctx.moveTo(30 + i * 26, 20);
        ctx.lineTo(50 + i * 26, 120);
        ctx.lineTo(80 + i * 26, 260);
        ctx.stroke();
      }
      ctx.fillStyle = '#ffffff'; ctx.font = 'bold 22px monospace'; ctx.textAlign = 'center';
      ctx.fillText('ESP32 CONTROLLER · 12V', 256, 40);
      ctx.font = '16px monospace';
      ctx.fillText('P1  P2  P3  SOL  FLOW  FLOAT', 256, 350);

      const pcbTex = new T.CanvasTexture(cv);
      const topM = new T.MeshStandardMaterial({ map: pcbTex, roughness: 0.45 });
      const blueSideM = std(0x13488a);
      g.add(new T.Mesh(new T.BoxGeometry(pcbW, pcbH, pcbD),
        [blueSideM, blueSideM, topM, blueSideM, blueSideM, blueSideM]));

      const espBoard = boxM(1.6, 0.14, 0.9, std(0x161a20));
      espBoard.position.set(0.4, 0.12, -0.1);
      g.add(espBoard);
      const shield = boxM(0.85, 0.05, 0.65, std(0xc0c8d4, { metalness: 0.85, roughness: 0.25 }));
      shield.position.set(-0.25, 0.1, 0);
      espBoard.add(shield);

      // 4 Electrolytic filter capacitors in a row
      for (let i = 0; i < 4; i++) {
        const cp = cylM(0.12, 0.36, std(0x22262e, { metalness: 0.7 }), 14);
        cp.position.set(-1.3, 0.22, -0.9 + i * 0.55);
        g.add(cp);
      }

      // Terminal blocks
      for (let i = 0; i < 4; i++) {
        const tb = boxM(0.38, 0.26, 0.3, std(0x2774db));
        tb.position.set(-0.6 + i * 0.55, 0.16, 1.05);
        g.add(tb);
      }

      self.pcbLed = add(sph(0.07, new T.MeshStandardMaterial({ color: 0x222222, emissive: 0x000000 })), -4.8, 0.32, 2.0);
      self.pcbFloatLed = add(sph(0.07, new T.MeshStandardMaterial({ color: 0x222222, emissive: 0x000000 })), -4.4, 0.32, 2.0);
      self.pcbBuz = add(cylM(0.12, 0.16, std(0x111111), 12), -3.6, 0.28, 2.0);

      // Industrial Rocker Switch
      const sw = new T.Group(); sw.position.set(-2.0, 0.2, 2.8); scene.add(sw);
      sw.add(boxM(0.8, 0.35, 0.65, std(0x22262d)));
      const lever = boxM(0.45, 0.2, 0.35, std(0xdcdcdc));
      lever.position.set(0, 0.25, 0);
      lever.userData.pick = 'switch';
      sw.add(lever);
      self.pick.push(lever);
      self.swLever = lever;
      label('ON/OFF Switch', -2.0, 1.05, 2.8, '#ffffff');

      label('ESP32 Control Board', -4.4, 1.45, 2.8, '#44d67c');
    })();

    // =========================================================================
    // 6. 3x PUMP DRIVERS (middle vertical column)
    // =========================================================================
    function makeDriver(xPos, zPos, tag) {
      const g = new T.Group();
      g.position.set(xPos, 0.12, zPos);
      scene.add(g);
      g.add(boxM(0.9, 0.1, 0.9, std(0x1a5fc4)));
      const tb1 = boxM(0.24, 0.22, 0.7, std(0x2b85e6)); tb1.position.x = -0.32; g.add(tb1);
      const tb2 = boxM(0.24, 0.22, 0.7, std(0x2b85e6)); tb2.position.x = 0.32; g.add(tb2);
      g.add(boxM(0.35, 0.22, 0.35, std(0x1e242d))).position.y = 0.14;
      label(tag, xPos, 0.85, zPos, '#68b4ff');
    }
    makeDriver(-0.8, -1.8, 'Pump Driver 1');
    makeDriver(-0.8, 0.5, 'Pump Driver 3');
    makeDriver(-0.8, 2.8, 'Pump Driver 2');

    // =========================================================================
    // 7. DIAPHRAGM PUMPS (Verified Physical Orientations from Photo)
    // =========================================================================
    // Builds a 12V diaphragm pump matching the teal/cyan units in the lab:
    // isVertical: if true, pump axis runs along Z; if false, runs along X
    // headEndPositive: if true, pump head is at +axis; if false, head is at -axis
    function createDiaphragmPump(x, z, isVertical, headEndPositive, tag) {
      const g = new T.Group();
      g.position.set(x, TY, z);
      scene.add(g);

      const baseMat = std(0x028c98, { roughness: 0.45, metalness: 0.1 });
      const rubberMat = std(0x1c2024, { roughness: 0.9 });
      const motorMat = std(0x029aa8, { roughness: 0.4, metalness: 0.25 });
      const headMat = std(0x768291, { roughness: 0.5, metalness: 0.15 });
      const cyanCoverMat = std(0x00a8b5, { roughness: 0.35 });
      const pushFitMat = std(0x1b2027, { roughness: 0.4 });
      const pushCollarMat = std(0x0d72d6, { roughness: 0.5 });

      // Pump local orientation container
      const pBody = new T.Group();
      g.add(pBody);
      if (isVertical) {
        pBody.rotation.y = headEndPositive ? Math.PI : 0;
      } else {
        pBody.rotation.y = headEndPositive ? -Math.PI / 2 : Math.PI / 2;
      }

      // Teal mounting base with 4 rubber vibration dampers
      const bracket = boxM(1.6, 0.08, 0.9, baseMat);
      bracket.position.y = -TY + 0.04;
      pBody.add(bracket);
      [[-0.65, -0.38], [0.65, -0.38], [-0.65, 0.38], [0.65, 0.38]].forEach(([fx, fz]) => {
        const foot = cylM(0.12, 0.1, rubberMat, 12);
        foot.position.set(fx, -TY + 0.05, fz);
        pBody.add(foot);
      });

      // Cylindrical DC electric motor body
      const motor = cylM(0.32, 1.15, motorMat, 24);
      motor.rotation.z = Math.PI / 2;
      motor.position.x = 0.35;
      pBody.add(motor);

      // Motor specification label band
      const labelBand = cylM(0.325, 0.42, std(0xf2f4f8, { roughness: 0.5 }), 24);
      labelBand.rotation.z = Math.PI / 2;
      labelBand.position.x = 0.35;
      pBody.add(labelBand);

      // Rear motor end-cap
      const rearCap = cylM(0.33, 0.12, std(0x1c2228), 20);
      rearCap.rotation.z = Math.PI / 2;
      rearCap.position.x = 0.95;
      pBody.add(rearCap);

      // Grey diaphragm pump head housing
      const head = boxM(0.68, 0.65, 0.65, headMat);
      head.position.x = -0.42;
      pBody.add(head);

      // Cyan front plate with reinforcement ribs
      const cPlate = boxM(0.08, 0.55, 0.55, cyanCoverMat);
      cPlate.position.set(-0.42, 0.33, 0);
      pBody.add(cPlate);

      // Front pressure switch nose
      const pCap = boxM(0.35, 0.42, 0.42, std(0x56606d));
      pCap.position.x = -0.78;
      pBody.add(pCap);

      // Push-fit tube connectors on lateral ports
      [-0.42, 0.42].forEach(pz => {
        const fit = cylM(0.12, 0.32, pushFitMat, 14);
        fit.position.set(-0.42, 0, pz);
        fit.rotation.x = Math.PI / 2;
        pBody.add(fit);
        const col = cylM(0.13, 0.08, pushCollarMat, 14);
        col.position.set(-0.42, 0, pz + (pz > 0 ? 0.14 : -0.14));
        col.rotation.x = Math.PI / 2;
        pBody.add(col);
      });

      // Operational status LED
      const led = sph(0.065, new T.MeshStandardMaterial({ color: 0x222222, emissive: 0x000000 }));
      led.position.set(0, 0.42, 0);
      pBody.add(led);

      label(tag, x, 1.85, z, '#4ee68f');
      return { g, led, x, z, baseTY: TY };
    }

    // Pump-01 (Top-Right): Mounted VERTICALLY (head at -Z, motor pointing +Z towards center)
    this.p1 = createDiaphragmPump(5.4, -3.2, true, false, 'Pump-01');

    // Pump-03 (Center / Flush Pump): Mounted HORIZONTALLY (head at -X, motor pointing +X to right)
    this.p3 = createDiaphragmPump(2.2, -0.4, false, false, 'Pump-03');

    // Pump-02 (Bottom-Right / Recirculation Pump): Mounted VERTICALLY (motor at -Z, head at +Z)
    this.p2 = createDiaphragmPump(5.8, 3.8, true, true, 'Pump-02');

    // =========================================================================
    // 8. VALVES, EQUAL TEE & FLOW TRANSMITTER
    // =========================================================================

    // Equal Tee on the top line
    (function () {
      const g = new T.Group();
      g.position.set(0.6, TY, -3.8);
      scene.add(g);
      const m = std(0x1e242c, { roughness: 0.4 });
      const hC = cylM(0.13, 0.65, m, 14); hC.rotation.z = Math.PI / 2; g.add(hC);
      const vC = cylM(0.13, 0.45, m, 14); vC.rotation.x = Math.PI / 2; vC.position.z = 0.22; g.add(vC);
      label('Equal Tee', 0.6, 1.35, -3.8, '#c9d3e3');
    })();

    // Red-handled push-fit ball valve
    function redBallValve(x, z, tag, pickId) {
      const g = new T.Group();
      g.position.set(x, TY, z);
      scene.add(g);

      const body = cylM(0.16, 0.65, std(0xf0f3f8, { roughness: 0.4 }), 18);
      body.rotation.z = Math.PI / 2;
      g.add(body);
      const midRing = cylM(0.22, 0.22, std(0x28303a), 18);
      midRing.rotation.z = Math.PI / 2;
      g.add(midRing);

      const stem = cylM(0.05, 0.22, std(0x8892a0), 10);
      stem.position.y = 0.2;
      g.add(stem);

      const handle = new T.Group();
      handle.position.y = 0.32;
      g.add(handle);
      const redLever = boxM(0.72, 0.08, 0.16, std(0xe02828, { roughness: 0.35 }));
      redLever.position.x = 0.26;
      handle.add(redLever);

      if (pickId) {
        redLever.userData.pick = pickId;
        self.pick.push(redLever);
      }
      label(tag, x, 1.45, z + 0.6, '#f0a44b');
      return { g, handle };
    }

    // BV-01: Manual flush valve on line between Flush Tank and Center Pump (Pump-03)
    this.bv1 = redBallValve(-2.6, 4.4, 'Ball Valve (BV-01)', 'bv1');

    // BV-02: Flow throttle ball valve on bottom recirculation line
    this.bv2 = redBallValve(-0.6, 5.2, 'Ball Valve (BV-02)', 'bv2');

    // BV-03: Reference isolation valve
    this.bv3 = redBallValve(3.8, -3.8, 'Ball Valve (BV-03)', null);

    // White FT-01 Turbine Flow Transmitter (inline on bottom recirculation line)
    (function () {
      const g = new T.Group();
      g.position.set(1.4, TY, 5.0);
      scene.add(g);

      const bodyMat = std(0xf4f7fa, { roughness: 0.35 });
      const mainChamber = cylM(0.28, 0.46, bodyMat, 20);
      g.add(mainChamber);

      const portL = cylM(0.15, 0.32, bodyMat, 16); portL.rotation.z = Math.PI / 2; portL.position.x = -0.32; g.add(portL);
      const portR = cylM(0.15, 0.32, bodyMat, 16); portR.rotation.z = Math.PI / 2; portR.position.x = 0.32; g.add(portR);

      const rot = new T.Group();
      g.add(rot);
      for (let i = 0; i < 4; i++) {
        const blade = boxM(0.38, 0.04, 0.08, std(0x1e78e6));
        blade.rotation.y = i * Math.PI / 4;
        rot.add(blade);
      }

      const hallCap = boxM(0.2, 0.12, 0.18, std(0x1a1a1a));
      hallCap.position.y = 0.28;
      g.add(hallCap);

      const wireRed = cylM(0.02, 0.4, std(0xe02828), 6); wireRed.position.set(-0.04, 0.38, 0); g.add(wireRed);
      const wireBlk = cylM(0.02, 0.4, std(0x111111), 6); wireBlk.position.set(0, 0.38, 0); g.add(wireBlk);
      const wireYel = cylM(0.02, 0.4, std(0xffd020), 6); wireYel.position.set(0.04, 0.38, 0); g.add(wireYel);

      const led = sph(0.06, new T.MeshStandardMaterial({ color: 0x222222, emissive: 0x000000 }));
      led.position.set(0.18, 0.34, 0);
      g.add(led);

      self.ft = { rot, led, hall: hallCap };
      label('Flow Transmitter (FT-01)', 1.4, 1.55, 5.0, '#ffd24a');
    })();

    // External Fluid Jug (for Case 1 external liquid charge test)
    (function () {
      const g = new T.Group();
      g.position.set(9.0, 2.2, -3.2);
      scene.add(g);
      const jug = cylM(0.42, 1.0, new T.MeshPhysicalMaterial({ color: 0xffe2b8, transparent: true, opacity: 0.55 }), 20);
      g.add(jug);
      jug.userData.pick = 'inject';
      self.pick.push(jug);

      const stream = cylM(0.05, 1.4, new T.MeshBasicMaterial({ color: 0x5cb5ff }), 8);
      stream.position.set(0.8, -0.7, 1.0);
      stream.visible = false;
      scene.add(stream);

      self.jug = { g, stream };
      label('External Fluid', 9.0, 3.4, -3.2, '#ffae1a');
    })();

    // =========================================================================
    // 9. HYDRAULIC TUBING & ANIMATED FLOW PARTICLE STREAMS
    // =========================================================================
    function roundPath(pts, rad) {
      const P = pts.map(p => new T.Vector3(p[0], p[1], p[2])), out = [P[0].clone()];
      for (let i = 1; i < P.length - 1; i++) {
        const a = P[i - 1], b = P[i], c = P[i + 1], d1 = b.clone().sub(a), d2 = c.clone().sub(b);
        const r = Math.min(rad, d1.length() / 2, d2.length() / 2);
        const p1 = b.clone().sub(d1.clone().normalize().multiplyScalar(r)), p2 = b.clone().add(d2.clone().normalize().multiplyScalar(r));
        new T.QuadraticBezierCurve3(p1, b, p2).getPoints(10).forEach(q => out.push(q));
      }
      out.push(P[P.length - 1].clone()); return out;
    }

    const collarMat = std(0x1c232c, { roughness: 0.4 });
    function collar(p, dir) {
      const c = cylM(0.12, 0.24, collarMat, 14);
      c.position.copy(p);
      c.quaternion.setFromUnitVectors(new T.Vector3(0, 1, 0), dir.clone().normalize());
      scene.add(c);
    }

    function Flow(path, color, radius) {
      const pts = roundPath(path, 0.45);
      const cum = [0];
      for (let i = 1; i < pts.length; i++) cum.push(cum[i - 1] + pts[i].distanceTo(pts[i - 1]));
      this.len = cum[cum.length - 1];
      this.pts = pts;
      this.cum = cum;

      const curve = new T.CurvePath();
      for (let i = 1; i < pts.length; i++) curve.add(new T.LineCurve3(pts[i - 1], pts[i]));
      const tube = new T.Mesh(new T.TubeGeometry(curve, Math.max(20, pts.length * 3), radius || 0.08, 12, false),
        new T.MeshStandardMaterial({ color: 0x3d94e8, transparent: true, opacity: 0.48, roughness: 0.2, metalness: 0.05 }));
      scene.add(tube);
      collar(pts[0], pts[1].clone().sub(pts[0]));
      collar(pts[pts.length - 1], pts[pts.length - 1].clone().sub(pts[pts.length - 2]));

      this.n = Math.max(6, Math.round(this.len / 0.45));
      this.u = new Float32Array(this.n);
      for (let i = 0; i < this.n; i++) this.u[i] = i / this.n;
      this.geo = new T.BufferGeometry();
      this.pos = new Float32Array(this.n * 3);
      this.geo.setAttribute('position', new T.BufferAttribute(this.pos, 3));
      this.mat = new T.PointsMaterial({ color: color, size: 0.22, transparent: true, opacity: 0.95, depthWrite: false });
      this.points = new T.Points(this.geo, this.mat);
      this.points.frustumCulled = false;
      scene.add(this.points);
      this.tube = tube;
    }

    Flow.prototype.update = function (q, dt, color) {
      this.points.visible = q > 0.03;
      if (color !== undefined) this.mat.color.copy(color);
      if (!this.points.visible) return;
      const adv = (0.28 + q * 0.58) * dt / this.len;
      for (let i = 0; i < this.n; i++) {
        this.u[i] = (this.u[i] + adv) % 1;
        const d = this.u[i] * this.len;
        let k = 1;
        while (k < this.cum.length - 1 && this.cum[k] < d) k++;
        const f = (d - this.cum[k - 1]) / (this.cum[k] - this.cum[k - 1] || 1);
        const a = this.pts[k - 1], b = this.pts[k];
        this.pos[i * 3] = a.x + (b.x - a.x) * f;
        this.pos[i * 3 + 1] = a.y + (b.y - a.y) * f;
        this.pos[i * 3 + 2] = a.z + (b.z - a.z) * f;
      }
      this.geo.attributes.position.needsUpdate = true;
    };

    const cProc = new T.Color(0x288bf5), cFlush = new T.Color(0xa2f0ff);
    this.cProc = cProc;
    this.cFlush = cFlush;
    this.mix = new T.Color();

    // Exact tubing trajectories matching physical routing:
    this.flows = {
      // Line A: Process Tank (T-01) -> Solenoid (OV-01) -> Equal Tee
      A: new Flow([[-7.9, TY + 0.35, -3.2], [-5.5, TY, -3.8], [-2.2, TY, -3.8], [0.6, TY, -3.8]], 0x288bf5),

      // Line B: Equal Tee -> Pump-01 inlet (left) -> Pump-01 outlet (right) -> Sump Tank
      B: new Flow([[0.6, TY, -3.8], [4.4, TY, -3.8], [5.0, TY, -3.6], [5.8, TY, -3.6], [8.6, TY + 0.4, -2.4]], 0x288bf5),

      // Line C: Flush Tank (F-01) -> BV-01 -> Center Pump-03 bottom inlet -> top outlet -> Equal Tee
      C: new Flow([[-7.9, TY + 0.35, 3.4], [-5.0, TY, 4.4], [-2.6, TY, 4.4], [-0.5, TY, 4.4], [1.7, TY, 0.1], [1.7, TY, -0.9], [0.6, TY, -3.8]], 0xa2f0ff),

      // Line D: Sump Tank SMP-01 -> Pump-02 right inlet -> Pump-02 left outlet
      D: new Flow([[9.2, TY + 0.1, 0.4], [7.8, TY, 2.5], [6.4, TY, 4.2], [5.2, TY, 4.2]], 0x288bf5),

      // Line E: Pump-02 outlet -> FT-01 -> BV-02 -> bottom return line to Process Tank
      E: new Flow([[5.2, TY, 4.2], [1.8, TY, 5.0], [0.8, TY, 5.0], [-0.6, TY, 5.2], [-7.6, TY, 5.2], [-11.8, TY + 0.4, 0.0], [-9.8, TY + 0.6, -3.2]], 0x288bf5)
    };

    // =========================================================================
    // 10. REALISTIC MULTI-COLOR WIRING HARNESSES
    // =========================================================================
    function wire(a, b, col) {
      const A = new T.Vector3(a[0], a[1], a[2]);
      const B = new T.Vector3(b[0], b[1], b[2]);
      const M = A.clone().add(B).multiplyScalar(0.5);
      M.y = 0.08;
      const cv = new T.QuadraticBezierCurve3(A, M, B);
      const m = new T.Mesh(new T.TubeGeometry(cv, 24, 0.026, 6, false), std(col, { roughness: 0.6 }));
      scene.add(m);
    }
    // SMPS 12V bus to ESP32 board
    wire([-4.8, 0.3, -1.0], [-4.4, 0.25, 1.6], 0xe02828);
    wire([-4.6, 0.3, -1.0], [-4.2, 0.25, 1.6], 0x161616);

    // Control ribbon from ESP32 to Pump Drivers
    wire([-3.0, 0.25, 2.4], [-1.2, 0.2, -1.8], 0x4488ff);
    wire([-3.0, 0.25, 2.5], [-1.2, 0.2, 0.5], 0x4488ff);
    wire([-3.0, 0.25, 2.6], [-1.2, 0.2, 2.8], 0x4488ff);

    // Driver power outputs to Pumps 1, 2, 3
    wire([-0.4, 0.2, -1.8], [4.8, 0.35, -2.6], 0x36d66e);
    wire([-0.4, 0.2, 0.5], [2.8, 0.35, -0.4], 0x36d66e);
    wire([-0.4, 0.2, 2.8], [5.4, 0.35, 3.2], 0x36d66e);

    // Solenoid wire to controller
    wire([-3.4, 0.25, 1.8], [-1.8, 0.35, -3.4], 0xff8833);

    // FT-01 3-wire signal lead to ESP32 board
    wire([1.4, 0.45, 4.8], [-3.2, 0.25, 3.2], 0xffc42e);

    // Float switch cable from Sump Tank to ESP32 board
    wire([12.4, 2.4, 0.4], [-3.0, 0.25, 3.4], 0x161616);

    // =========================================================================
    // 11. ORBIT CAMERA & VIEW PRESETS
    // =========================================================================
    const cam = { th: 0.42, ph: 1.05, r: 21, tx: 0.2, ty: 0.8, tz: 0.2 };
    this.cam = cam;

    function applyCam() {
      camera.position.set(
        cam.tx + cam.r * Math.sin(cam.ph) * Math.sin(cam.th),
        cam.ty + cam.r * Math.cos(cam.ph),
        cam.tz + cam.r * Math.sin(cam.ph) * Math.cos(cam.th)
      );
      camera.lookAt(cam.tx, cam.ty, cam.tz);
    }
    applyCam();

    this.views = {
      iso:     { th: 0.45, ph: 1.02, r: 21.5, tx: 0.2, ty: 0.8, tz: 0.2 },
      front:   { th: 0.02, ph: 1.12, r: 20.0, tx: 0.2, ty: 1.0, tz: 0.2 },
      top:     { th: 0.00, ph: 0.02, r: 22.0, tx: 0.2, ty: 0.0, tz: 0.2 },
      sump:    { th: 0.65, ph: 1.08, r: 8.5,  tx: 10.2, ty: 0.8, tz: -1.2 },
      flush:   { th: -0.45, ph: 1.06, r: 12.0, tx: -4.5, ty: 0.8, tz: 3.6 },
      ft:      { th: 0.15, ph: 0.95, r: 5.5,  tx: 1.4,  ty: 0.8, tz: 4.8 },
      control: { th: -0.15, ph: 0.92, r: 10.5, tx: -3.8, ty: 0.6, tz: 1.2 }
    };

    this.setView = function (name) {
      const v = self.views[name];
      if (!v) return;
      self._tw = { from: Object.assign({}, cam), to: v, t: 0 };
    };

    const dom = renderer.domElement;
    let drag = null;
    dom.addEventListener('contextmenu', e => e.preventDefault());
    dom.addEventListener('pointerdown', e => {
      dom.setPointerCapture(e.pointerId);
      drag = { x: e.clientX, y: e.clientY, sx: e.clientX, sy: e.clientY, pan: e.button === 2 || e.shiftKey };
      self._tw = null;
    });
    dom.addEventListener('pointermove', e => {
      if (!drag) return;
      const dx = e.clientX - drag.x, dy = e.clientY - drag.y;
      drag.x = e.clientX; drag.y = e.clientY;
      if (drag.pan) {
        const k = cam.r * 0.0016;
        cam.tx -= Math.cos(cam.th) * dx * k;
        cam.tz += Math.sin(cam.th) * dx * k;
        cam.ty += dy * k;
      } else {
        cam.th -= dx * 0.0055;
        cam.ph = Math.max(0.04, Math.min(1.48, cam.ph - dy * 0.0055));
      }
      applyCam();
    });
    dom.addEventListener('pointerup', e => {
      if (drag && Math.hypot(e.clientX - drag.sx, e.clientY - drag.sy) < 5) self._click(e);
      drag = null;
    });
    dom.addEventListener('wheel', e => {
      e.preventDefault();
      cam.r = Math.max(4, Math.min(65, cam.r * (1 + Math.sign(e.deltaY) * 0.08)));
      applyCam();
    }, { passive: false });

    const ray = new T.Raycaster(), mv = new T.Vector2();
    this.onPick = function () {};
    this._click = function (e) {
      const rc = dom.getBoundingClientRect();
      mv.set((e.clientX - rc.left) / rc.width * 2 - 1, -((e.clientY - rc.top) / rc.height) * 2 + 1);
      ray.setFromCamera(mv, camera);
      const hit = ray.intersectObjects(self.pick, false)[0];
      if (hit) self.onPick(hit.object.userData.pick);
    };

    this.resize = function () {
      const w = host.clientWidth, h = host.clientHeight;
      if (!w || !h) return;
      renderer.setSize(w, h);
      camera.aspect = w / h;
      camera.updateProjectionMatrix();
    };
    new ResizeObserver(() => this.resize()).observe(host);

    this.showLabels = function (on) {
      self.labels.forEach(l => l.visible = on);
    };

    scene.traverse(o => {
      if (o.isMesh) {
        o.receiveShadow = true;
        o.castShadow = !o.material.transparent && o.geometry.type !== 'PlaneGeometry';
      }
    });
  }

  const setLed = (m, on, col) => {
    m.material.color.set(on ? col : 0x222222);
    m.material.emissive.set(on ? col : 0x000000);
    m.material.emissiveIntensity = on ? 1.4 : 0;
  };

  // =========================================================================
  // 12. UPDATE LOOP – Synchronizes 3D digital twin with simulation state
  // =========================================================================
  Scene3D.prototype.update = function (sys, dt, blink) {
    const p = sys.plant, c = sys.ctrl, self = this;

    // Smooth camera tween
    if (this._tw) {
      const tw = this._tw;
      tw.t = Math.min(1, tw.t + dt / 0.85);
      const e = tw.t * tw.t * (3 - 2 * tw.t);
      ['th', 'ph', 'r', 'tx', 'ty', 'tz'].forEach(k => this.cam[k] = tw.from[k] + (tw.to[k] - tw.from[k]) * e);
      if (tw.t >= 1) this._tw = null;
      const cam = this.cam;
      this.camera.position.set(
        cam.tx + cam.r * Math.sin(cam.ph) * Math.sin(cam.th),
        cam.ty + cam.r * Math.cos(cam.ph),
        cam.tz + cam.r * Math.sin(cam.ph) * Math.cos(cam.th)
      );
      this.camera.lookAt(cam.tx, cam.ty, cam.tz);
    }

    // Dynamic fluid levels in rectangular storage tubs
    const updateTub = (v, lit, conc) => {
      const hh = Math.max(0.01, Math.min(v.h - 0.1, lit / v.area * 1.6));
      v.water.scale.y = hh;
      v.water.position.y = hh / 2;
      v.water.visible = lit > 0.005;
      v.surf.visible = lit > 0.005;
      v.surf.position.y = hh + 0.002;
      if (conc != null) {
        v.wmat.color.copy(self.cFlush).lerp(self.cProc, conc);
      }
    };
    updateTub(this.tank, p.tank, p.cTank);
    updateTub(this.sump, p.sump, p.cSump);
    updateTub(this.fl, p.flush, null);

    // Float switch sensor inside Sump Tank
    const sumpWaterH = Math.max(0.01, Math.min(this.sump.h - 0.1, p.sump / this.sump.area * 1.6));
    this.floatBall.position.y = Math.max(0.25, Math.min(1.8, sumpWaterH));
    this.floatBall.material.emissive.set(p.floatOn ? 0xff2200 : 0x000000);
    this.floatBall.material.color.set(p.floatOn ? 0xff3b14 : 0xffa818);

    // Animated particle flows
    const cIn = p.q1 > 1e-6 ? Math.min(1, p.qTank * p.cTank / p.q1) : 0;
    this.flows.A.update(p.qTank, dt, this.cProc);
    this.flows.B.update(p.q1, dt, this.cProc);
    this.flows.C.update(p.q3, dt, this.cFlush);
    this.flows.D.update(p.q2, dt, this.mix.copy(this.cFlush).lerp(this.cProc, p.cSump));
    this.flows.E.update(p.q2, dt, this.mix.copy(this.cFlush).lerp(this.cProc, p.cSump));

    // Pumps mechanical vibration & activity LEDs
    [
      [this.p1, p.q1 > 0.02 ? p.q1 / 6 : 0, p.duty[0] > 0],
      [this.p3, p.q3 > 0.02 ? p.q3 / 6 : 0, p.duty[2] > 0],
      [this.p2, p.q2 > 0.02 ? p.q2 / 6 : 0, p.duty[1] > 0]
    ].forEach(([pp, fr, on]) => {
      setLed(pp.led, on, 0x36e06a);
      const vib = on ? 0.007 * (0.5 + Math.max(fr, 0.2)) : 0;
      pp.g.position.y = pp.baseTY + vib * Math.sin(performance.now() * 0.09);
      pp.g.rotation.z = vib * 0.5 * Math.sin(performance.now() * 0.075);
    });

    // Valves
    this.bv1.handle.rotation.y = p.bv1Open ? 0 : -Math.PI / 2;
    this.bv2.handle.rotation.y = -(1 - p.bv2) * Math.PI / 2;
    this.bv3.handle.rotation.y = p.bv3Open ? 0 : -Math.PI / 2;
    setLed(this.ov.led, p.ov, 0x36e06a);

    // FT-01 Turbine flow sensor rotation & pulse LED
    this.ft.rot.rotation.y += dt * Math.min(p.freq * 0.9, 14);
    const near = p.freq > 0.3 && Math.cos(this.ft.rot.rotation.y) > 0.92;
    setLed(this.ft.led, near, 0x4ac2ff);

    // External fluid injection animation
    this.jug.stream.visible = p.injectQueue > 0.001;

    // ESP32 Control Board indicators
    setLed(this.pcbLed,
      (c.state !== 'OFF' && c.state !== 'FAULT') || (c.state === 'FAULT' && blink),
      c.state === 'FAULT' ? 0xff3030 : c.state === 'DONE' ? 0x4ac2ff : 0x36e06a
    );
    setLed(this.pcbFloatLed, p.floatOn, 0xff3030);
    this.pcbBuz.material.color.set(c.buzzer && blink ? 0xff3030 : 0x111111);
    this.swLever.rotation.z = c.sw ? -0.5 : 0.5;
    this.swLever.material.color.set(c.sw ? 0x36e06a : 0xdcdcdc);

    this.renderer.render(this.scene, this.camera);
  };

  root.Scene3D = Scene3D;
})(window);
