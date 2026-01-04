import pytest

from mental1104.common.i18n import FileMoProvider, I18n, locale_context
from mental1104.common.i18n.tools.check import check_po_tree
from mental1104.common.i18n.tools.compile import compile_po_tree


def test_compile_po_tree_and_runtime(tmp_path):
    po_root = tmp_path / "po"
    mo_root = tmp_path / "mo"
    domain = "ui"
    po_path = po_root / "en" / "LC_MESSAGES" / f"{domain}.po"
    po_path.parent.mkdir(parents=True, exist_ok=True)
    po_path.write_text(
        """
msgid ""
msgstr ""
"Language: en\\n"
"Plural-Forms: nplurals=2; plural=(n != 1);\\n"

msgid "Hello {name}"
msgstr "Hi {name}"

msgid "Apple"
msgid_plural "Apples"
msgstr[0] "Apple"
msgstr[1] "Apples"
""".strip(),
        encoding="utf-8",
    )

    compile_po_tree(po_root, mo_root, use_msgfmt_if_available=False)

    i18n = I18n(FileMoProvider(mo_root), default_locale="en", supported={"en"})
    with locale_context("en"):
        assert i18n.t("Hello {name}", domain=domain) == "Hi {name}"
        assert i18n.tn("Apple", "Apples", 2, domain=domain) == "Apples"


def test_check_po_tree_strict_failure(tmp_path):
    po_root = tmp_path / "po"
    po_path = po_root / "en" / "LC_MESSAGES" / "ui.po"
    po_path.parent.mkdir(parents=True, exist_ok=True)
    po_path.write_text(
        """
msgid ""
msgstr ""
"Language: en\\n"

#, fuzzy
msgid "Hello {name}"
msgstr "Hi {username}"

msgid "Empty"
msgstr ""
""".strip(),
        encoding="utf-8",
    )

    with pytest.raises(ValueError):
        check_po_tree(po_root, strict=True)
