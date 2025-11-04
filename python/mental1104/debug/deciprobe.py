# deciprobe.py —— Python 3.8+
"""
增强版 deciprobe：在原有 if / while / for / return 的基础上，新增：
- break / continue：在执行点打印
- 推导式 if（list/set/dict/generator comprehensions 内的 if 条件）：打印叶子条件
- try / except / else / finally：进入/结束点打印；except 记录异常类型/别名

其他特性沿用：零开销门卫、fail-open、列宽对齐、site 前缀裁剪、变量实值打印、源码行定位。
"""

import ast
import functools
import inspect
import itertools
import logging
import os
import sys
import textwrap
import threading
import types
from typing import Tuple, Iterable, Optional, Union

# ===== 全局门卫 =====
TRACE_IF_ENABLED = os.getenv("TRACE_IF_ENABLED", "1").lower() not in {"0", "false", "no"}

# ===== 列宽/前缀配置（可环境变量覆盖）=====
TYPE_FIELD_WIDTH = int(os.getenv("TRACE_IF_TYPE_WIDTH", "8"))   # 支持 'continue'(8) 对齐
ID_FIELD_WIDTH = int(os.getenv("TRACE_IF_ID_WIDTH",   "4"))
SITE_FIELD_WIDTH = int(os.getenv("TRACE_IF_SITE_WIDTH", "40"))
FUNC_FIELD_WIDTH = int(os.getenv("TRACE_IF_FUNC_WIDTH", "24"))
HEAD_PREFIX = os.getenv("TRACE_IF_PREFIX", "TRACEIF")
ELLIPSIS_CHAR = os.getenv("TRACE_IF_ELLIPSIS", "…")

# ===== 线程本地 & 组号 =====
_tls = threading.local()
_gid = itertools.count(1)


def __short_repr(v, limit=120):
    try:
        s = repr(v)
    except Exception:
        s = f"<{type(v).__name__}>"
    return s if len(s) <= limit else (s[: limit - 3] + "...")


def __norm_prefix(p: Optional[str]) -> Tuple[str, int]:
    if not p:
        return "", 0
    p2 = os.path.normpath(p)
    if p2 and not p2.endswith(os.sep):
        p2 += os.sep
    return p2, len(p2)


def _fit_field(text: str, width: int, align: str = "left") -> str:
    if width <= 0:
        return str(text)
    s = str(text)
    n = len(s)
    if n == width:
        return s
    if n < width:
        pad = " " * (width - n)
        return (pad + s) if align == "right" else (s + pad)
    e = ELLIPSIS_CHAR
    e_len = len(e)
    if width <= e_len + 1:
        return s[:width]
    head = (width - e_len) // 2
    tail = width - e_len - head
    return s[:head] + e + s[-tail:]

# ===== 统一发射日志（列宽对齐 + site 前缀裁剪）=====


def __emit(logger, level: int, pathname: str, lineno: int, func: str, msg: str, meta: dict):
    log = logger if isinstance(logger, logging.Logger) else logging.getLogger(str(logger) if logger else "trace_if")
    if not log.isEnabledFor(level):
        return
    pref = meta.get("site_prefix", "") or ""
    pref_len = int(meta.get("site_prefix_len", 0) or 0)
    path = pathname[pref_len:] if (pref_len and pathname.startswith(pref)) else pathname

    etype = str(meta.get("etype", "if"))
    if_id = meta.get("if_id")

    type_field = f"type={_fit_field(etype, TYPE_FIELD_WIDTH, 'left')}"
    id_field = f"id={_fit_field('-' if if_id is None else if_id, ID_FIELD_WIDTH, 'right')}"
    site_field = f"site={_fit_field(f'{path}:{lineno}', SITE_FIELD_WIDTH, 'left')}"
    func_field = f"func={_fit_field(func, FUNC_FIELD_WIDTH, 'left')}"
    head = f"{HEAD_PREFIX} | " + " | ".join([type_field, id_field, site_field, func_field])

    try:
        rec = logging.LogRecord(
            name=log.name, level=level,
            pathname=pathname, lineno=int(lineno),
            msg=f"{head} | {msg}", args=(), exc_info=None, func=func
        )
        if meta:
            for k, v in meta.items():
                if k not in rec.__dict__:
                    setattr(rec, k, v)
        log.handle(rec)
    except Exception:
        try:
            log.debug("TRACEIF emit failed", exc_info=True)
        except Exception:
            pass

