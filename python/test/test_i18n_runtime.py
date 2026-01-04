from mental1104.common.i18n import FileMoProvider, I18n, locale_context


def test_i18n_runtime_translations(tmp_path, write_mo_from_po_text):
    mo_root = tmp_path / "mo"
    domain = "ui"

    zh_po = """
msgid ""
msgstr ""
"Language: zh\\n"

msgid "Hello"
msgstr "你好"

msgctxt "btn"
msgid "Save"
msgstr "保存"
""".strip()

    en_po = """
msgid ""
msgstr ""
"Language: en\\n"

msgid "Hello"
msgstr "Hello"

msgctxt "btn"
msgid "Save"
msgstr "Save"
""".strip()

    write_mo_from_po_text(mo_root, "zh", domain, zh_po)
    write_mo_from_po_text(mo_root, "en", domain, en_po)

    provider = FileMoProvider(mo_root)
    i18n = I18n(provider, default_locale="zh", supported={"zh", "en"})

    with locale_context("zh-CN"):
        assert i18n.t("Hello", domain=domain) == "你好"
        assert i18n.tc("btn", "Save", domain=domain) == "保存"

    with locale_context("en-US"):
        assert i18n.t("Hello", domain=domain) == "Hello"
        assert i18n.tc("btn", "Save", domain=domain) == "Save"

    with locale_context("zh"):
        # Explicit locale override should win over contextvar
        assert i18n.t("Hello", domain=domain, locale="en-US") == "Hello"
