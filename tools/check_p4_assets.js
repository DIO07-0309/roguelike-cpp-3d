// P4 asset gate — `node tools/check_p4_assets.js`
// Validates 19 disk terrain JSONs (3 biomes × floor/stairs/door + wall variants):
// schema / name / finite(pos+color) / faces strictly > baseline / bbox <= baseline (tol 0.01).
const fs = require('fs');
const path = require('path');

const baselines = require('./p4_baselines.json');
const dir = 'assets/models';
const TOL = 0.01;
let failed = 0;

for (const name of Object.keys(baselines)) {
  const file = path.join(dir, `${name}.json`);
  let d;
  try {
    d = JSON.parse(fs.readFileSync(file, 'utf8'));
  } catch (e) {
    console.log(`FAIL ${name}: parse ${e.message}`);
    failed++;
    continue;
  }
  const keys = Object.keys(d).sort().join(',');
  const verts = Array.isArray(d.vertices) ? d.vertices : [];
  const badVertex = verts.findIndex(v =>
    !Array.isArray(v) || v.length !== 6 ||
    v.some(x => typeof x !== 'number' || !Number.isFinite(x)));
  const faces = Array.isArray(d.faces) ? d.faces.length : -1;
  const b = baselines[name].bbox;
  const bb = verts.length ? {
    xmin: Math.min(...verts.map(v => v[0])), xmax: Math.max(...verts.map(v => v[0])),
    ymin: Math.min(...verts.map(v => v[1])), ymax: Math.max(...verts.map(v => v[1])),
    zmin: Math.min(...verts.map(v => v[2])), zmax: Math.max(...verts.map(v => v[2])),
  } : null;
  const checks = {
    schema: keys === 'faces,name,vertices',
    name: d.name === name,
    finite: verts.length > 0 && badVertex === -1,
    facesGrew: faces > baselines[name].faces,
    bbox: bb !== null &&
      bb.xmin >= b.xmin - TOL && bb.xmax <= b.xmax + TOL &&
      bb.ymin >= b.ymin - TOL && bb.ymax <= b.ymax + TOL &&
      bb.zmin >= b.zmin - TOL && bb.zmax <= b.zmax + TOL,
  };
  const ok = Object.values(checks).every(Boolean);
  if (!ok) failed++;
  console.log(`${ok ? 'PASS' : 'FAIL'} ${name} faces=${faces}/${baselines[name].faces} ${JSON.stringify(checks)}`);
}
const total = Object.keys(baselines).length;
console.log(`ASSETS=${total} FAILED=${failed}`);
process.exit(failed === 0 ? 0 : 1);
