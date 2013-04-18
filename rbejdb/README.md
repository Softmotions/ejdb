Embedded JSON database library Ruby binding
============================================================

Installation
-----------------------------------------------------

**Required tools/system libraries:**

* gcc
* **ruby >= 1.9.1**
* EJDB C library **libtcejdb** ([from sources](https://github.com/Softmotions/ejdb#manual-installation) or as [debian packages](https://github.com/Softmotions/ejdb/wiki/Debian-Ubuntu-installation))

**(A) Installing directly from sources**

```
git clone https://github.com/Softmotions/ejdb.git
cd ./rbejdb
make && sudo make install && make check
```

One snippet intro
---------------------------------

```Ruby
require "rbejdb"

#Open zoo DB
jb = EJDB.open("zoo", EJDB::DEFAULT_OPEN_MODE | EJDB::JBOTRUNC)

parrot1 = {
    "name" => "Grenny",
    "type" => "African Grey",
    "male" => true,
    "age" => 1,
    "birthdate" => Time.now,
    "likes" => ["green color", "night", "toys"],
    "extra1" => nil
}
parrot2 = {
    "name" => "Bounty",
    "type" => "Cockatoo",
    "male" => false,
    "age" => 15,
    "birthdate" => Time.now,
    "likes" => ["sugar cane"],
    "extra1" => nil
}

jb.save("parrots", parrot1, parrot2)
puts "Grenny OID: #{parrot1["_id"]}"
puts "Bounty OID: #{parrot2["_id"]}"

results = jb.find("parrots", {"likes" => "toys"}, {"$orderby" => {"name" => 1}})

puts "Found #{results.count} parrots"

results.each { |res|
  puts "#{res['name']} likes toys!"
}

results.close #It's not mandatory to close cursor explicitly
jb.close #Close the database

```
