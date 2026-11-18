// P3 item asset gate — `node tools/check_ground_item_assets_p3.js`
// Validates the 8 refined general items: schema / name / finite / face>=1000 / bbox<=baseline.
const fs = require('fs');
const path = require('path');

const baselines = require('./ground_item_baselines_p3.json');
const dir = 'assets/models';
const TOL = 0.01;
let failed = 0;

for (const name of Object.keys(baselines)) {
  const file = path.join(dir, `${name}.json`);
  let d;
  try { d = JSON.parse(fs.readFileSync(file, 'utf8')); }
  catch (e) { console.log(`FAIL ${name}: parse ${e.message}`); failed++; continue; }
  const keys = Object.keys(d).sort().join(',');
  const bad = d.vertices.findIndex(v =>
    !Array.isArray(v) || v.length !== 6 ||
    v.slice(0, 3).some(x => typeof x !== 'number' || !Number.isFinite(x)));
  const faces = Array.isArray(d.faces) ? d.faces.length : -1;
  const b = baselines[name].bbox;
  const bb = {
    xmin: Math.min(...d.vertices.map(v => v[0])), xmax: Math.max(...d.vertices.map(v => v[0])),
    ymin: Math.min(...d.vertices.map(v => v[1])), ymax: Math.max(...d.vertices.map(v => v[1])),
    zmin: Math.min(...d.vertices.map(v => v[2])), zmax: Math.max(...d.vertices.map(v => v[2])),
  };
  const checks = {
    schema: keys === 'faces,name,vertices',
    name: d.name === name,
    finite: bad === -1,
    faces1000: faces >= 1000,
    bbox: bb.xmin >= b.xmin - TOL && bb.xmax <= b.xmax + TOL &&
          bb.ymin >= b.ymin - TOL && bb.ymax <= b.ymax + TOL &&
          bb.zmin >= b.zmin - TOL && bb.zmax <= b.zmax + TOL,
  };
  const ok = Object.values(checks).every(Boolean);
  if (!ok) failed++;
  console.log(`${ok ? 'PASS' : 'FAIL'} ${name} faces=${faces}/1000 ${JSON.stringify(checks)}`);
}
console.log(`ITEMS=${Object.keys(baselines).length} FAILED=${failed}`);
process.exit(failed === 0 ? 0 : 1);
