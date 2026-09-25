#!/usr/bin/env python3
"""
G7.1: World Validator — automatic cross-reference checker for all JSON resources.

Checks: JSON refs, dialogue next links, effect/risk DSL, empty buff strings,
        biome coverage, colon-delimited spawn/buff/debuff/equipment references.

Usage: python tools/world_validator.py [resources_dir]
"""

import json, os, sys
from collections import defaultdict

RES_DIR = sys.argv[1] if len(sys.argv) > 1 else "resources"
errors = []
warnings = []


def load_json(path: str):
    p = os.path.join(RES_DIR, path)
    if not os.path.exists(p):
        errors.append(f"MISSING: {path}")
        return None
    with open(p, "r", encoding="utf-8") as f:
        return json.load(f)


def ids_from(data, key="id") -> set:
    if isinstance(data, list):
        return {item[key] for item in data if key in item}
    if isinstance(data, dict):
        return set(data.keys())
    return set()


def err(msg): errors.append(msg)


def check_ref(ref, valid_set, label, context=""):
    if ref and ref not in valid_set:
        err(f"BROKEN REF: {label} '{ref}' not found {context}")


# ═══ Load ═══
print(f"[Validator] Scanning {RES_DIR}/ ...")

enemies    = load_json("enemies.json") or []
bosses     = load_json("bosses.json") or {}
skills     = load_json("skills.json") or []
buffs      = load_json("buffs.json") or []
relics     = load_json("relics.json") or []
items      = load_json("items.json") or []
biomes     = load_json("biomes.json") or []
landmarks  = load_json("landmarks.json") or []
encounters = load_json("encounters.json") or []
bio_events = load_json("biome_events.json") or []
vfx        = load_json("vfx_recipes.json") or {}
floor_cfg  = load_json("floor_config.json") or []
floor_nar  = load_json("floor_narrative.json") or []
quests     = load_json("quests.json") or []
dialogues  = load_json("dialogues.json") or []
endings    = load_json("endings.json") or []
meta_nodes = load_json("meta_nodes.json") or []

world_files = {}
for wf in ["world/prison.json", "world/volcano.json", "world/abyss.json"]:
    data = load_json(wf)
    if data:
        world_files[wf] = data

# ID sets
enemy_ids     = ids_from(enemies)
boss_ids      = ids_from(bosses) | {"5", "10", "15"}
skill_ids     = ids_from(skills)
buff_ids      = ids_from(buffs)
relic_ids     = ids_from(relics)
item_ids      = ids_from(items)
biome_ids     = ids_from(biomes)
landmark_ids  = ids_from(landmarks)
encounter_ids = ids_from(encounters)
vfx_recipes   = set(vfx.get("recipes", {}).keys()) if isinstance(vfx, dict) else set()

valid_bgm        = {"title", "select", "dungeon", "boss", "prison", "volcano", "abyss"}
valid_triggers   = {"floor_enter", "room_enter", "wall_interact", "entity_interact", "boss_dead", "item_use", "talk"}
valid_enc_types  = {"event", "npc", "trader", "quest_giver"}
valid_eff_kinds  = {"buff","relic","equipment","skill_level","heal","relic_from_pool","none","trade","set_flag","set_meta_flag"}
valid_risk_kinds = {"spawn","hp_loss","debuff","confuse","none"}

print(f"  {len(enemy_ids)} enemies, {len(boss_ids)} bosses, {len(skill_ids)} skills, "
      f"{len(buff_ids)} buffs, {len(relic_ids)} relics, {len(item_ids)} items, "
      f"{len(biome_ids)} biomes, {len(landmark_ids)} landmarks, "
      f"{len(encounter_ids)} encounters")


# ── Boss defs 自检 (B4: GOLEM 挑战房压轴 boss) ──
VALID_BOSS_SKILLS = {"charge", "shockwave", "summon", "barrage", "cone", "blink"}
VALID_COMBO_CMDS = {"normal", "charge", "shockwave", "summon", "defend",
                    "barrage", "cone", "blink", "whirlwind"}

