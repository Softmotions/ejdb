/**************************************************************************************************
 * EJDB2 Node.js native API binding.
 *
 * MIT License
 *
 * Copyright (c) 2012-2021 Softmotions Ltd <info@softmotions.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *************************************************************************************************/

const test = require('ava');
const { assert } = require('chai');
const { EJDB2, JBE } = require('./index');

test('Main', async (t) => {
  const db = await EJDB2.open('hello.db', { truncate: true });

  let q = db.createQuery('@mycoll/*');
  t.true(q != null);
  t.is(q.collection, 'mycoll');
  t.true(q.db != null);

  let id = await db.put('mycoll', { 'foo': 'bar' });
  t.is(id, 1);
  t.throwsAsync(db.put('mycoll', '{"'), {
    code: '@ejdb IWRC:86005 put',
    message: 'Unquoted JSON string (JBL_ERROR_PARSE_UNQUOTED_STRING)'
  });

  let doc = await db.get('mycoll', 1);
  t.deepEqual(doc, { foo: 'bar' });

  await db.put('mycoll', { 'foo': 'baz' });

  const list = await db.createQuery('@mycoll/*').list({ limit: 1 });
  t.is(list.length, 1);

  let first = await db.createQuery('@mycoll/*').first();
  t.true(first != null);
  t.deepEqual(first.json, { foo: 'baz' });
  t.is(first._raw, null);

  first = await db.createQuery('@mycoll/[zzz=bbb]').first();
  t.is(first, undefined);

  let firstN = await db.createQuery('@mycoll/*').firstN(5);
  t.true(firstN != null && firstN.length == 2);
  t.deepEqual(firstN[0].json, { foo: 'baz' });
  t.deepEqual(firstN[1].json, { foo: 'bar' });

  firstN = await db.createQuery('@mycoll/*').firstN(1);
  t.true(firstN != null && firstN.length == 1);
  t.deepEqual(firstN[0].json, { foo: 'baz' });

  // Query 1
  const rbuf = [];
  for await (const doc of q.stream()) {
    rbuf.push(doc.id);
    rbuf.push(doc._raw);
  }
  t.is(rbuf.toString(), '2,{"foo":"baz"},1,{"foo":"bar"}');

  // Query 2
  rbuf.length = 0;
  for await (const doc of db.createQuery('@mycoll/[foo=zaz]').stream()) {
    rbuf.push(doc.id);
    rbuf.push(doc._raw);
  }
  t.is(rbuf.length, 0);

  // Query 3
  for await (const doc of db.createQuery('/[foo=bar]', 'mycoll').stream()) {
    rbuf.push(doc.id);
    rbuf.push(doc._raw);
  }
  t.is(rbuf.toString(), '1,{"foo":"bar"}');

  let error = null;
  try {
    await db.createQuery('@mycoll/[').stream();
  } catch (e) {
    error = e;
    t.true(JBE.isInvalidQuery(e));
  }
  t.true(error != null);
  error = null;

  let count = await db.createQuery('@mycoll/* | count').scalarInt();
  t.is(count, 2);

  // Del
  await db.del('mycoll', 1);

  error = null;
  try {
    await db.get('mycoll', 1);
  } catch (e) {
    error = e;
    t.true(JBE.isNotFound(e));
  }
  t.true(error != null);
  error = null;

  count = await db.createQuery('@mycoll/* | count').scalarInt();
  t.is(count, 1);

  // Patch
  await db.patch('mycoll', '[{"op":"add", "path":"/baz", "value":"qux"}]', 2);
  doc = await db.get('mycoll', 2);
  t.deepEqual(doc, { foo: 'baz', baz: 'qux' });

  // DB Info
  doc = await db.info();
  assert.deepNestedInclude(doc, {
    'collections[0]': {
      name: 'mycoll',
      rnum: 1,
      dbid: 3,
      indexes: []
    }
  });

  // Indexes
  await db.ensureStringIndex('mycoll', '/foo', true);
  doc = await db.info();
  assert.deepNestedInclude(doc, {
    'collections[0].indexes[0]': {
      ptr: '/foo',
      mode: 5,
      idbf: 0,
      dbid: 4,
      rnum: 1
    }
  });

  // Test JQL set
  doc = await db.createQuery('@mycoll/[foo=:?]').setString(0, 'baz').first();
  assert.deepInclude(doc.json, {
    foo: 'baz',
    baz: 'qux'
  });

  let log = null;
  // Test explain log
  await db.createQuery('@mycoll/[foo=:?]').setString(0, 'baz').completionPromise({
    explainCallback: (l) => {
      log = l;
    }
  });
  assert.isString(log);
  t.true(log.indexOf('[INDEX] MATCHED  UNIQUE|STR|1 /foo EXPR1: \'foo = :?\' INIT: IWKV_CURSOR_EQ') != -1);


  doc = await db.createQuery('@mycoll/[foo=:?] and /[baz=:?]')
    .setString(0, 'baz')
    .setString(1, 'qux')
    .first();
  assert.deepInclude(doc.json, {
    foo: 'baz',
    baz: 'qux'
  });

  doc = await db.createQuery('@mycoll/[foo=:foo] and /[baz=:baz]')
    .setString('foo', 'baz')
    .setString('baz', 'qux')
    .first();
  assert.deepInclude(doc.json, {
    foo: 'baz',
    baz: 'qux'
  });

  doc = await db.createQuery('@mycoll/[foo re :?]').setRegexp(0, '.*').first();
  assert.deepInclude(doc.json, {
    foo: 'baz',
    baz: 'qux'
  });

  doc = await db.createQuery('@mycoll/* | /foo').first();
  assert.deepEqual(doc.json, { foo: 'baz' });

  await db.removeStringIndex('mycoll', '/foo', true);
  doc = await db.info();
  assert.deepNestedInclude(doc, {
    'collections[0].indexes': []
  });

  await db.removeCollection('mycoll');
  doc = await db.info();
  assert.deepNestedInclude(doc, {
    'collections': []
  });

  q = db.createQuery('@c1/* | limit 2');
  t.is(q.limit, 2);

  q = db.createQuery('@c2/*');
  t.is(q.limit, 0);

  // Rename collection
  id = await db.put('cc1', { 'foo': 1 });
  doc = await db.get('cc1', id);
  t.deepEqual(doc, { 'foo': 1 });

  await db.renameCollection('cc1', 'cc2');
  doc = await db.get('cc2', id);
  t.deepEqual(doc, { 'foo': 1 });

  const ts0 = +new Date();
  const ts = await db.onlineBackup('hello-bkp.db');
  t.true(ts0 < ts);

  await db.close();

  // Restore from backup
  const db2 = await EJDB2.open('hello-bkp.db', { truncate: false });
  doc = await db2.get('cc2', id);
  t.deepEqual(doc, { 'foo': 1 });
  await db2.close();

  //global.gc();
});

// test('GC', async () => {
//   global.gc();
//   await new Promise((resolve) => {
//     global.gc();
//     setTimeout(() => {
//       resolve();
//     }, 500);
//   });
// });

