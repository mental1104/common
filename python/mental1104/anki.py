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
        with open(json_file, "r", encoding="utf-8") as f:
            data = json.load(f)
        
        for key, value in data.items():
            note = genanki.Note(
                model=self.model,
                fields=[key, value])
            self.deck.add_note(note)

    def save_to_file(self, output_file):
        genanki.Package(self.deck).write_to_file(output_file)