import { EJDB2 } from 'ejdb2_node';

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
