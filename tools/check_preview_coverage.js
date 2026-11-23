// 逐件资产预览覆盖闸门 — `node tools/check_preview_coverage.js`
// 单一事实源: art_src/preview/sheet_manifest.json (由 preview_all_assets.py 生成).
// 验证: 每个模型 json 都在某张 sheet 清单里 | 8 张 sheet png 存在且非微小 | 无陈旧条目.
const fs = require('fs');
const path = require('path');

// 锚定到仓库根 (本文件在 tools/ 下), 从任意 cwd 运行都可.
const REPO = path.resolve(__dirname, '..');
const MODEL_DIR = path.join(REPO, 'assets', 'models');
const PREVIEW_DIR = path.join(REPO, 'art_src', 'preview');
const MANIFEST = path.join(PREVIEW_DIR, 'sheet_manifest.json');
// 动态阈值: 纯色空白 sheet 的体积约为 area*0.004, 真实渲染含模型约 area*0.08+.
// 用 area*0.01 作下限, 可拦住任何纯色/近空白图 (旧固定 2000B 对大图失效).
const MIN_BYTES_PER_PX = 0.01;

function pngSize(buf) {
  if (buf.length < 24 || buf.toString('ascii', 1, 4) !== 'PNG') return null;
  return { w: buf.readUInt32BE(16), h: buf.readUInt32BE(20) };
}
// 这两类已有独立逐资产预览 (landmark_preview_*.png / terrain_preview_*.png), 不属于 P2 缺口.
const INDEPENDENT = [
  { prefix: 'lm_', png: n => `landmark_preview_${n.slice(3)}.png` },
  { prefix: 'terrain_', png: n => `terrain_preview_${n}.png` },
];

let failed = 0;

const models = fs.readdirSync(MODEL_DIR).filter(f => f.endsWith('.json')).map(f => f.slice(0, -5)).sort();

let manifest;
try {
  manifest = JSON.parse(fs.readFileSync(MANIFEST, 'utf8'));
} catch (e) {
  console.log(`FAIL manifest: ${e.message} — 先运行 python art_src/scripts/preview_all_assets.py`);
  process.exit(1);
}

const covered = new Set();
for (const group of Object.keys(manifest)) {
  const names = manifest[group];
  for (const n of names) covered.add(n);
  const png = path.join(PREVIEW_DIR, group + '.png');
  let ok = fs.existsSync(png);
  let bytes = ok ? fs.statSync(png).size : 0;
  const dims = ok ? pngSize(fs.readFileSync(png)) : null;
  const floor = dims ? Math.max(2000, dims.w * dims.h * MIN_BYTES_PER_PX) : Infinity;
  if (bytes < floor) ok = false;
  if (!ok) failed++;
  console.log(`${ok ? 'PASS' : 'FAIL'} ${group} cells=${names.length} png_bytes=${bytes}/${Math.round(floor)}`);
}

for (const m of models) {
  if (covered.has(m)) continue;
  const indie = INDEPENDENT.find(x => m.startsWith(x.prefix));
  if (indie) {
    const p = path.join(PREVIEW_DIR, indie.png(m));
    const ok = fs.existsSync(p) && fs.statSync(p).size >= 2000;
    if (!ok) { console.log(`FAIL independent preview missing: ${p}`); failed++; }
    continue;
  }
  console.log(`FAIL uncovered model: ${m}`); failed++;
}
for (const n of covered) {
  if (!models.includes(n)) { console.log(`FAIL stale preview entry: ${n}`); failed++; }
}

console.log(`MODELS=${models.length} COVERED=${covered.size} FAILED=${failed}`);
process.exit(failed === 0 ? 0 : 1);