# ===== 组头/结果（if/while）=====


def __bool_group__(fn, header, meta, logger, level: int, line: int, kind: str):
    gid_prev = getattr(_tls, "gid", None)
    kind_prev = getattr(_tls, "kind", None)
    gid = next(_gid)
    _tls.gid = gid
    _tls.kind = kind
    try:
        __emit(logger, level, meta["file"], line, meta["func"],
               f"[{kind}#{gid}] {header}", {**meta, "etype": kind, "if_id": gid})
        res = fn()
        __emit(logger, level, meta["file"], line, meta["func"],
               f"[{kind}#{gid}] RESULT -> {bool(res)}", {**meta, "etype": kind, "if_id": gid})
        return res
    except Exception:
        # 打印失败不影响求值
        return fn()
    finally:
        _tls.gid = gid_prev
        _tls.kind = kind_prev

# ===== 叶子条件（含变量值）=====


def __mark_cond__(fn, label, meta, logger, level: int, include_names, static_names, line: int):
    val = fn()
    try:
        outer_locals = {}
        try:
            target = meta.get("func", "")
            f = sys._getframe(1)
            while f and f.f_code.co_name != target:
                f = f.f_back
            if f:
                outer_locals = f.f_locals
        except Exception:
            pass
        names = set(include_names) | set(static_names or ())
        kvs = ", ".join(f"{n}={__short_repr(outer_locals[n])}" for n in names if n in outer_locals)
        suffix = f" | vars={kvs}" if kvs else ""
        kind = getattr(_tls, "kind", "if")
        gid = getattr(_tls, "gid", None)
        __emit(logger, level, meta["file"], line, meta["func"], f"[{kind}#{gid}] {label} -> {bool(val)}{suffix}",
               {**meta, "etype": kind, "if_id": gid, "expr": label})
    except Exception:
        pass
    return val

# ===== 推导式 if 条件（类型单独标识为 'comp'）=====


def __mark_comp_if__(fn, label, meta, logger, level: int, line: int):
    val = fn()
    try:
        __emit(logger, level, meta["file"], line, meta["func"], f"[comp] {label} -> {bool(val)}",
               {**meta, "etype": "comp"})  # 无组 id
    except Exception:
        pass
    return val

# ===== for 迭代 =====


def __iter_trace__(iterable, meta, logger, level: int, line: int, header: str):
    gid = next(_gid)
    try:
        __emit(logger, level, meta["file"], line, meta["func"], f"[for#{gid}] {header}",
               {**meta, "etype": "for", "for_id": gid, "if_id": gid, "kind": "for_begin"})
    except Exception:
        pass
    try:
        for idx, item in enumerate(iterable):
            try:
                __emit(logger, level, meta["file"], line, meta["func"], f"[for#{gid}] item[{idx}] -> {__short_repr(item)}",
                       {**meta, "etype": "for", "for_id": gid, "if_id": gid, "kind": "for_item", "index": idx})
            except Exception:
                pass
            yield item
    finally:
        try:
            __emit(logger, level, meta["file"], line, meta["func"], f"[for#{gid}] END",
                   {**meta, "etype": "for", "for_id": gid, "if_id": gid, "kind": "for_end"})
        except Exception:
            pass

# ===== return =====


def __ret_trace__(value, meta, logger, level: int, line: int):
    try:
        __emit(logger, level, meta["file"], line, meta["func"], f"[ret] return -> {__short_repr(value)}",
               {**meta, "etype": "ret"})
    except Exception:
        pass
    return value

# ===== break / continue（在执行点打印）=====


def __loop_event__(kind: str, meta, logger, level: int, line: int):
    try:
        __emit(logger, level, meta["file"], line, meta["func"], f"[{kind}]",
               {**meta, "etype": kind})
    except Exception:
        pass

# ===== try / except / else / finally 标记 =====


