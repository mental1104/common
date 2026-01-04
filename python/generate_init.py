# 脚本概述：递归扫描 mental1104 包, 收集公开符号, 区分安全/风险模块, 生成带直接导入与惰性导出的 __init__.py;
# 处理重名决策与 import * 的 __all__, 并生成惰性 __getattr__/__dir__ 实现
import ast
import os
from pathlib import Path
from typing import Dict, List, Set, Tuple

PKG = "mental1104"
# resolve() 获取当前文件绝对路径; parent 为其所在目录; "/" 左为 Path/PurePath, 右为路径片段(str/Path-like), 用于拼接; 最后附加包名
BASE_PACKAGE = Path(__file__).resolve().parent / PKG
INIT_FILE = BASE_PACKAGE / "__init__.py"


class ModuleInfo:
    # __slots__ 限定实例属性集合, 禁止动态新增并减少拼写错误
    # 定义 __slots__ 默认移除实例 __dict__, 除非 slots 包含 '__dict__'
    # 或继承自仍保留 __dict__ 的基类; 去掉 __dict__ 可节省内存并提升属性访问
    __slots__ = ("file", "module", "names", "risk")

    def __init__(self, module: str, file: Path, names: List[str], risk: bool):
        self.module = module
        self.file = file
        self.names = names
        self.risk = risk


def collect_public_names(file_path: Path) -> List[str]:
    names: List[str] = []
    with file_path.open("r", encoding="utf-8") as f:
        try:
            # ast.parse 每次解析一段源码文本, 通常对应单个文件
            # 多个文件需多次调用并传入各自源码
            tree = ast.parse(
                f.read(), filename=str(file_path)
            )  # 解析源码为 AST, filename 便于报错定位, tree 为 ast.Module 根节点
        except SyntaxError:
            return []
    # tree.body 是 List[ast.stmt], 常见元素有 FunctionDef、
    # ClassDef、Assign、Import 等语句节点
    for node in tree.body:  # 遍历模块顶层语句, node 是 ast.AST 子类实例, 挑选公开的类/函数/变量名
        if isinstance(
            node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)
        ):  # 判断顶层同步/异步函数或类
            name = getattr(node, "name", None)  # 此处取到的是类名或函数名字符串
            if name and not name.startswith("_"):
                names.append(name)
        elif isinstance(node, ast.Assign) and node.targets:
            t0 = node.targets[0]  # 处理赋值语句, 取首个目标
            if isinstance(t0, ast.Name):  # 顶层赋值视为模块级“全局”变量
                name = t0.id  # 对 Name 节点, 标识符存放在 id 字段
                if name and not name.startswith("_"):
                    names.append(name)
    # set 去重：同名函数/变量重复定义或同一名字在不同分支重复出现时避免重复导出
    return sorted(set(names))  # 汇总顶层同步/异步函数、类与全局变量, 忽略以下划线开头


def detect_risk_imports(file_path: Path) -> bool:
    """检测该模块是否在顶层 import 了 mental1104（或 from mental1104 import ...）。
    这类模块作为『风险模块』, 避免在包初始化时直接导入, 以规避循环依赖。"""
    with file_path.open("r", encoding="utf-8") as f:
        try:
            tree = ast.parse(f.read(), filename=str(file_path))
        except SyntaxError:
            return False
    for node in tree.body:
        if isinstance(node, ast.Import):  # 检测 import xxx 语句
            for alias in node.names:
                if alias.name == PKG or alias.name.startswith(PKG + "."):
                    return True
        elif (
            isinstance(node, ast.ImportFrom)
            and node.module
            and (node.module == PKG or node.module.startswith(PKG + "."))
        ):  # 检测 from xxx import ... 语句
            return True
    return False


