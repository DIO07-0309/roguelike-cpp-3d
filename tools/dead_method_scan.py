"""死方法扫描 v2 —— 限定名解析, 补掉 v1 的 tick/draw/reset 重名盲区。

用法:
    conda run python tools/dead_method_scan.py
    (在仓库根目录跑; 需要 conda 环境的 python, 系统 python 是 WindowsApps stub)

v1 的盲点: re.findall(r"\b<name>\b") 数裸词。tick/draw/reset/update 这类
重名方法全仓出现几百次, n<=2 的阈值永远达不到, 整类方法从不可见。

v2 的判定分三层, 宁可报「待人工」也不误报「确证死」:
  确证死  —— 四种引用形态全为零
  待人工  —— 无任何限定引用, 但存在点号调用或裸 token 碰撞
  存活    —— 有 Class::name 引用, 或有 Class 自身体内的裸调用

四种引用形态:
  1. Class::name      —— 限定调用
  2. name( 在本类体内 —— implicit this, Foo::draw 在 Foo::tick 里写裸 draw(...)
  3. .name( / ->name( —— 最宽松一层, 只要有就不判死
  4. 裸 token          —— 名字作为「值」传递: EventBus::subscribe(..., _handler,
                          "..."), std::bind, std::function 参数。前三道闸门
                          全都看不见, 这是把 _on_relic_gained 误判死的元凶

已排除的不可判定类 (纳入必然大规模误报):
  - 构造函数: 调用点是 T x(...)/make_unique<T>(..., 无 Class::Class( 也无 .Class(
  - 析构函数: 调用点在 unique_ptr/vector 清理与虚表分派里, 文本分析不可见

局限 (已知, 宁漏勿误):
  - 不做类型推导, obj->tick() 中 obj 的静态类型无法解析, 故只要存在
    任意 .name( / ->name( 就降级为「待人工」, 不判死
  - 不处理嵌套类; operator 重载不在覆盖范围
  - 成员变量式初始化 (std::vector<int> v(10);) 若参数含空格/&/*/逗号/>
    会被当成方法
  - 所以「确证死」只是强候选, 删除前仍要人工看一眼调用点

产出参考 (2026-09-30 首跑): 938 候选 / 16 确证死 / 355 待人工。
16 个全部人工复核确认真死, 其中 Player::render 是空体,
GameScene::_player_attack/_use_skill 是 D6 委托后残留的空壳。

历史金标准验证 (同一脚本分别跑 2ce1e2b 与 d2278b1 两棵树):
  - 确证死清单在相隔 7 个 commit 的两棵树上完全一致 (16/16), 说明确定性
  - 待人工从 356 降到 355, 唯一消失的是 CombatCoordinator::cleanup_dead_monsters
    —— 正是 G21 (017be99) 手工发现并删除的互相引用死代码簇。
      v1 扫描器对它的裸词计数是 7 次, n<=2 阈值永远达不到, 整簇不可见。

工具位置校验: 脚本必须放在 <repo>/tools/ 下 (靠 __file__ 定位仓库根)。
放错位置会静默扫空根并打印一堆 0, 极易被误读成「全仓无死代码」,
故加护栏: 文件数为 0 时 sys.exit(1) 并打印实际定位到的路径。
"""
import io
import os
import re
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO_ROOT)

ROOTS = ["src", "tests"]
SKIP_DIRS = {"build"}
EXTS = (".h", ".hpp", ".cpp")

CLASS_KW = {"if", "for", "while", "switch", "return", "sizeof", "catch",
            "public", "private", "protected", "using", "typedef", "else",
            "do", "throw", "new", "delete", "operator", "and", "or", "not"}


def read(p):
    try:
        with io.open(p, encoding="utf-8", errors="replace") as f:
            return f.read()
    except Exception:
        return ""


def strip_noise(t):
    """去注释与字面量, 等长替换, 行号不变。"""
    out = []
    i, n = 0, len(t)
    while i < n:
        c = t[i]
        if c == "/" and i + 1 < n:
            if t[i + 1] == "/":
                while i < n and t[i] != "\n":
                    out.append(" ")
                    i += 1
                continue
            if t[i + 1] == "*":
                j = t.find("*/", i + 2)
                j = n if j < 0 else j + 2
                while i < j:
                    out.append("\n" if t[i] == "\n" else " ")
                    i += 1
                continue
        if c in ('"', "'"):
            q = c
            out.append(" ")
            i += 1
            while i < n:
                if t[i] == "\\":
                    out.append("  ")
                    i += 2
                    continue
                if t[i] == q:
                    out.append(" ")
                    i += 1
                    break
                out.append("\n" if t[i] == "\n" else " ")
                i += 1
            continue
        out.append(c)
        i += 1
    return "".join(out)


