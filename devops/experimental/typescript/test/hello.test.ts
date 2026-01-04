import { test } from 'node:test';
import assert from 'node:assert/strict';

import { HELLO, extractWorld } from '../src/hello.js';

test('extracts world', () => {
  assert.strictEqual(extractWorld(HELLO), 'world');
});

test('returns null when missing', () => {
  assert.strictEqual(extractWorld('no match here'), null);
  assert.strictEqual(extractWorld(undefined), null);
});
