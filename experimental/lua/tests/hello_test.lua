package.path = table.concat({
  package.path,
  string.format("%s/../src/?.lua", debug.getinfo(1, "S").source:match("^@(.+)/"))
}, ";")

local hello = require("hello")

assert(hello.extract_world(hello.HELLO) == "world", "should find world")
assert(hello.extract_world("no match") == nil, "should return nil when missing")

print("[ok] lua tests passed")
