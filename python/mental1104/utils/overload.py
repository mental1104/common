# dispatch_for.py
import inspect
import sys
from typing import Any, Callable, Dict, Tuple, Type

_Pattern = Tuple[Type[Any], ...]


def dispatch_for(*decorator_args: Any) -> Any:
    """
    Unified decorator for runtime multi-argument overloading.

    Two usages:

    1) On methods: register an overload for a type pattern (T1, T2, ...):

        @dispatch_for(T1, T2, ...)
        def handler(self, a1: T1, a2: T2, ...):
            ...

       The pattern matches the runtime types of positional arguments:
         key = (type(a1), type(a2), ...)

       Only exact matches are used (no implicit conversions, no "best match").

    2) On a class: bind the class as implementation of an entry function:

        def op(...):
            raise NotImplementedError

        @dispatch_for(op)
        class OpImpl:
            ...

       After decoration:
         - op(...) is replaced with a dispatcher that:
             * chooses the method by types of positional args
             * falls back to Impl.default(...) if defined
             * otherwise raises TypeError
         - OpImpl().__call__(...) also uses the same dispatcher.
    """

    # CASE 1: @dispatch_for(T1, T2, ...) on a METHOD
    if decorator_args and all(isinstance(arg, type) for arg in decorator_args):
        pattern: _Pattern = tuple(decorator_args)

        def method_decorator(fn: Callable[..., Any]) -> Callable[..., Any]:
            patterns = getattr(fn, "__dispatch_patterns__", None)
            if patterns is None:
                patterns = []
                setattr(fn, "__dispatch_patterns__", patterns)
            patterns.append(pattern)
            return fn

        return method_decorator

    # CASE 2: @dispatch_for(entry_func) on a CLASS
    if len(decorator_args) == 1 and inspect.isfunction(decorator_args[0]):
        entry_func = decorator_args[0]

        def class_decorator(cls: Type[Any]) -> Type[Any]:
            # 1) Collect overloads: {arity -> {pattern -> function}}
            registry: Dict[int, Dict[_Pattern, Callable[..., Any]]] = {}

            for name, attr in cls.__dict__.items():
                patterns = getattr(attr, "__dispatch_patterns__", None)
                if not patterns:
                    continue

                for pat in patterns:
                    arity = len(pat)
                    arity_map = registry.setdefault(arity, {})
                    if pat in arity_map:
                        raise TypeError(
                            f"Duplicate overload for pattern {pat} "
                            f"on method {attr.__qualname__}"
                        )
                    arity_map[pat] = attr

            # 2) Single instance (stateless pattern; adjust if you need per-call state)
            instance = cls()

            # Build a trie per arity to avoid creating type tuples at call time.
            trie_registry: Dict[int, Dict[Type[Any], Any]] = {}
            arity_lookup: Dict[int, Callable[[tuple[Any, ...]], Callable[..., Any] | None]] = {}

            def _make_trie_lookup(root: Dict[Type[Any], Any], arity: int):
                if arity == 1:
                    def _lookup(args: tuple[Any, ...]):
                        return root.get(args[0].__class__)
                elif arity == 2:
                    def _lookup(args: tuple[Any, ...]):
                        node = root.get(args[0].__class__)
                        if node is None:
                            return None
                        return node.get(args[1].__class__)
                elif arity == 3:
                    def _lookup(args: tuple[Any, ...]):
                        node = root.get(args[0].__class__)
                        if node is None:
                            return None
                        node = node.get(args[1].__class__)
                        if node is None:
                            return None
                        return node.get(args[2].__class__)
                else:
                    def _lookup(args: tuple[Any, ...]):
                        node: Any = root
                        last_index = len(args) - 1
                        for idx, arg in enumerate(args):
                            node = node.get(arg.__class__)
                            if node is None:
                                return None
                            if idx == last_index:
                                return node
                        return None
                return _lookup

            for arity, arity_map in registry.items():
                root: Dict[Type[Any], Any] = {}
                for pat, fn in arity_map.items():
                    node = root
                    for t in pat[:-1]:
                        node = node.setdefault(t, {})
                    node[pat[-1]] = fn
                trie_registry[arity] = root
                arity_lookup[arity] = _make_trie_lookup(root, arity)

            _arity_lookup_get = arity_lookup.get
            _default = getattr(instance, "default", None)

            def dispatcher(*args: Any, **kwargs: Any) -> Any:
                lookup = _arity_lookup_get(len(args))
                if lookup is None:
                    # no overloads registered for this arg count
                    if _default is not None:
                        return _default(*args, **kwargs)
                    raise TypeError(
                        f"No overloads for {entry_func.__name__} "
                        f"with {len(args)} positional arguments"
                    )

                fn = lookup(args)
                if fn is not None:
                    return fn(instance, *args, **kwargs)

                # fallback to default(self, *args, **kwargs) if present
                if _default is not None:
                    return _default(*args, **kwargs)

                raise TypeError(
                    f"No overload for {entry_func.__name__} "
                    f"with argument types {[type(a) for a in args]}"
                )

            # 3) Keep name/doc so tools still see the original symbol
            dispatcher.__name__ = entry_func.__name__
            dispatcher.__doc__ = entry_func.__doc__

            # 4) Replace the entry function in its module with the dispatcher
            mod = sys.modules[entry_func.__module__]
            mod.__dict__[entry_func.__name__] = dispatcher

            # 5) Also allow Impl()(...) if you ever want to call the class instance directly.
            #    We ignore the bound self here so we can keep using the singleton instance above.
            def _instance_call(_self: Any, *args: Any, **kwargs: Any) -> Any:
                return dispatcher(*args, **kwargs)

            setattr(cls, "__call__", _instance_call)

            return cls

        return class_decorator

    raise TypeError(
        "dispatch_for must be used either as:\n"
        "  @dispatch_for(T1, T2, ...) on a method, or\n"
        "  @dispatch_for(entry_function) on a class."
    )
