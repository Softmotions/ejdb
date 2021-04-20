/**
 * Sample React Native App
 * https://github.com/facebook/react-native
 *
 * @format
 * @flow
 */

import React, { Component } from 'react';
import { Button, StyleSheet, Text, View } from 'react-native';
import { EJDB2, JBE } from 'ejdb2_react_native';

const { assert } = require('chai');
const t = {
  true: a => assert.isTrue(a),
  is: (a, b) => assert.equal(a, b),
  throwsAsync: (a, b) => a.catch(e => assert.deepInclude(e, b)),
  deepEqual: (a, b) => assert.deepEqual(a, b),
};

export default class App extends Component {
  constructor(props) {
    super(props);
    this.state = {
      status: 0,
    }
  }

  async test() {

    const db = await EJDB2.open('hello.db', { truncate: true });

    await db.createQuery('@mycoll/*').use(q => {
      t.true(q != null);
      t.is(q.collection, 'mycoll');
      t.true(q.db != null);
    });

    let id = await db.put('mycoll', { 'foo': 'bar' });
    t.is(id, 1);

    await t.throwsAsync(db.put('mycoll', '{"'), {
      code: '86005',
      message: 'com.softmotions.ejdb2.EJDB2Exception: @ejdb IWRC:86005 errno:0 Unquoted JSON string (JBL_ERROR_PARSE_UNQUOTED_STRING)'
    });

    let doc = await db.get('mycoll', 1);
    t.deepEqual(doc, { foo: 'bar' });

    await db.put('mycoll', { 'foo': 'baz' });

    const list = await db.createQuery('@mycoll/*').use(q => q.list({ limit: 1 }));
    t.is(list.length, 1);

    let first = await db.createQuery('@mycoll/*').use(q => q.first());
    t.true(first != null);
    t.deepEqual(first.json, { foo: 'baz' });
    t.is(first._raw, null);


    first = await db.createQuery('@mycoll/[zzz=bbb]').use(q => q.first());
    t.is(first, undefined);

    let firstN = await db.createQuery('@mycoll/*').use(q => q.firstN(5));
    t.true(firstN != null && firstN.length == 2);
    t.deepEqual(firstN[0].json, { foo: 'baz' });
    t.deepEqual(firstN[1].json, { foo: 'bar' });

    firstN = await db.createQuery('@mycoll/*').use(q => q.firstN(1));
    t.true(firstN != null && firstN.length == 1);
    t.deepEqual(firstN[0].json, { foo: 'baz' });

    // Query 1
    const rbuf = [];
    await db.createQuery('@mycoll/*').withLimit(10).useExecute((doc) => {
      rbuf.push(doc.id);
      rbuf.push(doc._raw);
    });
    t.is(rbuf.toString(), '2,{"foo":"baz"},1,{"foo":"bar"}');

    // Query 2
    rbuf.length = 0;
    await db.createQuery('@mycoll/[foo=:? and foo=:bar]')
      .setString(0, 'baz')
      .setString('bar', 'baz')
      .useExecute((doc) => {
        rbuf.push(doc.id);
        rbuf.push(doc._raw);
      });
    t.is(rbuf.toString(), '2,{"foo":"baz"}');


    let error = null;
    try {
      await db.createQuery('@mycoll/[').useExecute();
    } catch (e) {
      error = e;
      t.true(JBE.isInvalidQuery(e));
    }
    t.true(error != null);
    error = null;

    let q = await db.createQuery('@mycoll/*');
    q.close();

    try {
      await q.useExecute();
    } catch (e) {
      error = e;
      t.true(e.toString().indexOf('IllegalStateException') != -1);
    }
    t.true(error != null);
    error = null;

    let count = await db.createQuery('@mycoll/* | count').use(q => q.scalarInt());
    t.is(count, 2);

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

    count = await db.createQuery('@mycoll/* | count').use(q => q.scalarInt());
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
    doc = await db.createQuery('@mycoll/[foo=:?]')
      .setString(0, 'baz').use(q => q.first());
    assert.deepInclude(doc.json, {
      foo: 'baz',
      baz: 'qux'
    });

    // Test explain log
    let log;
    q = await db.createQuery('@mycoll/[foo=:?]')
      .setString(0, 'baz')
      .withExplain();
    await q.useExecute();
    log = q.explainLog;
    t.true(log.indexOf('[INDEX] MATCHED  UNIQUE|STR|1 /foo EXPR1: \'foo = :?\' INIT: IWKV_CURSOR_EQ') != -1);


    await db.createQuery('@mycoll/[foo=:?]')
      .setString(0, 'baz')
      .useExecute((jbdoc, jql) => {
        t.true(jql != null);
        q.explainLog;
        t.true(log.indexOf('[INDEX] MATCHED  UNIQUE|STR|1 /foo EXPR1: \'foo = :?\' INIT: IWKV_CURSOR_EQ') != -1);
      });

    doc = await db.createQuery('@mycoll/[foo=:?] and /[baz=:?]')
      .setString(0, 'baz')
      .setString(1, 'qux')
      .use(q => q.first());

    assert.deepInclude(doc.json, {
      foo: 'baz',
      baz: 'qux'
    });


    doc = await db.createQuery('@mycoll/[foo re :?]').setRegexp(0, '.*').use(q => q.first());
    assert.deepInclude(doc.json, {
      foo: 'baz',
      baz: 'qux'
    });

    doc = await db.createQuery('@mycoll/[foo re :?]').setRegexp(0, /.*/).use(q => q.first());
    assert.deepInclude(doc.json, {
      foo: 'baz',
      baz: 'qux'
    });

    doc = await db.createQuery('@mycoll/* | /foo').use(q => q.first());
    assert.deepEqual(doc.json, { foo: 'baz' });

    await db.removeStringIndex('mycoll', '/foo', true);
    doc = await db.info();

    const dbfile = doc.file;
    t.true(typeof dbfile == 'string');

    assert.deepNestedInclude(doc, {
      'collections[0].indexes': []
    });

    await db.removeCollection('mycoll');
    doc = await db.info();
    assert.deepNestedInclude(doc, {
      'collections': []
    });

    q = db.createQuery('@c1/* | limit 2 skip 3');
    t.is(q.limit, 2);
    t.is(q.skip, 3);
    q.close();

    // Rename collection
    id = await db.put('cc1', { 'foo': 1 });
    t.true(id > 0);
    doc = await db.get('cc1', id);
    t.deepEqual(doc, { 'foo': 1 });

    await db.renameCollection('cc1', 'cc2');
    doc = await db.get('cc2', id);
    t.deepEqual(doc, { 'foo': 1 });


    let i = 0;
    for (i = 0; i < 1023; ++i) {
      await db.put('load', { 'name': `v${i}` });
    }

    i = 0;
    await db.createQuery('@load/* | inverse').useExecute(((jbdoc) => {
      const json = jbdoc.json;
      t.is(json.name, `v${i}`);
      i++;
    }));
    t.is(i, 1023);

    const ts0 = +new Date();
    const ts = await db.onlineBackup(`${dbfile}.bkp`);
    t.true(ts0 < ts);

    await db.close();

    // Restore from backup
    const db2 = await EJDB2.open('hello.db.bkp', { truncate: false });
    doc = await db2.get('cc2', id);
    t.deepEqual(doc, { 'foo': 1 });
    await db2.close();
  }

  render() {
    return (
      <View style={styles.container}>
        {this.state.status
          ? <Text testID="status">{this.state.status}</Text>
          : <Button testID="run" title="Run tests" onPress={e => this.run()}></Button>}
      </View>
    );
  }

  async run() {
    try {
      this.setState({ status: 'Testing' });

      await this.test();

      this.setState({ status: 'OK' });
    } catch (e) {
      console.log('Error ', e);
      this.setState({ status: `${e.code || ''} ${e}` });
      throw e;
    }
  }
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: '#F5FCFF',
  },
});
