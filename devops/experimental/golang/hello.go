package hello

import "strings"

const Hello = "hello world"

func ExtractWorld(greeting string) (string, bool) {
	idx := strings.Index(greeting, "world")
	if idx == -1 {
		return "", false
	}

	word := greeting[idx : idx+len("world")]
	return word, true
}
