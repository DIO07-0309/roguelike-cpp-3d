// Refined-assets gate — `node tools/check_refined_assets.js`
// Locks 61 already-refined assets (19 room / 30 boss / 6 player_part /
// 4 npc / 2 guard_status): schema / name / finite(pos+color) /
// faces >= locked baseline / bbox <= locked baseline / axis extent >= 50% baseline
// (tol from _meta). Extent floor blocks collapse-type regression; shrink is still allowed.
// Baseline = current on-disk state; further refinement must update it explicitly.
const fs = require('fs');
const path = require('path');

const baselines = require('./refined_asset_baselines.json');
const TOL = baselines._meta.tolerance;
const DIR = 'assets/models';

function checkAsset(name, base) {
  let data;
  try {
    data = JSON.parse(fs.readFileSync(path.join(DIR, `${name}.json`), 'utf8'));
  } catch (e) {
    return { ok: false, faces: -1, checks: { parse: e.message } };
  }
  if (!data || typeof data !== 'object' || Array.isArray(data)) {
    return { ok: false, faces: -1, checks: { schema: false } };
  }
  const keys = Object.keys(data).sort().join(',');
  const verts = Array.isArray(data.vertices) ? data.vertices : [];
  const badVertex = verts.findIndex(v =>
    !Array.isArray(v) || v.length !== 6 ||
    v.some(x => typeof x !== 'number' || !Number.isFinite(x)));
  const faces = Array.isArray(data.faces) ? data.faces.length : -1;
  const bb = verts.length ? {
    xmin: Math.min(...verts.map(v => v[0])), xmax: Math.max(...verts.map(v => v[0])),
    ymin: Math.min(...verts.map(v => v[1])), ymax: Math.max(...verts.map(v => v[1])),
    zmin: Math.min(...verts.map(v => v[2])), zmax: Math.max(...verts.map(v => v[2])),
  } : null;
  const extentsOk = bb !== null && ['x', 'y', 'z'].every(axis =>
    (bb[`${axis}max`] - bb[`${axis}min`]) >=
    0.5 * (base.bbox[`${axis}max`] - base.bbox[`${axis}min`]) - TOL);
  const checks = {
    schema: keys === 'faces,name,vertices',
    name: data.name === name,
    finite: verts.length > 0 && badVertex === -1,
    faces: faces >= base.faces,
    bbox: bb !== null &&
      bb.xmin >= base.bbox.xmin - TOL && bb.xmax <= base.bbox.xmax + TOL &&
      bb.ymin >= base.bbox.ymin - TOL && bb.ymax <= base.bbox.ymax + TOL &&
      bb.zmin >= base.bbox.zmin - TOL && bb.zmax <= base.bbox.zmax + TOL,
    extent: extentsOk,
  };
  return { ok: Object.values(checks).every(Boolean), faces, checks };
}

let failed = 0;
let checked = 0;
for (const name of Object.keys(baselines)) {
  if (name === '_meta') continue;
  checked++;
  const result = checkAsset(name, baselines[name]);
  if (!result.ok) failed++;
  console.log(`${result.ok ? 'PASS' : 'FAIL'} ${name} faces=${result.faces}/${baselines[name].faces} ${JSON.stringify(result.checks)}`);
}
if (checked !== baselines._meta.count) {
  console.log(`FAIL baseline count: ${checked} != ${baselines._meta.count}`);
  failed++;
}
console.log(`ASSETS=${checked} FAILED=${failed}`);
process.exit(failed === 0 ? 0 : 1);