if isinstance(bosses, dict):
    _boss_list = list(bosses.values())
elif isinstance(bosses, list):
    _boss_list = list(bosses)
else:
    _boss_list = []
_boss_by_id = {b.get("id"): b for b in _boss_list if b.get("id")}

# get_boss_def_for_floor 是硬编码 switch (boss_defs.cpp:170-176), 目标必须存在
for _fl, _fid in ((5, "shadow_knight"), (10, "fire_demon"), (15, "demon_lord")):
    if _fid not in _boss_by_id:
        err("bosses.json: get_boss_def_for_floor(%d) 硬编码 '%s' 但 JSON 无该条目"
            % (_fl, _fid))

# BossType 映射一致性: get_boss_def_for_type(4) -> "golem" (boss_defs.cpp:185)
if "golem" not in _boss_by_id:
    err("bosses.json: 缺 'golem' — BossType::GOLEM(4) 映射断链, 挑战房压轴不可达")
else:
    _g = _boss_by_id["golem"]
    if not _g.get("is_defender"):
        err("bosses.json [golem]: is_defender 必须为 true (golem_shield_pct 赋值开关)")
    if not (_g.get("shield_pct") or 0) > 0:
        err("bosses.json [golem]: shield_pct 必须 > 0 (boss.cpp:787 生效条件)")
    # 技能按 id-match 覆盖参数 (boss.cpp:1240-1257), 与数组位置无关
    _ids = [_s.get("id") for _s in _g.get("skills", [])]
    for _req in ("charge", "shockwave"):
        if _req not in _ids:
            err("bosses.json [golem].skills 缺 '%s' — ai->_%s 参数无法被覆盖"
                % (_req, _req))
    if _g.get("skill_cycle_bias") in (4, 6):
        err("bosses.json [golem].skill_cycle_bias=%s 会让 _next_cycle_skill "
            "返回 Summon, 与纯 tank 定位冲突" % _g.get("skill_cycle_bias"))
    if _g.get("is_summoner"):
        warnings.append("bosses.json [golem].is_summoner=true — 该字段只用于显示串, "
                        "不 gating 召唤")
    for _i, _s in enumerate(_g.get("skills", [])):
        if _s.get("id") not in VALID_BOSS_SKILLS:
            err("bosses.json [golem].skills[%d].id '%s' 非法"
                % (_i, _s.get("id")))
    for _ci, _c in enumerate(_g.get("combos", [])):
        for _cmd in _c.get("commands", []):
            if _cmd not in VALID_COMBO_CMDS:
                err("bosses.json [golem].combos[%d].commands '%s' 非法"
                    % (_ci, _cmd))


# ═══ Step 1: Basic cross-references ═══
for b in biomes:
    ctx = f"biomes.json [{b['id']}]"
    for e in b.get("enemy_pool", []):
        check_ref(e, enemy_ids, f"{ctx} enemy_pool", ctx)
    check_ref(b.get("boss_id",""), boss_ids, f"{ctx} boss_id", ctx)
    check_ref(b.get("bgm",""), valid_bgm, f"{ctx} bgm", ctx)
    # A2.2: ambient.style 白名单 + ambient.texture 文件存在 (从仓库根运行)
    amb = b.get("ambient", {})
    style = amb.get("style", "")
    if style and style not in ("dust", "ember", "firefly"):
        err(f"BROKEN REF: {ctx} ambient.style '{style}' 非法 (dust/ember/firefly)")
    tex = amb.get("texture", "")
    if tex and not os.path.exists(tex):
        err(f"MISSING: {ctx} ambient.texture {tex}")

for lm in landmarks:
    ctx = f"landmarks.json [{lm['id']}]"
    check_ref(lm["biome"], biome_ids, f"{ctx} biome", ctx)

