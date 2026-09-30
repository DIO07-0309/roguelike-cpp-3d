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
        ob = t.find("{", m.end() - 1)
        if ob < 0:
            continue
        if t.rfind(";", 0, ob) > t.rfind("(", 0, ob):   # 是声明不是定义
            continue
        cb = match_brace(t, ob)
        if cb < 0:
            continue
        class_bodies.append((cls, name, p, ob, cb))

# 全仓 [.]name( 与 ->name( 形式的调用 (最宽松的一层)
dot_cache = {}


def dot_count(name):
    if name not in dot_cache:
        dot_cache[name] = len(re.findall(
            r"[.>]" + re.escape(name) + r"\s*\(", corpus))
    return dot_cache[name]


# ---------------------------------------------------------------- 判定
# 第四道闸门: 名字作为「值」传递的引用 (EventBus::subscribe(..., _handler, ...),
# std::bind, std::function 参数) 既没有 Class:: 前缀也没有紧随的 (,
# 前面三道闸门全部看不见。所以再数一遍裸 token, 减去声明与定义各 1 次,
# 还剩任何出现就降级为「待人工」, 绝不判死。
def bare_tokens(name):
    return len(re.findall(r"\b" + re.escape(name) + r"\b", corpus))


dead, ambig = [], []
for (cls, name), decls in cands.items():
    q = len(re.findall(r"\b" + re.escape(cls) + r"::" + re.escape(name) + r"\b",
                       corpus))
    defs = sum(1 for c, n, _p, _a, _b in class_bodies if c == cls and n == name)
    q -= defs                                   # 定义自身不算引用
    inner = 0
    for c, n, p, a, b in class_bodies:
        if c == cls:
            inner += len(re.findall(r"\b" + re.escape(name) + r"\s*\(",
                                    text[p][a:b]))
    d = dot_count(name)
    if q + inner > 0:
        continue                                # 存活
    b = bare_tokens(name) - d - 2               # 减去调用形态与「声明+定义」
    row = (cls, name, sorted(set(decls)), q, inner, d, max(b, 0))
    (ambig if d > 0 or b > 0 else dead).append(row)

dead.sort()
ambig.sort()
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
