// A22 asset validator — `node tools/check_ground_item_assets.js [--all]`
// Gates refined monster parts: schema / name==stem / finite coords / face cap 4000
// / front-facing detail (mesh.y<0 vertex count exceeds raw baseline).
// Default: only monsters with a REFINEMENTS entry in the script; --all: every monster.
const fs = require('fs');
const path = require('path');

const dir = 'assets/models';
const slots = ['head', 'torso', 'arm', 'leg', 'weapon'];
const baselineFront = require('./ground_item_baselines.json');

const script = fs.readFileSync('art_src/scripts/blender_models_monsters.py', 'utf8');
const withRefine = new Set(
  [...script.matchAll(/REFINEMENTS\["([a-z_]+)"\]\s*=/g)].map(m => m[1])
);
// Family-refine loop covers all monsters via MONSTERS keys; when present, treat all as refined.
const familyRefine = script.includes('_REFINE_FAMILY');
const allArg = process.argv.includes('--all');
const names = Object.keys(baselineFront).filter(n =>
  allArg || familyRefine || withRefine.has(n)
);

let failed = 0;
for (const name of names) {
  for (const slot of slots) {
    const file = path.join(dir, `mon_${name}_part_${slot}.json`);
    let d;
    try { d = JSON.parse(fs.readFileSync(file, 'utf8')); }
    catch (e) { console.log(`FAIL parse ${file}: ${e.message}`); failed++; continue; }
    const keys = Object.keys(d).sort().join(',');
    const bad = d.vertices.findIndex(v =>
      !Array.isArray(v) || v.length !== 6 ||
      v.slice(0, 3).some(x => typeof x !== 'number' || !Number.isFinite(x)));
    const tris = Array.isArray(d.faces) ? d.faces.length : -1;
    const front = d.vertices.filter(v => v[1] < -1.0).length;
    const base = baselineFront[name][slot];
    const checks = {
      schema: keys === 'faces,name,vertices',
      name: d.name === `mon_${name}_part_${slot}`,
      finite: bad === -1,
      cap4000: tris > 0 && tris <= 4000,
      front_detail: front > base,
    };
    const ok = Object.values(checks).every(Boolean);
    if (!ok) failed++;
    if (!ok || allArg) {
      console.log(`${ok ? 'PASS' : 'FAIL'} ${name}/${slot} tris=${tris} front=${front}/${base} ${JSON.stringify(checks)}`);
    }
  }
}
console.log(`CHECKED=${names.length * slots.length} MONSTERS=${names.length} FAILED=${failed}`);
process.exit(failed === 0 ? 0 : 1);