for enc in encounters:
    ctx = f"encounters.json [{enc['id']}]"
    check_ref(enc["biome"], biome_ids, f"{ctx} biome", ctx)
    check_ref(enc["trigger"], valid_triggers, f"{ctx} trigger", ctx)
    check_ref(enc["type"], valid_enc_types, f"{ctx} type", ctx)
    if enc.get("room"):
        check_ref(enc["room"], landmark_ids | {""}, f"{ctx} room (landmark)", ctx)
    node_ids = set()
    for node in enc.get("dialogue", []):
        node_ids.add(node["id"])
        for c in node.get("choices", []):
            eff = c.get("effect","none")
            rsk = c.get("risk","none")
            for val, label in [(eff,"effect"),(rsk,"risk")]:
                if val not in ("","none") and ":" in val:
                    check_ref(val.split(":")[0], valid_eff_kinds|valid_risk_kinds,
                              f"{ctx} node[{node['id']}] {label}.kind", ctx)
    for node in enc.get("dialogue", []):
        for c in node.get("choices", []):
            nxt = c.get("next","end")
            if nxt != "end" and nxt not in node_ids:
                err(f"BROKEN NEXT: {ctx} node[{node['id']}] '{c['text'][:20]}' → '{nxt}'")

if isinstance(bio_events, list):
    for bev in bio_events:
        ctx = f"biome_events.json [{bev['id']}]"
        check_ref(bev["biome"], biome_ids, f"{ctx} biome", ctx)

for fc in floor_cfg:
    f = fc.get("floor",0)
    if f < 1 or f > 15:
        err(f"floor_config.json: floor {f} out of range")

for fn in floor_nar:
    f = fn.get("floor",0)
    if f < 1 or f > 15:
        err(f"floor_narrative.json: floor {f} out of range")


# ═══ Step 2: Colon-delimited effect/risk refs ═══
def check_colon(val, idx, kind, valid_set, ctx):
    if val in ("","none"): return
    parts = val.split(":")
    if kind == "spawn":
        if len(parts) >= 2: check_ref(parts[1], valid_set, f"{ctx} spawn target", ctx)
    elif kind in ("buff","debuff"):
        if len(parts) >= 2: check_ref(parts[1], valid_set, f"{ctx} {kind} target", ctx)
    elif kind == "equipment":
        if len(parts) >= 2 and parts[1] not in ("rare","epic","legendary","common"):
            err(f"INVALID RARITY: '{val}' in {ctx}")
    elif kind not in ("potion","skill_level","relic","hp_loss","confuse","heal",
                      "relic_from_pool","set_flag","set_meta_flag","trade","none"):
        err(f"UNKNOWN TOKEN: '{kind}' in '{val}' at {ctx}")

for src_list in [encounters, bio_events]:
    if not isinstance(src_list, list): continue
    for item in src_list:
        ctx = f"[{item['id']}]"
        for node in item.get("dialogue", []):
            for c in node.get("choices", []):
                for fld in ("effect","risk"):
                    val = c.get(fld,"none")
                    if val not in ("","none") and ":" in val:
                        check_colon(val,0,val.split(":")[0],
                                    enemy_ids|buff_ids,
                                    f"{item['id']} node[{node['id']}] {fld}")
        for c in item.get("choices", []):
            for fld in ("effect","risk"):
                val = c.get(fld,"none")
                if val not in ("","none") and ":" in val:
                    check_colon(val,0,val.split(":")[0],
                                enemy_ids|buff_ids,
                                f"{item['id']} choice {fld}")


# ═══ Step 3: Enemy/skill/item internal refs ═══
for enemy in enemies:
    for eb in enemy.get("elite_buffs", []):
        bv = eb.get("buff","")
        if bv == "":
            err(f"EMPTY BUFF: enemies.json [{enemy['id']}] elite_buffs")
        elif bv not in buff_ids:
            err(f"BROKEN REF: enemies.json [{enemy['id']}] elite_buffs buff '{bv}'")
    for oh in enemy.get("on_hit", []):
        bv = oh.get("buff","")
        if bv and bv not in buff_ids:
            err(f"BROKEN REF: enemies.json [{enemy['id']}] on_hit buff '{bv}'")

