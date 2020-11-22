# EJDB2 Node.js native binding

Embeddable JSON Database engine http://ejdb.org Node.js binding.

See https://github.com/Softmotions/ejdb/blob/master/README.md

For API usage examples take a look into [/example](https://github.com/Softmotions/ejdb/tree/master/src/bindings/ejdb2_node/example) and [test.js](https://github.com/Softmotions/ejdb/tree/master/src/bindings/ejdb2_node/test.js)

```javascript
const { EJDB2 } = require('node-ejdb-lite');

async function run() {
  const db = await EJDB2.open('example.db', { truncate: true });

  var id = await db.put('parrots', {'name': 'Bianca', 'age': 4});
  console.log(`Bianca record: ${id}`);

  id = await db.put('parrots', {'name': 'Darko', 'age': 8});
  console.log(`Darko record: ${id}`);

  const q = db.createQuery('/[age > :age]', 'parrots');

  for await (const doc of q.setNumber('age', 3).stream()) {
    console.log(`Found ${doc}`);
  }

  await db.close();
}

run();
```

## Supported platforms

* Linux x64
* OSX

## Prerequisites

* node >= v10.0.0

## How build it manually

``` sh
git clone https://github.com/markwylde/node-ejdb-lite.git
cd ./node-ejdb-lite
npm install
```