def walk_modules(base_dir: Path) -> List[ModuleInfo]:
    out: List[ModuleInfo] = []
    for dirpath, dirnames, filenames in os.walk(
        base_dir
    ):  # os.walk 返回迭代器, 逐层递归遍历目录, 遍历完覆盖全部子树但不会一次性列出全部文件
        dirnames.sort()  # 确保目录遍历顺序稳定, 生成结果可复现
        py_files = sorted(
            f for f in filenames if f.endswith(".py") and f != "__init__.py"
        )  # 筛选当前目录下除 __init__.py 的 py 文件并排序, 保证处理顺序稳定
        for filename in (
            py_files
        ):  # 内层循环处理当前目录的每个 .py 文件, 外层遍历目录、内层遍历文件形成双重循环
            file_path = Path(dirpath) / filename
            # 例如 base_dir=/home/.../mental1104, file_path=/home/.../mental1104/foo
            # /bar.py, relative_to 后 rel=foo/bar
            rel = file_path.relative_to(base_dir).with_suffix(
                ""
            )  # relative_to 计算相对 base_dir 的子路径, 去掉后缀用于模块名组装
            # rel 为 Path, as_posix() 输出用/的 POSIX 字符串
            # Windows 也支持该方法, 仍返回带/的字符串, 便于后续替换成模块分隔符
            # 若需反斜杠, 可用 str(rel)/os.fspath(rel), Windows 自动 '\'
            module_path = f"{PKG}." + rel.as_posix().replace("/", ".")
            names = collect_public_names(file_path)  # AST 收集公开符号列表
            # 无公开符号的模块直接跳过, 避免在 __init__.py 导出空条目
            if not names:
                continue
            risk = detect_risk_imports(file_path)  # 判断模块顶层是否导入 mental1104, 用于标记风险
            out.append(ModuleInfo(module_path, file_path, names, risk))
    # 模块路径排序, 跨平台稳定
    out.sort(key=lambda m: m.module)  # 默认按模块名升序; 若需降序可加 reverse=True
    return out


def choose_providers(mods: List[ModuleInfo]) -> Tuple[Dict[str, str], Dict[str, List[str]]]:
    """跨模块重名稳定决策：优先『路径更浅』的模块, 其次字典序。返回 name->module 的映射。"""
    export_map: Dict[str, str] = {}
    dups: Dict[str, List[str]] = {}

    def rank(modname: str):  # 返回元组(层级数, 名字), 用于比较模块优先级：层级浅优先, 其次字典序
        return (modname.count("."), modname)

    for mi in mods:
        for n in mi.names:
            if n not in export_map:
                export_map[n] = mi.module
            else:
                prev = export_map[n]  # 已有提供者
                chosen = min([prev, mi.module], key=rank)  # 选层级更浅/字典序更小的模块
                export_map[n] = chosen  # 更新为最终提供者
                dups.setdefault(
                    n, []
                )  # 记录重名模块列表, 便于生成提示注释, export_map 已定胜者但 dups 保留所有提供者
                if prev not in dups[n]:
                    dups[n].append(prev)  # 追加旧提供者
                if mi.module not in dups[n]:
                    dups[n].append(mi.module)  # 追加当前模块, 保留所有候选
    return export_map, dups