for sk in skills:
    for tr in sk.get("triggers", []):
        bv = tr.get("buff","")
        if bv and bv not in buff_ids:
            err(f"BROKEN REF: skills.json [{sk['id']}] trigger buff '{bv}'")

for it in items:
    for ref_field, id_set, label in [("buff_id", buff_ids, "buff"), ("skill_id", skill_ids, "skill")]:
        if ref_field in it and it[ref_field]:
            check_ref(it[ref_field], id_set, f"items.json [{it['id']}] {ref_field}")

for wf_path, wf in world_files.items():
    b = wf.get("biome",{})
    if b.get("id","") not in biome_ids:
        err(f"BROKEN REF: {wf_path} biome.id '{b.get('id','')}'")


# ═══ Step 4: Coverage warnings ═══
for b in biomes:
    if not b.get("enemy_pool"): warnings.append(f"biomes.json [{b['id']}]: empty enemy_pool")
    if not b.get("boss_id"):    warnings.append(f"biomes.json [{b['id']}]: no boss_id")

lm_count = defaultdict(int)
for lm in landmarks: lm_count[lm["biome"]] += 1
for bid in biome_ids:
    if lm_count.get(bid,0) == 0:
        warnings.append(f"biome '{bid}': no landmarks")
    elif lm_count[bid] < 2:
        warnings.append(f"biome '{bid}': only {lm_count[bid]} landmark(s)")

enc_count = defaultdict(int)
for enc in encounters: enc_count[enc["biome"]] += 1
for bid in biome_ids:
    if enc_count.get(bid,0) == 0:
        warnings.append(f"biome '{bid}': no encounters")


# ═══ G5.8.8: VFX recipe 完整性 ═══
valid_vfx_kinds = {"ring","beam","bolt","lightning","explosion","shockwave",
                   "slash_arc","smoke","spark","aura","flash","cone","pulse"}
valid_vfx_colors = {"default","fire","ice","lightning","poison","time","heal",
                    "shadow","bleed","white","summon","blood","nature","holy",
                    "void","gold","red"}

elements_data = load_json("elements.json") or {}
elements_list = elements_data.get("elements", []) if isinstance(elements_data, dict) else []

for el in elements_list:
    ctx = f"elements.json [{el.get('id','')}]"
    for slot, ref in (el.get("vfx") or {}).items():
        check_ref(ref, vfx_recipes, f"{ctx} vfx.{slot}", "in vfx_recipes.json")

for rid, recipe in (vfx.get("recipes") or {}).items():
    for i, step in enumerate(recipe.get("steps", [])):
        kind = step.get("kind")
        if kind and kind not in valid_vfx_kinds:
            err(f"BAD VFX KIND: {rid} step {i} kind '{kind}' (valid: {sorted(valid_vfx_kinds)})")
        color = step.get("color")
        if color and color not in valid_vfx_colors:
            err(f"BAD VFX COLOR: {rid} step {i} color '{color}' (valid: {sorted(valid_vfx_colors)})")


# ═══ D2: Enemy projectile 配置完整性 ═══
ranged_types = {"archer", "shaman"}
for e in enemies:
    ctx = f"enemies.json [{e.get('id','')}]"
    pj = e.get("projectile")
    if e.get("type") in ranged_types and not pj:
        warnings.append(f"{ctx}: ranged type '{e.get('type')}' has no projectile block")
    if pj:
        if not isinstance(pj.get("enabled", True), bool):
            err(f"{ctx} projectile.enabled must be bool")
        spd = pj.get("speed")
        if spd is not None and not (100.0 <= spd <= 600.0):
            err(f"{ctx} projectile.speed {spd} out of range [100,600]")
        wt = pj.get("warning_time")
        if wt is not None and not (0.2 <= wt <= 2.0):
            err(f"{ctx} projectile.warning_time {wt} out of range [0.2,2.0]")
        lvl = pj.get("warning_level")
        if lvl is not None and lvl not in (0, 1, 2):
            err(f"{ctx} projectile.warning_level {lvl} must be 0/1/2")


