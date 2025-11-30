local M = {}

M.HELLO = "hello world"

function M.extract_world(greeting)
  if greeting == nil then
    return nil
  end

  local start_idx = string.find(greeting, "world", 1, true)
  if not start_idx then
    return nil
  end

  local word_len = string.len("world")
  return string.sub(greeting, start_idx, start_idx + word_len - 1)
end

return M