def match_brace(t, open_idx):
    """从 open_idx 处的 '{' 配对, 返回 '}' 的下标; 失败返回 -1。"""
    depth = 0
    for i in range(open_idx, len(t)):
        if t[i] == "{":
            depth += 1
        elif t[i] == "}":
            depth -= 1
            if depth == 0:
                return i
    return -1


# ---------------------------------------------------------------- 收集文件
files = []
for root in ROOTS:
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for fn in filenames:
            if fn.endswith(EXTS):
                files.append(os.path.join(dirpath, fn))

text = {}          # path -> 去噪文本
lines_of = {}      # path -> 原始行表 (用于回显)
for p in files:
    raw = read(p)
    text[p] = strip_noise(raw)
    lines_of[p] = raw.split("\n")

if not files:
    # 护栏: 脚本位置错了会静默扫空根并打印一堆 0, 极易被误读成「全仓无死代码」。
    sys.exit("错误: 未在 %s 下找到 src/ 或 tests/ —— 本脚本必须放在 <repo>/tools/ 下\n"
             "实际定位到: %s" % (REPO_ROOT, os.getcwd()))

corpus = "\n".join(text[p] for p in files)

# ------------------------------------------------- .h: 类 -> 成员声明
# 不用单条正则: [^;{]* 会跨嵌套括号, 把 static_cast<T*>(x); / printf("a", f())
# 当成方法声明; ret 若允许纯空白, "  " 也能当返回类型。改为「正则找头 + 括号配平」。
DECL_HEAD_RE = re.compile(
    r"^[ \t]*(?P<ret>[\w][\w:<>,&*\s]*?)[ \t]+(?P<name>~?\w+)\s*\(", re.M)
BAD_RET = ("(", ")", "=", "cast<")


def iter_method_decls(body):
    """产出类体内真实成员声明的方法名 (含析构)。"""
    for m in DECL_HEAD_RE.finditer(body):
        ret = m.group("ret")
        name = m.group("name")
        if name in CLASS_KW or any(b in ret for b in BAD_RET):
            continue
        depth, i, n = 1, m.end(), len(body)
        while i < n and depth:                     # 括号配平, 不跨嵌套
            if body[i] == "(":
                depth += 1
            elif body[i] == ")":
                depth -= 1
            i += 1
        if depth:
            continue
        args = body[m.end():i - 1]
        # 成员变量式初始化 std::vector<int> v(10); 的参数里没有类型记号
        if args and not re.search(r"[ &*,>]", args):
            continue
        j = i
        while j < n and body[j] in " \t\r\n":
            j += 1
        if not re.match(r"(?:const\s+|noexcept\s+|override\s+|final\s+)*;",
                        body[j:j + 80]):
            continue
        yield name


CLASS_RE = re.compile(r"\b(?:class|struct)\s+(\w+)\s*(?:final)?[^;{]*\{")

cands = {}         # (cls, name) -> [path]
for p, t in text.items():
    if not p.endswith((".h", ".hpp")):
        continue
    for cm in CLASS_RE.finditer(t):
        cls = cm.group(1)
        end = match_brace(t, cm.end() - 1)
        if end < 0:
            continue
        body = t[cm.end():end]
        for name in iter_method_decls(body):
            if name.startswith("~"):
                continue                           # 析构: 调用点在 unique_ptr/vector
                                                  # 清理与虚表分派里, 文本分析不可见
            if name == cls:
                continue                           # 构造函数: 调用点是 T x(...)/make_unique<T>(...),
                                                  # 两种形态都识别不了, 纳入必然大规模误报
            if len(name) < 3 or name.startswith("_") and len(name) < 4:
                continue
            cands.setdefault((cls, name), []).append(p)

# ----------------------------------------- .cpp: 限定定义 -> 类作用域函数体
# 返回类型写成「词符 + 空白」的可重复组 (组内不含 \s), 避免嵌套量词灾难性回溯;
# 组可为空 —— 析构/构造的定义里类名兼任返回类型 (SkeletonAvatar::~SkeletonAvatar())。
QDEF_RE = re.compile(r"^[ \t]*(?:[\w:*&<>,]+[ \t])*(\w+)::(~?\w+)\s*\(", re.M)

