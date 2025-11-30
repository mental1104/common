#!/usr/bin/env bash

HELLO="hello world"

extract_world() {
  local greeting="${1:-}"
  if [[ "$greeting" == *"world"* ]]; then
    echo "world"
    return 0
  fi
  return 1
}
