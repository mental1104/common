import { test } from 'node:test';
import assert from 'node:assert';

import { HELLO, extractWorld } from '../src/hello.js';

test('extracts world', () => {
  assert.strictEqual(extractWorld(HELLO), 'world');
});

test('returns null when missing', () => {
  assert.strictEqual(extractWorld('nothing here'), null);
});