class_bodies = []  # (cls, name, path, start_off, end_off)
for p, t in text.items():
    if not p.endswith((".cpp", ".c")):
        continue
    for m in QDEF_RE.finditer(t):
        cls, name = m.group(1), m.group(2)
        if any(b in m.group(0) for b in ("=", "cast<")):
            continue                               # 赋值/强转, 不是定义
        # 配平参数括号找到签名真正结束的位置。
        # 早先直接 t.find("{") 找下一个大括号、再用 rfind(";",0,ob) 与
        # rfind("(",0,ob) 比较来判声明/定义 —— 这两个 rfind 会跨语句越过
        # 后续不相关的 if (...) {, 于是调用语句 Class::f(a, b) 被当成定义
        # (game_scene.cpp:479 / weapon_executor.cpp:106 都是这么漏进来的),
        # defs 虚增、q 被扣到 0, 把真被调用的方法误报成确证死。
        _depth = 1
        _k = m.end()
        while _k < len(t) and _depth > 0:
            if t[_k] == "(":
                _depth += 1
            elif t[_k] == ")":
                _depth -= 1
            _k += 1
        if _depth != 0:
            continue
        _j = _k
        while _j < len(t) and t[_j] in " \t\n":
            _j += 1
        if _j >= len(t) or t[_j] != "{":
            continue                               # ';' 结尾 = 声明或调用
        ob = _j
        cb = match_brace(t, ob)
        if cb < 0:
            continue
        class_bodies.append((cls, name, p, ob, cb))

# 按类名分组。判定循环要对每个候选遍历「本类的函数体」, 不分组就是
# 候选数 x 函数体总数 的线性扫描 (938 x 1661 = 156 万次), 全仓跑一次约 5 分钟。
bodies_by_class = {}
defs_by_pair = {}
for c, n, p, ob, cb in class_bodies:
    bodies_by_class.setdefault(c, []).append((p, ob, cb))
    defs_by_pair[(c, n)] = defs_by_pair.get((c, n), 0) + 1

# ---------------------------------------------------------------- 预计算索引
# 早先每道闸门都全语料跑一遍 re.findall (938 候选 x 3 闸门 = 2800 次全量扫描,
# cProfile 显示单这一项 136s / 总 240s)。改成一遍扫描把所有标识符出现点
# 按上下文分类归档, 之后全部闸门都是字典查表。
#
# 四个上下文类别:
#   qualified  前面是 identifier::  (归属类可判, 用于第一道闸门)
#   dot        前面是 . 或 ->      (第三道闸门, 最宽松)
#   bare       其余                (声明位 / 定义位 / 作为值传递的引用)
# 另外单独记录「后面紧跟 (」的出现点带偏移, 供本类体内调用闸门二分/线性查。
IDENT_RE = re.compile(r"[A-Za-z_]\w*")

qual_owner = {}     # (owner_cls, name) -> count
dot_calls = {}      # name -> count
bare_refs = {}      # name -> count
call_offsets = {}   # (name, path) -> [start_off, ...]

for _p in files:
    _t = text[_p]
    for _m in IDENT_RE.finditer(_t):
        _name = _m.group(0)
        if len(_name) < 3:
            continue
        _i = _m.start() - 1
        while _i >= 0 and _t[_i] in " \t\n":
            _i -= 1
        _c = _t[_i] if _i >= 0 else ""
        _owner = ""
        if _c == ":" and _i >= 1 and _t[_i - 1] == ":":
            _j = _i - 2
            while _j >= 0 and _t[_j] in " \t\n":
                _j -= 1
            _k = _j
            while _k >= 0 and (_t[_k].isalnum() or _t[_k] == "_"):
                _k -= 1
            _owner = _t[_k + 1:_j + 1]
        elif _c == ".":
            dot_calls[_name] = dot_calls.get(_name, 0) + 1
        elif _c == ">" and _i >= 1 and _t[_i - 1] == "-":
            dot_calls[_name] = dot_calls.get(_name, 0) + 1
        else:
            bare_refs[_name] = bare_refs.get(_name, 0) + 1
        if _owner:
            qual_owner[(_owner, _name)] = qual_owner.get((_owner, _name), 0) + 1
        _j = _m.end()
        while _j < len(_t) and _t[_j] in " \t\n":
            _j += 1
        if _j < len(_t) and _t[_j] == "(":
            call_offsets.setdefault((_name, _p), []).append(_m.start())


