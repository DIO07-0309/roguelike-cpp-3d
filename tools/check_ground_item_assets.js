// A22 asset validator — runs from repo root: `node tools/check_ground_item_assets.js`
// Gates refined monster parts: schema / name==stem / finite coords / face cap 4000
// / front-facing detail (mesh.y<0 vertex count exceeds raw baseline).
const fs = require('fs');
const path = require('path');

const dir = 'assets/models';
const pilots = ['orc', 'slime', 'golem', 'fire_imp'];
const slots = ['head', 'torso', 'arm', 'leg', 'weapon'];

// Raw-baseline front vertex counts (mesh.y < -1.0), measured from unrefined builders.
// Any refined part must exceed these — this catches details authored on the back (mesh.y>0).
const baselineFront = {
  orc:      {head:1334, torso:1300, arm:2492, leg:926,  weapon:1708},
  slime:    {head:804,  torso:1124, arm:396,  leg:232,  weapon:256},
  golem:    {head:1080, torso:976,  arm:1080, leg:648,  weapon:1192},
  fire_imp: {head:1050, torso:1192, arm:940,  leg:219,  weapon:1758},
};

let failed = 0;
for (const name of pilots) {
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
    const checks = {
      schema: keys === 'faces,name,vertices',
      name: d.name === `mon_${name}_part_${slot}`,
      finite: bad === -1,
      cap4000: tris > 0 && tris <= 4000,
      front_detail: front > baselineFront[name][slot],
    };
    const ok = Object.values(checks).every(Boolean);
    if (!ok) failed++;
    console.log(`${ok ? 'PASS' : 'FAIL'} ${name}/${slot} tris=${tris} front=${front}/${baselineFront[name][slot]} ${JSON.stringify(checks)}`);
  }
}
console.log(`TOTAL=${pilots.length * slots.length} FAILED=${failed}`);
process.exit(failed === 0 ? 0 : 1);
