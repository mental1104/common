# python/generate_init.py
import os
import ast
from pathlib import Path
from typing import Dict, List, Tuple, Set

PKG = "mental1104"
BASE_PACKAGE = Path(__file__).resolve().parent / PKG
INIT_FILE = BASE_PACKAGE / "__init__.py"

class ModuleInfo:
    __slots__ = ("module", "file", "names", "risk")
    def __init__(self, module: str, file: Path, names: List[str], risk: bool):
        self.module = module
        self.file = file
        self.names = names
        self.risk = risk

def collect_public_names(file_path: Path) -> List[str]:
    names: List[str] = []
    with file_path.open("r", encoding="utf-8") as f:
        try:
            tree = ast.parse(f.read(), filename=str(file_path))
        except SyntaxError:
            return []
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
            name = getattr(node, "name", None)
            if name and not name.startswith("_"):
                names.append(name)
        elif isinstance(node, ast.Assign) and node.targets:
            t0 = node.targets[0]
            if isinstance(t0, ast.Name):
                name = t0.id
                if name and not name.startswith("_"):
                    names.append(name)
    return sorted(set(names))

def detect_risk_imports(file_path: Path) -> bool:
    """检测该模块是否在顶层 import 了 mental1104（或 from mental1104 import ...）。
       这类模块作为『风险模块』，避免在包初始化时直接导入，以规避循环依赖。"""
    with file_path.open("r", encoding="utf-8") as f:
        try:
            tree = ast.parse(f.read(), filename=str(file_path))
        except SyntaxError:
            return False
    for node in tree.body:
        if isinstance(node, ast.Import):
            for alias in node.names:
                if alias.name == PKG or alias.name.startswith(PKG + "."):
                    return True
        elif isinstance(node, ast.ImportFrom):
            if node.module and (node.module == PKG or node.module.startswith(PKG + ".")):
                return True
    return False

def walk_modules(base_dir: Path) -> List[ModuleInfo]:
    out: List[ModuleInfo] = []
    for dirpath, dirnames, filenames in os.walk(base_dir):
        dirnames.sort()
        py_files = sorted(f for f in filenames if f.endswith(".py") and f != "__init__.py")
        for filename in py_files:
            file_path = Path(dirpath) / filename
            rel = file_path.relative_to(base_dir).with_suffix("")
            module_path = f"{PKG}." + rel.as_posix().replace("/", ".")
            names = collect_public_names(file_path)
            if not names:
                continue
            risk = detect_risk_imports(file_path)
            out.append(ModuleInfo(module_path, file_path, names, risk))
    # 模块路径排序，跨平台稳定
    out.sort(key=lambda m: m.module)
    return out

def choose_providers(mods: List[ModuleInfo]) -> Tuple[Dict[str, str], Dict[str, List[str]]]:
    """跨模块重名稳定决策：优先『路径更浅』的模块，其次字典序。返回 name->module 的映射。"""
    export_map: Dict[str, str] = {}
    dups: Dict[str, List[str]] = {}
    def rank(modname: str):
        return (modname.count("."), modname)
    for mi in mods:
        for n in mi.names:
            if n not in export_map:
                export_map[n] = mi.module
            else:
                prev = export_map[n]
                chosen = min([prev, mi.module], key=rank)
                export_map[n] = chosen
                dups.setdefault(n, [])
                if prev not in dups[n]:
                    dups[n].append(prev)
                if mi.module not in dups[n]:
                    dups[n].append(mi.module)
    return export_map, dups

def generate_init(mods: List[ModuleInfo]) -> str:
    # 计算全局提供者（重名稳定决策）
    export_map, dups = choose_providers(mods)

    # 将 name 分配回对应模块，只保留被选中的提供者
    chosen_per_mod: Dict[str, List[str]] = {}
    for name, mod in export_map.items():
        chosen_per_mod.setdefault(mod, []).append(name)

    # 按模块内部名字字典序
    for mod in list(chosen_per_mod.keys()):
        chosen_per_mod[mod] = sorted(chosen_per_mod[mod])

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
        for n in names:
            lazy_items.append((n, mod))

    # __all__（安全+风险）统一字典序
    all_names = sorted(export_map.keys())

    # 重名注释
    dup_lines: List[str] = []
    if dups:
        dup_lines.append("# NOTE: Duplicate names detected; chosen provider by (shallower module path -> lexicographic):")
        for name in sorted(dups.keys()):
            chosen = export_map[name]
            others = ", ".join(sorted(set(dups[name]) - {chosen}))
            dup_lines.append(f"#   {name}: {chosen}  (others: {others})")

    lines = [
        "# Auto-generated by generate_init.py (deterministic; hybrid: direct + lazy)",
        "# Do not edit manually.",
        *dup_lines,
        "",
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
        "def __getattr__(name):",
        "    # PEP 562: lazy attribute access for risky modules & fallback",
        "    try:",
        "        modname = _EXPORT_MAP[name]",
        "    except KeyError:",
        "        raise AttributeError(f'module {__name__} has no attribute {name!r}') from None",
        "    import importlib, types",
        "    mod = importlib.import_module(modname)",
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
        "    globals()[name] = obj  # cache",
        "    return obj",
        "",
        "def __dir__():",
        "    return sorted(list(globals().keys()) + list(__all__))",
        "",
    ]
    return "\n".join(lines)

def main():
    mods = walk_modules(BASE_PACKAGE)
    content = generate_init(mods)
    INIT_FILE.write_text(content, encoding="utf-8")
    total = sum(len(mi.names) for mi in mods)
    risky = sum(1 for mi in mods if mi.risk)
    print(f"""✅ Generated {INIT_FILE}
- modules: {len(mods)} (risky: {risky})
- total exports detected: {total}
- direct imports: {sum(1 for mi in mods if mi.module not in {m.module for m in mods if m.risk})}
""")

if __name__ == "__main__":
    main()