import exp_hello


def test_extract_world():
    assert exp_hello.extract_world(exp_hello.HELLO) == "world"


def test_missing_word():
    assert exp_hello.extract_world("nothing here") is None
    assert exp_hello.extract_world(None) is None
