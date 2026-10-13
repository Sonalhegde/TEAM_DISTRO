require('../../simulation/web/sim.js'); const T = require('./tests.js');
const res = T.runAll(); let f = 0;
res.forEach(r => { if (!r.pass) f++; console.log((r.pass ? 'PASS ' : 'FAIL ') + r.id + ' ' + r.req + '\n     ' + r.detail); });
console.log(`\n${res.length - f}/${res.length} passed`); process.exit(f ? 1 : 0);