# ═══ A5: skeletal animation cross-refs ═══
skel = load_json(os.path.join("animations", "player_skeleton.json"))
anim = load_json(os.path.join("animations", "player_anim.json"))
if skel and anim:
    bone_names = {b["name"] for b in skel.get("bones", [])}
    for i, p in enumerate(skel.get("parts", [])):
        if p.get("bone") not in bone_names:
            err(f"player_skeleton parts[{i}]: unknown bone '{p.get('bone')}'")
        part_file = str(p.get("file", "")).replace("\\", "/")
        part_file = part_file[len("assets/sprites/"):] if part_file.startswith("assets/sprites/") else part_file
        sprite = os.path.join("assets", "sprites", part_file)
        if not os.path.exists(sprite):
            err(f"player_skeleton parts[{i}]: missing art {sprite}")
    if skel.get("pixels_per_unit", 0) <= 0:
        err("player_skeleton: pixels_per_unit must be > 0")
    for cname, clip in anim.get("animations", {}).items():
        if clip.get("dur", 0) <= 0:
            err(f"player_anim [{cname}]: dur must be > 0")
        for tr in clip.get("tracks", []):
            if tr.get("bone") not in bone_names:
                err(f"player_anim [{cname}]: unknown track bone '{tr.get('bone')}'")
            ks = tr.get("keys", [])
            if not ks:
                err(f"player_anim [{cname}]: track '{tr.get('bone')}' has no keys")
                continue
            if abs(ks[-1].get("t", -1) - clip.get("dur", 0)) > 1e-3:
                err(f"player_anim [{cname}]: track '{tr.get('bone')}' last key t != dur")
            if ks[0].get("t", -1) > 1e-3:
                warnings.append(f"player_anim [{cname}]: track '{tr.get('bone')}' first key t != 0")
            ts = [k.get("t", 0) for k in ks]
            if ts != sorted(ts):
                err(f"player_anim [{cname}]: track '{tr.get('bone')}' keys not sorted by t")
    for need in ("idle", "walk", "attack", "hit"):
        if need not in anim.get("animations", {}):
            err(f"player_anim: required clip '{need}' missing")

# ═══ A6: camera language validation ═══
cam = load_json(os.path.join("camera", "boss_camera.json"))
if cam:
    bw = cam.get("boss_war", {})
    for zoom_key in ("zoom_in", "zoom_out"):
        z = bw.get(zoom_key, {})
        fs = z.get("fov_scale", 0)
        if fs <= 0.1 or fs > 2.0:
            err(f"boss_camera: boss_war.{zoom_key}.fov_scale {fs} out of range (0.1, 2.0]")
        if z.get("duration", 0) < 0:
            err(f"boss_camera: boss_war.{zoom_key}.duration must be >= 0")
    ls = bw.get("lerp_speed", 0)
    if ls <= 0:
        err("boss_camera: boss_war.lerp_speed must be > 0")
    ks = cam.get("kill_stun", {})
    kd = ks.get("duration", 0)
    if kd <= 0 or kd > 0.15:
        err(f"boss_camera: kill_stun.duration {kd} out of range (0, 0.15]")
    if ks.get("shake_amplitude", 0) < 0:
        err("boss_camera: kill_stun.shake_amplitude must be >= 0")
    if ks.get("shake_frequency", 0) <= 0:
        err("boss_camera: kill_stun.shake_frequency must be > 0")

# ═══ A6-S2 批次9: spawn tables 前后向交叉校验 ═══
# enemy_slots.json (12 槽位 -> 候选怪) + challenge_pools.json (3 群系 x 3 波)
# 前向: 候选 id 必须是真敌人或已声明别名; 反向: enemies.json 每个敌人必须可达
_spawn_slot_ok = os.path.exists(os.path.join(RES_DIR, "enemy_slots.json"))
_spawn_pool_ok = os.path.exists(os.path.join(RES_DIR, "challenge_pools.json"))
if not (_spawn_slot_ok and _spawn_pool_ok):
    print("  [INFO] spawn tables 未就绪, 跳过 A6-S2 批次9 校验")
