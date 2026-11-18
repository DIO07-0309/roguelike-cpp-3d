// P5 rubble asset gate — `node tools/check_p5_assets.js`
// New assets (no "before" baseline): face floor + absolute bbox bounds.
// Validates 3 terrain_rubble_<biome> JSONs:
// schema / name / finite(pos+color) / faces>=300 / bbox within x/z±16, y∈[0,6].
const fs = require('fs');
const path = require('path');

const names = ['terrain_rubble_prison', 'terrain_rubble_volcano', 'terrain_rubble_void'];
const dir = 'assets/models';
const FACE_MIN = 300;
const XZ = 16.0;   // 1 tile = 32, half-extent 16
const Y_MAX = 6.0; // rubble sits on top of slab (placed by C++ at y≈5)
let failed = 0;

for (const name of names) {
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
  const bb = verts.length ? {
    xmin: Math.min(...verts.map(v => v[0])), xmax: Math.max(...verts.map(v => v[0])),
    ymin: Math.min(...verts.map(v => v[1])), ymax: Math.max(...verts.map(v => v[1])),
    zmin: Math.min(...verts.map(v => v[2])), zmax: Math.max(...verts.map(v => v[2])),
  } : null;
  const checks = {
    schema: keys === 'faces,name,vertices',
    name: d.name === name,
    finite: verts.length > 0 && badVertex === -1,
    faces: faces >= FACE_MIN,
    bbox: bb !== null &&
      bb.xmin >= -XZ && bb.xmax <= XZ && bb.zmin >= -XZ && bb.zmax <= XZ &&
      bb.ymin >= -0.01 && bb.ymax <= Y_MAX + 0.01,
  };
  const ok = Object.values(checks).every(Boolean);
  if (!ok) failed++;
  console.log(`${ok ? 'PASS' : 'FAIL'} ${name} faces=${faces}/${FACE_MIN} ${JSON.stringify(checks)}`);
}
console.log(`ASSETS=${names.length} FAILED=${failed}`);
process.exit(failed === 0 ? 0 : 1);
