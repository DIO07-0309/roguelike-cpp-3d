// A22 asset validator — `node tools/check_ground_item_assets.js [--all]`
// Gates refined monster parts: schema / name==stem / finite coords / face cap 4000
// / front-facing detail (mesh.y<0 vertex count exceeds raw baseline).
// Default: only monsters with a REFINEMENTS entry in the script; --all: every monster.
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

// P6 一怪一形: --unique 检查 26 种签名怪的 head+签名槽顶点坐标多重集哈希互不相同。
// 直接读 JSON (注入用 REFINEMENTS[_mon] 正则匹配不到); 排除 arm/leg (merge 保留 pilot 几何)。
const SIGNATURES = {
  night_stalker:  ['head','weapon'],
  skeleton_archer:['head','weapon'],
  bone_soldier:   ['head','weapon'],
  shadow_assassin:['head','weapon'],
  shadow_stalker: ['head','torso'],
  storm_elemental:['head','torso'],
  lightning_orb:  ['head'],
  void_walker:    ['head','torso'],
  blood_leech:    ['head','torso'],
  poison_wyrm:    ['head','weapon'],
  stone_guardian: ['head','torso'],
  ice_warden:     ['head','torso'],
  iron_sentinel:  ['head','torso'],
  // 第二批 (剩余 13 种)
  archer:         ['head','weapon'],
  goblin_hunter:  ['head','weapon'],
  shaman:         ['head','torso'],
  dark_mage:      ['head','weapon'],
  blood_priest:   ['head','torso'],
  summoner:       ['head','weapon'],
  necromancer:    ['head','weapon'],
  tank:           ['head','torso'],
  charger:        ['head','weapon'],
  bomber:         ['head','torso'],
  elite_orc:      ['head','weapon'],
  frost_slime:    ['head'],
  elite_slime:    ['head'],
};
// 每族的 pilot (签名怪必须与它不同)。
const FAMILY_PILOT = {
  night_stalker:'orc', skeleton_archer:'orc', bone_soldier:'orc',
  shadow_assassin:'orc', shadow_stalker:'orc', ice_warden:'orc', iron_sentinel:'orc',
  storm_elemental:'fire_imp', lightning_orb:'fire_imp', void_walker:'fire_imp',
  blood_leech:'slime', poison_wyrm:'slime',
  stone_guardian:'golem',
  // 第二批
  archer:'orc', goblin_hunter:'orc', shaman:'orc', dark_mage:'orc', blood_priest:'orc',
  summoner:'orc', necromancer:'orc', tank:'orc', charger:'orc', bomber:'orc', elite_orc:'orc',
  frost_slime:'slime', elite_slime:'slime',
};

function coordHash(file) {
  let d;
  try { d = JSON.parse(fs.readFileSync(file, 'utf8')); }
  catch (e) { return { err: e.message }; }
  // 顶点坐标 (x,y,z) round(,3) 已固化; 排序拼接 → 多重集哈希, 免疫 bmesh 面序非确定性。
  const coords = d.vertices.map(v => `${v[0]},${v[1]},${v[2]}`).sort();
  const h = crypto.createHash('sha256').update(coords.join('|')).digest('hex').slice(0, 16);
  return { hash: h, count: d.vertices.length };
}

function runUnique() {
  // 使用模块级 failed (与 150 项 schema/finite/cap 检查合并), 不单独 process.exit。
  const ids = Object.keys(SIGNATURES);
  // 按 slot 分组比较: 同族内两两不同 + 与 pilot 不同。
  for (const slot of ['head','weapon','torso']) {
    const group = ids.filter(id => SIGNATURES[id].includes(slot));
    if (group.length < 2) continue;
    const seen = new Map(); // hash -> id
    for (const id of group) {
      const r = coordHash(`assets/models/mon_${id}_part_${slot}.json`);
      if (r.err) { console.log(`FAIL ${id}/${slot}: ${r.err}`); failed++; continue; }
      if (seen.has(r.hash)) {
        console.log(`FAIL ${id}/${slot} hash=${r.hash} COLLIDES with ${seen.get(r.hash)} (cloning)`);
        failed++;
      } else seen.set(r.hash, id);
      // 与 pilot 比
      const pilot = FAMILY_PILOT[id];
      const pr = coordHash(`assets/models/mon_${pilot}_part_${slot}.json`);
      if (!pr.err && pr.count === r.count && pr.hash === r.hash) {
        console.log(`FAIL ${id}/${slot} identical to pilot ${pilot} (cloning)`);
        failed++;
      }
    }
  }
  console.log(`UNIQUE ids=${ids.length} FAILED=${failed}`);
}

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
if (process.argv.includes('--unique')) runUnique();

console.log(`CHECKED=${names.length * slots.length} MONSTERS=${names.length} FAILED=${failed}`);
process.exit(failed === 0 ? 0 : 1);
