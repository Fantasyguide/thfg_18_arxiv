import test from 'node:test';
import assert from 'node:assert/strict';

test('node:test 可用，ES module 导入正常', () => {
  assert.equal(1 + 1, 2);
});
