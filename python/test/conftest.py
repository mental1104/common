#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import pathlib
import sys
import types

import pytest

# Ensure commonly used plugins are loaded for all tests.
pytest_plugins = ("pytest_mock",)

# Make project importable from tests
PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[1]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

# Stub optional dependencies pulled in by the auto-generated mental1104 __init__
if "pypdf" not in sys.modules:
    stub = types.ModuleType("pypdf")

    class _StubReader:
        def __init__(self, *_args, **_kwargs):
            self.pages = []

    class _StubWriter:
        def add_page(self, *_args, **_kwargs):
            return None

        def write(self, *_args, **_kwargs):
            return None

    stub.PdfReader = _StubReader
    stub.PdfWriter = _StubWriter
    sys.modules["pypdf"] = stub

from mental1104.common.i18n.tools.compile import po_text_to_mo_bytes


@pytest.fixture
def write_mo_from_po_text():
    """
    Helper fixture to compile inline PO text into MO bytes and persist them
    under the standard `<root>/<locale>/LC_MESSAGES/<domain>.mo` layout.
    """

    def _write(tmp_mo_root: pathlib.Path, locale: str, domain: str, po_text: str) -> pathlib.Path:
        mo_bytes = po_text_to_mo_bytes(po_text)
        mo_path = pathlib.Path(tmp_mo_root) / locale / "LC_MESSAGES" / f"{domain}.mo"
        mo_path.parent.mkdir(parents=True, exist_ok=True)
        mo_path.write_bytes(mo_bytes)
        return mo_path

    return _write