def dot_count(name):
    return dot_calls.get(name, 0)


def bare_tokens(name):
    return bare_refs.get(name, 0)


# 裸 token 计数必须减去「全仓所有同名方法的声明位与定义位」, 而不是固定减 2。
# 固定减 2 假设了全仓只有这一个方法叫这个名字 —— 只要另一个类也有同名方法
# (VFXShader::enable / ModManager::enable), 别人的声明+定义就会冒充存活证据,
# 把这个方法永久挡在「确证死」之外。这正是 ModManager::enable 漏报的原因。
name_decl_sites = {}
name_def_sites = {}
for (c, n), ds in cands.items():
    name_decl_sites[n] = name_decl_sites.get(n, 0) + len(set(ds))
for (c, n), cnt in defs_by_pair.items():
    name_def_sites[n] = name_def_sites.get(n, 0) + cnt


dead, ambig = [], []
for (cls, name), decls in cands.items():
    q = qual_owner.get((cls, name), 0)
    q -= defs_by_pair.get((cls, name), 0)   # 定义自身不算引用
    q = max(q, 0)
    inner = 0
    for p, ob, cb in bodies_by_class.get(cls, []):
        for off in call_offsets.get((name, p), []):
            if ob <= off < cb:
                inner += 1
    d = dot_count(name)
    if q + inner > 0:
        continue                                # 存活
    # 裸引用桶里混着「声明位」和「作为值传递的引用」两样东西。
    # 定义不必扣 —— 成员函数定义永远是限定形式 (Foo::bar), 走的是 qual 桶。
    # 只扣声明位, 剩下的就是 std::function/std::bind 传参那类引用。
    # 例: EventBus::subscribe(EVT, _on_relic_gained, "AtkEvo") 的第三参
    b = bare_tokens(name) - name_decl_sites[name]
    row = (cls, name, sorted(set(decls)), q, inner, d, max(b, 0))
    (ambig if d > 0 or b > 0 else dead).append(row)

dead.sort()
ambig.sort()

# --debug NAME: 打印单个方法的全部中间量, 用于排查「为什么没被判死」
if "--debug" in sys.argv:
    want = sys.argv[sys.argv.index("--debug") + 1]
    for (cls, name), decls in sorted(cands.items()):
        if name != want:
            continue
        q = len(re.findall(r"\b" + re.escape(cls) + r"::" + re.escape(name) + r"\b",
                           corpus))
        defs = sum(1 for c, n, _p, _a, _b in class_bodies if c == cls and n == name)
        print("DEBUG %-30s decls=%s rawq=%d defs=%d dot=%d bare=%d "
              "name_decls=%d name_defs=%d"
              % (cls + "::" + name, sorted(set(decls)), q, defs, dot_count(name),
                 bare_tokens(name), name_decl_sites[name], name_def_sites[name]))
    print("DEBUG class_bodies entries named %s:" % want)
    for c, n, p, a, b in class_bodies:
        if n == want:
            print("   %s::%s  %s  span=%d" % (c, n, p.replace("/", "\\"), b - a))
    print("DEBUG bare-token hits for %s:" % want)
    for p in files:
        for m in re.finditer(r"\b" + re.escape(want) + r"\b", text[p]):
            ln = text[p].count("\n", 0, m.start()) + 1
            print("   %s:%d" % (p.replace("/", "\\"), ln))

print("=== 死方法扫描 v2 (限定名解析) ===")
print("候选 (类, 方法): %d   cpp 作用域函数体: %d   扫描文件: %d"
      % (len(cands), len(class_bodies), len(files)))
print()
print("--- 确证死 (四种引用形态全为零) : %d ---" % len(dead))
for cls, name, decls, q, inner, d, b in dead:
    print("  %-40s %s" % (cls + "::" + name, decls[0].replace("/", "\\")))
print()
print("--- 待人工 (无限定引用, 但有点号调用或裸 token 碰撞) : %d ---" % len(ambig))
for cls, name, decls, q, inner, d, b in ambig:
    print("  %-40s dot=%-4d bare=%-3d %s"
          % (cls + "::" + name, d, b, decls[0].replace("/", "\\")))