def __try_mark__(phase: str, detail: str, meta, logger, level: int, line: int, etype="try"):
    """
    phase: 'try_begin'/'try_end'/'try_else_begin'/'try_else_end'/'finally_begin'/'finally_end'
           or 'except_begin'/'except_end'
    etype: 'try' / 'except' / 'finally'
    """
    try:
        __emit(logger, level, meta["file"], line, meta["func"], f"[{phase}] {detail}",
               {**meta, "etype": etype})
    except Exception:
        pass

# ===== AST 注入 =====


class _IfTracer(ast.NodeTransformer):
    def __init__(self, src, file, qualname, logger_name: str, level_int: int,
                 static_names, start_line: int, site_prefix: str, site_prefix_len: int):
        self.src = src
        self.file = file
        self.qualname = qualname
        self.logger_name = logger_name
        self.level_int = int(level_int)
        self.static_names = tuple(static_names or ())
        self.start_line = int(start_line)
        self.site_prefix = site_prefix
        self.site_prefix_len = int(site_prefix_len)

    def _seg(self, node: ast.AST) -> str:
        s = ast.get_source_segment(self.src, node)
        if isinstance(s, str) and s.strip():
            return s.strip()
        try:
            return ast.unparse(node)  # type: ignore[attr-defined]
        except Exception:
            return "<expr>"

    def _absline(self, rel: Optional[int]) -> int:
        return self.start_line + (int(rel) if rel else 1) - 1

    def _meta_dict(self, rel_lineno: int) -> ast.Dict:
        return ast.Dict(
            keys=[ast.Constant("file"), ast.Constant("func"), ast.Constant("line"),
                  ast.Constant("site_prefix"), ast.Constant("site_prefix_len")],
            values=[ast.Constant(self.file), ast.Constant(self.qualname), ast.Constant(self._absline(rel_lineno)),
                    ast.Constant(self.site_prefix), ast.Constant(self.site_prefix_len)],
        )

    def _logger_value(self): return ast.Constant(self.logger_name)
    def _level_value(self): return ast.Constant(self.level_int)

    def _include_tuple(self, names: Iterable[str]): return ast.Tuple(
        elts=[ast.Constant(n) for n in names], ctx=ast.Load())

    def _collect_names(self, node: ast.AST):
        names = set()

        class V(ast.NodeVisitor):
            def visit_Name(self, n):
                if isinstance(n.ctx, ast.Load):
                    names.add(n.id)
        V().visit(node)
        return tuple(n for n in names if n not in {"True", "False", "None"})

    # —— 叶子打点 ——
    def _mark(self, eval_expr: ast.expr, label_node: ast.AST, rel_lineno: int) -> ast.Call:
        label_text = self._seg(label_node)
        include_names = self._collect_names(label_node)
        abs_line = self._absline(rel_lineno)
        return ast.Call(
            func=ast.Name(id="__mark_cond__", ctx=ast.Load()),
            args=[
                ast.Lambda(args=ast.arguments(posonlyargs=[], args=[], kwonlyargs=[], kw_defaults=[], defaults=[]),
                           body=eval_expr),
                ast.Constant(label_text),
                self._meta_dict(rel_lineno),
                self._logger_value(),
                self._level_value(),
                self._include_tuple(include_names),
                self._include_tuple(self.static_names),
                ast.Constant(abs_line),
            ],
            keywords=[],
        )

    def _instrument_bool(self, node: ast.expr) -> ast.expr:
        if isinstance(node, ast.BoolOp):
            return ast.BoolOp(op=node.op, values=[self._instrument_bool(v) for v in node.values])
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.Not):
            return self._mark(node, node, getattr(node, "lineno", 1))
        if isinstance(node, ast.Compare) and len(node.ops) > 1:
            parts, left = [], node.left
            for op, right in zip(node.ops, node.comparators):
                cmp = ast.Compare(left=left, ops=[op], comparators=[right])
                parts.append(self._mark(cmp, cmp, getattr(cmp, "lineno", node.lineno)))
                left = right
            return ast.BoolOp(op=ast.And(), values=parts)
        if isinstance(node, ast.Compare):
            return self._mark(node, node, getattr(node, "lineno", 1))
        return self._mark(node, node, getattr(node, "lineno", 1))

    # —— if ——
    def visit_If(self, node: ast.If):
        header = self._seg(node.test)
        instrumented = self._instrument_bool(node.test)
        abs_line = self._absline(getattr(node, "lineno", 1))
        node.test = ast.Call(
            func=ast.Name(id="__bool_group__", ctx=ast.Load()),
            args=[
                ast.Lambda(args=ast.arguments(posonlyargs=[], args=[], kwonlyargs=[], kw_defaults=[], defaults=[]),
                           body=instrumented),
                ast.Constant(f"IF: {header}"),
                self._meta_dict(node.lineno),
                self._logger_value(),
                self._level_value(),
                ast.Constant(abs_line),
                ast.Constant("if"),
            ],
            keywords=[],
        )
        self.generic_visit(node)
        return node

    # —— while ——
    def visit_While(self, node: ast.While):
        header = self._seg(node.test)
        instrumented = self._instrument_bool(node.test)
        abs_line = self._absline(getattr(node, "lineno", 1))
        node.test = ast.Call(
            func=ast.Name(id="__bool_group__", ctx=ast.Load()),
            args=[
                ast.Lambda(args=ast.arguments(posonlyargs=[], args=[], kwonlyargs=[], kw_defaults=[], defaults=[]),
                           body=instrumented),
                ast.Constant(f"WHILE: {header}"),
                self._meta_dict(node.lineno),
                self._logger_value(),
                self._level_value(),
                ast.Constant(abs_line),
                ast.Constant("while"),
            ],
            keywords=[],
        )
        self.generic_visit(node)
        return node

    # —— for ——
    def visit_For(self, node: ast.For):
        iter_src = self._seg(node.iter)
        target_src = self._seg(node.target)
        abs_line = self._absline(getattr(node, "lineno", 1))
        header = f"FOR: for {target_src} in {iter_src}"
        node.iter = ast.Call(
            func=ast.Name(id="__iter_trace__", ctx=ast.Load()),
            args=[
                node.iter,
                self._meta_dict(node.lineno),
                self._logger_value(),
                self._level_value(),
                ast.Constant(abs_line),
                ast.Constant(header),
            ],
            keywords=[],
        )
        self.generic_visit(node)
        return node

    # —— return ——
    def visit_Return(self, node: ast.Return):
        abs_line = self._absline(getattr(node, "lineno", 1))
        val = node.value if node.value is not None else ast.Constant(None)
        node.value = ast.Call(
            func=ast.Name(id="__ret_trace__", ctx=ast.Load()),
            args=[val, self._meta_dict(node.lineno), self._logger_value(), self._level_value(), ast.Constant(abs_line)],
            keywords=[],
        )
        return node

    # —— break / continue：替换为 [日志, break/continue] 两条语句 ——
    def visit_Break(self, node: ast.Break):
        abs_line = self._absline(getattr(node, "lineno", 1))
        log_stmt = ast.Expr(value=ast.Call(
            func=ast.Name(id="__loop_event__", ctx=ast.Load()),
            args=[ast.Constant("break"), self._meta_dict(node.lineno), self._logger_value(),
                  self._level_value(), ast.Constant(abs_line)],
            keywords=[],
        ))
        return [log_stmt, node]

    def visit_Continue(self, node: ast.Continue):
        abs_line = self._absline(getattr(node, "lineno", 1))
        log_stmt = ast.Expr(value=ast.Call(
            func=ast.Name(id="__loop_event__", ctx=ast.Load()),
            args=[ast.Constant("continue"), self._meta_dict(node.lineno), self._logger_value(),
                  self._level_value(), ast.Constant(abs_line)],
            keywords=[],
        ))
        return [log_stmt, node]

    # —— 推导式：把 generators[*].ifs[*] 替换为 __mark_comp_if__(lambda: test, "test", ...) ——
    def _instrument_comp_gens(self, gens):
        for gen in gens:
            new_ifs = []
            for cond in gen.ifs:
                abs_line = self._absline(getattr(cond, "lineno", 1))
                new_ifs.append(ast.Call(
                    func=ast.Name(id="__mark_comp_if__", ctx=ast.Load()),
                    args=[
                        ast.Lambda(args=ast.arguments(posonlyargs=[], args=[], kwonlyargs=[], kw_defaults=[], defaults=[]),
                                   body=cond),
                        ast.Constant(self._seg(cond)),
                        self._meta_dict(getattr(cond, "lineno", 1)),
                        self._logger_value(),
                        self._level_value(),
                        ast.Constant(abs_line),
                    ],
                    keywords=[],
                ))
            gen.ifs = new_ifs

    def visit_ListComp(self, node: ast.ListComp):
        self._instrument_comp_gens(node.generators)
        self.generic_visit(node)
        return node

    def visit_SetComp(self, node: ast.SetComp):
        self._instrument_comp_gens(node.generators)
        self.generic_visit(node)
        return node

    def visit_DictComp(self, node: ast.DictComp):
        self._instrument_comp_gens(node.generators)
        self.generic_visit(node)
        return node

    def visit_GeneratorExp(self, node: ast.GeneratorExp):
        self._instrument_comp_gens(node.generators)
        self.generic_visit(node)
        return node

    # —— try/except/else/finally：在块首/尾插入标记 ——
    def visit_Try(self, node: ast.Try):
        # try body
        abs_line = self._absline(getattr(node, "lineno", 1))
        head = ast.Expr(value=ast.Call(
            func=ast.Name(id="__try_mark__", ctx=ast.Load()),
            args=[ast.Constant("try_begin"), ast.Constant(""), self._meta_dict(node.lineno),
                  self._logger_value(), self._level_value(), ast.Constant(abs_line), ast.Constant("try")],
            keywords=[],
        ))
        tail = ast.Expr(value=ast.Call(
            func=ast.Name(id="__try_mark__", ctx=ast.Load()),
            args=[ast.Constant("try_end"), ast.Constant(""), self._meta_dict(node.lineno),
                  self._logger_value(), self._level_value(), ast.Constant(abs_line), ast.Constant("try")],
            keywords=[],
        ))
        node.body.insert(0, head)
        node.body.append(tail)

        # except handlers
        for h in node.handlers:
            t_src = self._seg(h.type) if h.type is not None else "Exception"
            nm = h.name if isinstance(h.name, str) else ""
            detail = f"{t_src}" + (f" as {nm}" if nm else "")
            l = self._absline(getattr(h, "lineno", 1))
            hhead = ast.Expr(value=ast.Call(
                func=ast.Name(id="__try_mark__", ctx=ast.Load()),
                args=[ast.Constant("except_begin"), ast.Constant(detail), self._meta_dict(
                    getattr(h, "lineno", 1)), self._logger_value(), self._level_value(), ast.Constant(l), ast.Constant("except")],
                keywords=[],
            ))
            htail = ast.Expr(value=ast.Call(
                func=ast.Name(id="__try_mark__", ctx=ast.Load()),
                args=[ast.Constant("except_end"), ast.Constant(detail), self._meta_dict(
                    getattr(h, "lineno", 1)), self._logger_value(), self._level_value(), ast.Constant(l), ast.Constant("except")],
                keywords=[],
            ))
            h.body.insert(0, hhead)
            h.body.append(htail)

        # else
        if node.orelse:
            l = self._absline(getattr(node, "lineno", 1))
            ehead = ast.Expr(value=ast.Call(
                func=ast.Name(id="__try_mark__", ctx=ast.Load()),
                args=[ast.Constant("try_else_begin"), ast.Constant(""), self._meta_dict(node.lineno),
                      self._logger_value(), self._level_value(), ast.Constant(l), ast.Constant("try")],
                keywords=[],
            ))
            etail = ast.Expr(value=ast.Call(
                func=ast.Name(id="__try_mark__", ctx=ast.Load()),
                args=[ast.Constant("try_else_end"), ast.Constant(""), self._meta_dict(node.lineno),
                      self._logger_value(), self._level_value(), ast.Constant(l), ast.Constant("try")],
                keywords=[],
            ))
            node.orelse.insert(0, ehead)
            node.orelse.append(etail)

        # finally
        if node.finalbody:
            l = self._absline(getattr(node, "lineno", 1))
            fhead = ast.Expr(value=ast.Call(
                func=ast.Name(id="__try_mark__", ctx=ast.Load()),
                args=[ast.Constant("finally_begin"), ast.Constant(""), self._meta_dict(node.lineno),
                      self._logger_value(), self._level_value(), ast.Constant(l), ast.Constant("finally")],
                keywords=[],
            ))
            ftail = ast.Expr(value=ast.Call(
                func=ast.Name(id="__try_mark__", ctx=ast.Load()),
                args=[ast.Constant("finally_end"), ast.Constant(""), self._meta_dict(node.lineno),
                      self._logger_value(), self._level_value(), ast.Constant(l), ast.Constant("finally")],
                keywords=[],
            ))
            node.finalbody.insert(0, fhead)
            node.finalbody.append(ftail)

        self.generic_visit(node)
        return node