else:
    spawn_slots = load_json("enemy_slots.json")
    spawn_pools = load_json("challenge_pools.json")
    slot_list = spawn_slots.get("slots", []) if isinstance(spawn_slots, dict) else []
    aliases = spawn_slots.get("aliases", {}) if isinstance(spawn_slots, dict) else {}
    alias_keys = set(aliases.keys())

    def _resolve(eid):
        return set(aliases[eid]) if eid in aliases else {eid}

    reachable = set()
    challenge_only = set()

    if len(slot_list) != 12:
        err(f"enemy_slots.json: slots 数量 {len(slot_list)} != 12 (需与 FloorConfig::enemy_weights[12] 对齐)")
    for i, sl in enumerate(slot_list):
        ctx = f"enemy_slots.json slot[{i}] {sl.get('archetype', '?')}"
        cands = sl.get("candidates", [])
        if not cands:
            err(f"{ctx}: 空 candidates")
        for c in cands:
            cid = c.get("id", "")
            if cid not in enemy_ids and cid not in alias_keys:
                err(f"BROKEN REF: {ctx} candidate '{cid}' not in enemies.json / aliases")
            if c.get("weight", 0) <= 0:
                err(f"{ctx} candidate '{cid}': weight must be > 0")
            fl = c.get("floors")
            if fl is not None and (not isinstance(fl, list) or len(fl) != 2
                                   or fl[0] <= 0 or fl[1] > 15 or fl[0] > fl[1]):
                err(f"{ctx} candidate '{cid}': floors {fl} 非法 (需闭区间 1..15)")
            reachable.update(_resolve(cid))

    for bi, bm in enumerate(spawn_pools.get("biomes", []) if isinstance(spawn_pools, dict) else []):
        ctx = f"challenge_pools.json biome[{bi}]"
        fl = bm.get("floors")
        if not isinstance(fl, list) or len(fl) != 2 or fl[0] <= 0 or fl[1] > 15 or fl[0] > fl[1]:
            err(f"{ctx}: floors {fl} 非法 (需闭区间 1..15)")
        for wi, wv in enumerate(bm.get("waves", [])):
            if not wv:
                err(f"{ctx} wave[{wi}]: 空池")
            for x in wv:
                if x not in enemy_ids and x not in alias_keys:
                    err(f"BROKEN REF: {ctx} wave[{wi}] '{x}' not in enemies.json / aliases")
                challenge_only.update(_resolve(x))

    for e in sorted(enemy_ids - reachable - challenge_only):
        err(f"UNREACHABLE: enemies.json '{e}' 不在 enemy_slots.json 或 challenge_pools.json (运行时永不刷出)")
    only_challenge = sorted((challenge_only - reachable) & enemy_ids)
    print(f"  [INFO] spawn: {len(enemy_ids)} enemies, 直达 {len(reachable & enemy_ids)}, "
          f"仅经挑战房(需钥匙) {len(only_challenge)}: {', '.join(only_challenge)}")

# ═══ Report ═══
print(f"\n{'='*60}")
print(f"  WORLD VALIDATOR REPORT")
print(f"{'='*60}")
print(f"  Resources: {RES_DIR}")
print(f"  Files:    20+ JSON")
print(f"  Errors:   {len(errors)}")
print(f"  Warnings: {len(warnings)}")

if errors:
    print(f"\n  ═══ ERRORS ═══")
    for e in errors:
        print(f"  [ERR] {e}")

if warnings:
    print(f"\n  ═══ WARNINGS ═══")
    for w in warnings:
        print(f"  [WRN] {w}")

if not errors and not warnings:
    print(f"\n  All checks passed. World is valid.")

print(f"{'='*60}")
exit(0 if not errors else 1)