def generate_init(mods: List[ModuleInfo]) -> str:
    # 计算全局提供者（重名稳定决策）
    export_map, dups = choose_providers(mods)

    # 将 name 分配回对应模块, 只保留被选中的提供者
    chosen_per_mod: Dict[str, List[str]] = {}
    for name, mod in export_map.items():  # 以模块为 key, 把最终导出名回填到该模块列表
        chosen_per_mod.setdefault(mod, []).append(name)

    # 按模块内部名字字典序
    for mod in list(chosen_per_mod.keys()):
        chosen_per_mod[mod] = sorted(chosen_per_mod[mod])  # 对每个模块内的导出名排序

    # 区分『安全模块』与『风险模块』
    risk_modules: Set[str] = {mi.module for mi in mods if mi.risk}
    safe_modules: Set[str] = set(chosen_per_mod.keys()) - risk_modules

    # 生成直接导入行（仅安全模块）
    direct_import_lines: List[str] = []
    for mod in sorted(safe_modules):
        names = chosen_per_mod[mod]
        if names:
            direct_import_lines.append(f"from {mod} import {', '.join(names)}")

    # 惰性映射（仅风险模块）
    lazy_items = []
    for mod in sorted(risk_modules):
        names = chosen_per_mod.get(mod, [])
        lazy_items.extend((n, mod) for n in names)

    # 重名注释
    dup_lines: List[str] = []  # 重名信息只写入注释, 不影响导出内容
    if dups:
        dup_lines.append(
            "# NOTE: Duplicate names detected; chosen provider by (shallower module path -> lexicographic):"
        )  # 仅生成说明注释
        for name in sorted(dups.keys()):
            chosen = export_map[name]
            others = ", ".join(sorted(set(dups[name]) - {chosen}))
            dup_lines.append(f"#   {name}: {chosen}  (others: {others})")

    # __all__（安全+风险）统一字典序
    # __all__ 控制 import *（如 from mental1104 import *）导出范围, 只包含列表中符号
    # 若不设置, import * 仅导出当前已加载的全局, 不含惰性符号
    all_names = sorted(export_map.keys())

    lines = [
        "# Auto-generated by generate_init.py (deterministic; hybrid: direct + lazy)",
        "# Do not edit manually.",
        *dup_lines,
        "",
        # 安全模块已上面直接导入, 访问时不会触发 __getattr__
        "# ---- Direct imports (safe modules; no mental1104 top-level imports) ----",
        *direct_import_lines,
        "",
        "# ---- Lazy exports (risky modules; may import mental1104 at top-level) ----",
        "_EXPORT_MAP = {",
        *[f"    '{name}': '{mod}'," for name, mod in sorted(lazy_items)],
        "}",
        "",
        "__all__ = [",
        *[f"    '{name}'," for name in all_names],
        "]",
        "",
        # 生成 __getattr__/__dir__：惰性加载风险符号并缓存, dir() 返回全局+__all__
        "# 惰性导入降低循环引用概率, 但模块内部仍可能互相 import 导致循环",
        "def __getattr__(name):",  # 访问模块属性（如 from mental1104 import foo）时惰性导入风险模块并缓存, 避免循环依赖
        "    # PEP 562: lazy attribute access for risky modules & fallback",
        "    try:",
        "        modname = _EXPORT_MAP[name]",
        "    except KeyError:",
        "        raise AttributeError(f'module {__name__} has no attribute {name!r}') from None",
        "    import importlib, types",
        "    try:",
        "        mod = importlib.import_module(modname)",
        "    except ImportError as exc:",
        '        raise ImportError(f"{modname} is not available; optional dependency may be missing or unsupported on this platform") from exc',
        "    obj = getattr(mod, name, None)",
        "    if obj is None or isinstance(obj, types.ModuleType):",
        "        # 兼容包结构：尝试从子模块 modname.name 再取一次",
        "        try:",
        "            sub = importlib.import_module(f'{modname}.{name}')",
        "        except Exception as _e:  # noqa: F841",
        "            raise AttributeError(f'{modname} has no attribute {name!r}') from None",
        "        obj = getattr(sub, name, None)",
        "        if obj is None:",
        "            raise AttributeError(f'{modname}.{name} has no attribute {name!r}') from None",
        "    globals()[name] = obj  # cache：写回模块全局（由 globals() 返回的字典）以便下次直接取",
        "    return obj",
        "",
        "def __dir__():",  # 交互探索或 IDE 补全时用户会调用 dir(mental1104)
        "    return sorted(list(globals().keys()) + list(__all__))",
        "",
    ]
    return "\n".join(lines)


def main():
    mods = walk_modules(BASE_PACKAGE)
    content = generate_init(mods)
    INIT_FILE.write_text(
        content, encoding="utf-8"
    )  # Path.write_text 写入文本文件, 自动打开/覆盖目标, 指定编码
    total = sum(len(mi.names) for mi in mods)
    risky = sum(1 for mi in mods if mi.risk)
    print(f"""✅ Generated {INIT_FILE}
- modules: {len(mods)} (risky: {risky})
- total exports detected: {total}
- direct imports: {sum(1 for mi in mods if mi.module not in {m.module for m in mods if m.risk})}
""")


if __name__ == "__main__":
    main()