# ===== 构建注入版函数（exec 产物，保留闭包）=====


def _build_instrumented_function(func, logger, level_int: int, vars_cfg=("auto",), site_prefix: Optional[str] = None):
    try:
        src = inspect.getsource(func)
        file = inspect.getsourcefile(func) or "<string>"
        _, start_line = inspect.getsourcelines(func)
    except OSError:
        return None

    site_pref, site_pref_len = __norm_prefix(site_prefix)
    src_d = textwrap.dedent(src)
    mod = ast.parse(src_d)
    qualname = getattr(func, "__qualname__", func.__name__)
    logger_name = logger.name if isinstance(logger, logging.Logger) else ("trace_if" if logger is None else str(logger))
    static_names = tuple(n for n in (vars_cfg or ()) if n != "auto")

    tr = _IfTracer(src_d, file, qualname, logger_name, int(level_int),
                   static_names, start_line, site_pref, site_pref_len)
    mod = tr.visit(mod)
    ast.fix_missing_locations(mod)

    g = func.__globals__
    g.update({
        "__bool_group__": __bool_group__,
        "__mark_cond__": __mark_cond__,
        "__mark_comp_if__": __mark_comp_if__,
        "__iter_trace__": __iter_trace__,
        "__ret_trace__": __ret_trace__,
        "__loop_event__": __loop_event__,
        "__try_mark__": __try_mark__,
        "__emit": __emit,
        "logging": logging,
    })
    ns = {}
    try:
        code = compile(mod, filename=file, mode="exec")
        exec(code, g, ns)
        newf = ns.get(func.__name__)
        if not isinstance(newf, types.FunctionType):
            return None
        newf.__defaults__ = func.__defaults__
        newf.__kwdefaults__ = func.__kwdefaults__
        try:
            newf.__dict__.update(getattr(func, "__dict__", {}))
        except Exception:
            pass
        newf.__module__ = func.__module__
        newf.__qualname__ = func.__qualname__
        return functools.update_wrapper(newf, func)
    except Exception:
        try:
            lg = logger if isinstance(logger, logging.Logger) else logging.getLogger(logger or "trace_if")
            lg.debug("TRACEIF build failed", exc_info=True)
        except Exception:
            pass
        return None

# ===== 装饰器（双版本门卫）=====


def trace_if(logger: Union[logging.Logger, str, None] = None,
             level: Union[int, str] = logging.DEBUG,
             enabled: bool = True,
             vars: Tuple[str, ...] = ("auto",),
             site_prefix: Optional[str] = None):
    level_int = level if isinstance(level, int) else getattr(logging, str(level).upper(), logging.DEBUG)

    def deco(func):
        if not enabled:
            return func
        orig = func
        traced = _build_instrumented_function(func, logger, level_int, vars, site_prefix)
        gate_logger = logger if isinstance(logger, logging.Logger) else logging.getLogger(logger or "trace_if")

        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            if not TRACE_IF_ENABLED or not gate_logger.isEnabledFor(level_int) or traced is None:
                return orig(*args, **kwargs)
            try:
                return traced(*args, **kwargs)
            except Exception:
                try:
                    gate_logger.warning("TRACEIF disabled at runtime due to error; falling back to orig", exc_info=True)
                except Exception:
                    pass
                return orig(*args, **kwargs)
        return wrapper
    return deco


def deciprobe(*args, **kwargs):
    return trace_if(*args, **kwargs)


__all__ = ["deciprobe", "trace_if"]
