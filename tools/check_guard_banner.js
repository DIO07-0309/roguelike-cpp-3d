// guard_banner asset gate — `node tools/check_guard_banner.js`
// schema / name / finite(pos+color) / faces>=1000 / bbox<=baseline (tol 0.01).
const fs = require('fs');
const b = require('./guard_banner_baseline.json').guard_banner;
const TOL = 0.01;
const file = 'assets/models/guard_banner.json';
let d;
try {
  d = JSON.parse(fs.readFileSync(file, 'utf8'));
} catch (e) {
  console.log(`FAIL guard_banner: parse ${e.message}`);
  process.exit(1);
}
const keys = Object.keys(d).sort().join(',');
const verts = Array.isArray(d.vertices) ? d.vertices : [];
const badVertex = verts.findIndex(v =>
  !Array.isArray(v) || v.length !== 6 || v.some(x => typeof x !== 'number' || !Number.isFinite(x)));
const faces = Array.isArray(d.faces) ? d.faces.length : -1;
const bb = verts.length ? {
  xmin: Math.min(...verts.map(v => v[0])), xmax: Math.max(...verts.map(v => v[0])),
  ymin: Math.min(...verts.map(v => v[1])), ymax: Math.max(...verts.map(v => v[1])),
  zmin: Math.min(...verts.map(v => v[2])), zmax: Math.max(...verts.map(v => v[2])),
} : null;
const checks = {
  schema: keys === 'faces,name,vertices',
  name: d.name === 'guard_banner',
  finite: verts.length > 0 && badVertex === -1,
  faces: faces >= 1000,
  bbox: bb !== null &&
    bb.xmin >= b.bbox.xmin - TOL && bb.xmax <= b.bbox.xmax + TOL &&
    bb.ymin >= b.bbox.ymin - TOL && bb.ymax <= b.bbox.ymax + TOL &&
    bb.zmin >= b.bbox.zmin - TOL && bb.zmax <= b.bbox.zmax + TOL,
};
const ok = Object.values(checks).every(Boolean);
console.log(`${ok ? 'PASS' : 'FAIL'} guard_banner faces=${faces}/1000 ${JSON.stringify(checks)}`);
process.exit(ok ? 0 : 1);
