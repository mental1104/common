import genanki
import json
import random

class AnkiApkgGenerator:
    def __init__(self, model_name='Simple Model', deck_name='Sample Deck'):
        self.model = genanki.Model(
            random.randint(1000000000, 9999999999),  # 随机的唯一ID
            model_name,
            fields=[
                {'name': 'Question'},
                {'name': 'Answer'},
            ],
            templates=[
                {
                    'name': 'Card 1',
                    'qfmt': '{{Question}}',   # 正面模板
                    'afmt': '{{FrontSide}}<hr id="answer">{{Answer}}',  # 背面模板
                },
            ])
        
        self.deck = genanki.Deck(
            random.randint(1000000000, 9999999999),  # 随机的唯一ID
            deck_name)

    def add_notes_from_json(self, json_file):
        """json文件按如下组织：
        {
            "办理登机手续": "Check in for my flight",
            "登机牌": "Boarding pass",
            "托运行李额：20公斤": "Checked baggage allowance: 20 kilograms",
            "随身行李": "Carry-on luggage",
            "护照有效期至少六个月": "Passport valid for at least six months",
            "入境卡": "Immigration card",
            "海关申报表": "Customs declaration form",
            "汇率": "Currency exchange rate",
            "免签入境": "Visa-free entry",
            "安检": "Security check"
        }
        """
        with open(json_file, "r", encoding="utf-8") as f:
            data = json.load(f)
        
        for key, value in data.items():
            note = genanki.Note(
                model=self.model,
                fields=[key, value])
            self.deck.add_note(note)

    def save_to_file(self, output_file):
        genanki.Package(self.deck).write_to_file(output_file)