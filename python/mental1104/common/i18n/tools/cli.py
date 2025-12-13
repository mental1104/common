"""
Command-line entry point for i18n tools.
"""

from __future__ import annotations

import argparse
import sys

from .check import check_po_tree
from .compile import compile_po_tree


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="python -m mental1104.common.i18n.tools")
    sub = parser.add_subparsers(dest="command", required=True)

    compile_parser = sub.add_parser("compile", help="Compile PO tree into MO files")
    compile_parser.add_argument("po_root", help="Path to PO root")
    compile_parser.add_argument("mo_root", help="Path to MO output root")
    compile_parser.add_argument("--domains", nargs="*", help="Optional domain filter")
    compile_parser.add_argument("--locales", nargs="*", help="Optional locale filter")
    compile_parser.add_argument(
        "--no-msgfmt",
        action="store_true",
        help="Force pure Python compilation even if msgfmt exists",
    )

    check_parser = sub.add_parser("check", help="Validate PO files for consistency")
    check_parser.add_argument("po_root", help="Path to PO root")
    check_parser.add_argument(
        "--no-strict",
        action="store_true",
        help="Emit warnings instead of failing on fuzzy/empty translations",
    )

    return parser


def main(argv: list[str] | None = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(argv)
    if args.command == "compile":
        compile_po_tree(
            args.po_root,
            args.mo_root,
            domains=set(args.domains) if args.domains else None,
            locales=set(args.locales) if args.locales else None,
            use_msgfmt_if_available=not args.no_msgfmt,
        )
        return 0
    if args.command == "check":
        warnings = check_po_tree(args.po_root, strict=not args.no_strict)
        if warnings:
            for w in warnings:
                print(f"WARNING: {w}")
        return 0
    parser.print_help()
    return 1


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
