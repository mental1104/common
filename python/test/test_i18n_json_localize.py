from mental1104.common.i18n.json_localize import localize_json


def test_localize_json_overrides_and_prunes():
    cfg = {
        "name": "中文",
        "name_en": "English",
        "nested": {
            "title": "标题",
            "title_en": "Title",
        },
    }

    en_result = localize_json(cfg, locale="en")
    assert en_result == {"name": "English", "nested": {"title": "Title"}}
    assert "name_en" not in en_result

    zh_result = localize_json(cfg, locale="zh")
    assert zh_result == {"name": "中文", "nested": {"title": "标题"}}
    assert "name_en" not in zh_result
