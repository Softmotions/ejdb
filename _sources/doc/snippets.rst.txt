
Intro snippets
==============

There is a set of sample snippets demonstrating EJDB API which are used in various languages.

.. contents::


C/C++
-----

.. code-block:: c

    #include <ejdb/ejdb.h>

    static EJDB *jb;

    int main() {
        jb = ejdbnew();
        if (!ejdbopen(jb, "addressbook",
                      JBOWRITER | JBOCREAT | JBOTRUNC)) {
            return 1;
        }
        //Get or create collection 'contacts'
        EJCOLL *coll = ejdbcreatecoll(jb, "contacts", NULL);

        bson bsrec;
        bson_oid_t oid;

        //Insert one record:
        //JSON: {'name' : 'Bruce', 'phone' : '333-222-333', 'age' : 58}
        bson_init(&bsrec);
        bson_append_string(&bsrec, "name", "Bruce");
        bson_append_string(&bsrec, "phone", "333-222-333");
        bson_append_int(&bsrec, "age", 58);
        bson_finish(&bsrec);
        //Save BSON
        ejdbsavebson(coll, &bsrec, &oid);
        fprintf(stderr, "\nSaved Bruce");
        bson_destroy(&bsrec);

        //Now execute query
        //QUERY: {'name' : {'$begin' : 'Bru'}}
        //Name starts with 'Bru' string

        bson bq1;
        bson_init_as_query(&bq1);
        bson_append_start_object(&bq1, "name");
        bson_append_string(&bq1, "$begin", "Bru");
        bson_append_finish_object(&bq1);
        bson_finish(&bq1);

        EJQ *q1 = ejdbcreatequery(jb, &bq1, NULL, 0, NULL);

        uint32_t count;
        TCLIST *res = ejdbqryexecute(coll, q1, &count, 0, NULL);
        fprintf(stderr, "\n\nRecords found: %d\n", count);

        //Now print the result set records
        for (int i = 0; i < TCLISTNUM(res); ++i) {
            void *bsdata = TCLISTVALPTR(res, i);
            bson_print_raw(bsdata, 0);
        }
        fprintf(stderr, "\n");

        //Dispose result set
        tclistdel(res);

        //Dispose query
        ejdbquerydel(q1);
        bson_destroy(&bq1);

        //Close database
        ejdbclose(jb);
        ejdbdel(jb);
        return 0;
    }

You can save this code in csnippet.c then build:

.. code-block:: sh

    gcc -std=c99 -Wall -pedantic  -c -o csnippet.o csnippet.c
    gcc -o csnippet csnippet.o -ltcejdb


Nodejs
------

.. code-block:: js

    var EJDB = require("ejdb");
    //Open zoo DB
    var jb = EJDB.open("zoo",
                        EJDB.DEFAULT_OPEN_MODE | EJDB.JBOTRUNC);

    var parrot1 = {
        "name" : "Grenny",
        "type" : "African Grey",
        "male" : true,
        "age" : 1,
        "birthdate" : new Date(),
        "likes" : ["green color", "night", "toys"],
        "extra1" : null
    };
    var parrot2 = {
        "name" : "Bounty",
        "type" : "Cockatoo",
        "male" : false,
        "age" : 15,
        "birthdate" : new Date(),
        "likes" : ["sugar cane"]
    };

    jb.save("parrots", [parrot1, parrot2], function(err, oids) {
        if (err) {
            console.error(err);
            return;
        }
        console.log("Grenny OID: " + parrot1["_id"]);
        console.log("Bounty OID: " + parrot2["_id"]);

        jb.find("parrots",
                {"likes" : "toys"},
                {"$orderby" : {"name" : 1}},
                function(err, cursor, count) {
                    if (err) {
                        console.error(err);
                        return;
                    }
                    console.log("Found " + count + " parrots");
                    while (cursor.next()) {
                        console.log(cursor.field("name") + " likes toys!");
                    }
                    //It's not mandatory to close cursor explicitly
                    cursor.close();
                    jb.close(); //Close the database
                });
    });

.. seealso::

      :ref:`Nodejs language binding <nodejs>`


Python
------

.. code-block:: python

    import pyejdb
    from datetime import datetime

    #Open database
    ejdb = pyejdb.EJDB("zoo", pyejdb.DEFAULT_OPEN_MODE | pyejdb.JBOTRUNC)

    parrot1 = {
        "name": "Grenny",
        "type": "African Grey",
        "male": True,
        "age": 1,
        "birthdate": datetime.utcnow(),
        "likes": ["green color", "night", "toys"],
        "extra1": None
    }
    parrot2 = {
        "name": "Bounty",
        "type": "Cockatoo",
        "male": False,
        "age": 15,
        "birthdate": datetime.utcnow(),
        "likes": ["sugar cane"],
        "extra1": None
    }
    ejdb.save("parrots2", parrot1, parrot2)

    with ejdb.find("parrots2", {"likes" : "toys"},
              hints={"$orderby" : [("name", 1)]}) as cur:
        print("found %s parrots" % len(cur))
        for p in cur:
            print("%s likes toys!" % p["name"])

    ejdb.close()


.. seealso::

     `Python language binding <https://github.com/Softmotions/ejdb-python>`_


Lua
----

.. code-block:: lua

    local ejdb = require("ejdb")
    local inspect = require("ejdb.inspect")
    local Q = ejdb.Q

    -- Used modes:
    -- 'r' - read
    -- 'w' - write
    -- 'c' - create db if not exists
    -- 't' - truncate existing db
    local db = ejdb.open("zoo", "rwct")

    -- Unordered lua table
    local parrot1 = {
      name = "Grenny",
      type = "African Grey",
      male = true,
      age = 1,
      birthhdate = ejdb.toDateNow(),
      likes = { "green color", "night", "toys" },
      extra1 = ejdb.toNull()
    }

    -- Preserve order of BSON keys
    local parrot2 = Q();
    parrot2:KV("name", "Bounty"):KV("type", "Cockatoo"):KV("male", false)
    parrot2:KV("age", 15):KV("birthdate",
      ejdb.toDate({ year = 2013, month = 1, day = 1, hour = 0, sec = 1 }))
    parrot2:KV("likes", { "sugar cane" }):KV("extra1", ejdb.toNull())

    --IF YOU WANT SOME DATA INSPECTIONS:
    --print(ejdb.print_bson(parrot2:toBSON()))
    --local obj = ejdb.from_bson(parrot2:toBSON())
    --print(inspect(obj));

    db:save("parrots2", parrot1)
    db:save("parrots2", parrot2)

    -- Below two equivalent queries:
    -- Q1
    local res, count, log =
    db:find("parrots2", Q("likes", "toys"):OrderBy("name asc", "age desc"))
    for i = 1, #res do -- iterate one
      local ob = res:object(i)
      print("" .. ob["name"] .. " likes toys #1")
    end

    -- Q2
    local res, count, log =
    db:find("parrots2", Q():F("likes"):Eq("toys"):OrderBy({ name = 1 }, { age = -1 }))
    for i = 1, #res do -- iterate one
      print("" .. res:field(i, "name") .. " likes toys #2")
    end

    -- Second way to iterate
    for vobj, idx in res() do
      print("" .. vobj["name"] .. " likes toys #3")
    end

    db:close()


.. seealso::

     `Lua language binding <https://github.com/Softmotions/ejdb-lua>`_


Ruby
----

.. code-block:: ruby

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

.. seealso::

     `Ruby language binding <https://github.com/Softmotions/ejdb-ruby>`_


C#
----

.. code-block:: c#

    using System;
    using Ejdb.DB;
    using Ejdb.BSON;

    namespace sample {

        class MainClass {

            public static void Main(string[] args) {
                var jb = new EJDB("zoo",
                                   EJDB.DEFAULT_OPEN_MODE | EJDB.JBOTRUNC);
                jb.ThrowExceptionOnFail = true;

                var parrot1 = BSONDocument.ValueOf(new {
                    name = "Grenny",
                    type = "African Grey",
                    male = true,
                    age = 1,
                    birthdate = DateTime.Now,
                    likes = new string[] { "green color", "night", "toys" },
                    extra = BSONull.VALUE
                });

                var parrot2 = BSONDocument.ValueOf(new {
                    name = "Bounty",
                    type = "Cockatoo",
                    male = false,
                    age = 15,
                    birthdate = DateTime.Now,
                    likes = new string[] { "sugar cane" }
                });

                jb.Save("parrots", parrot1, parrot2);

                Console.WriteLine("Grenny OID: " + parrot1["_id"]);
                Console.WriteLine("Bounty OID: " + parrot2["_id"]);

                var q = jb.CreateQuery(new {
                    likes = "toys"
                }, "parrots").OrderBy("name");

                using (var cur = q.Find()) {
                    Console.WriteLine("Found " + cur.Length + " parrots");
                    foreach (var e in cur) {
                        //fetch the `name` and the first element
                        // of likes array from the current BSON iterator.
                        //alternatively you can fetch whole
                        // document from the iterator: `e.ToBSONDocument()`
                        BSONDocument rdoc = e.ToBSONDocument("name", "likes.0");
                        Console.WriteLine(string.Format("{0} likes the '{1}'",
                                          rdoc["name"], rdoc["likes.0"]));
                    }
                }
                q.Dispose();
                jb.Dispose();
            }
        }
    }

.. seealso::

     `C# language binding <https://github.com/Softmotions/ejdb-csharp>`_


.. code-block:: java

    package org.ejdb.sample1;

    import org.ejdb.bson.BSONObject;
    import org.ejdb.driver.EJDB;
    import org.ejdb.driver.EJDBCollection;
    import org.ejdb.driver.EJDBQueryBuilder;
    import org.ejdb.driver.EJDBResultSet;

    import java.util.Calendar;
    import java.util.Date;

    /**
     * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
     * @version $Id$
     */
    public class Main {

        public static void main(String[] args) {
            EJDB ejdb = new EJDB();

            try {
                // Used modes:
                //  - read
                //  - write
                //  - create db if not exists
                //  - truncate existing db
                ejdb.open("zoo", EJDB.JBOREADER | EJDB.JBOWRITER |
                                 EJDB.JBOCREAT | EJDB.JBOTRUNC);

                BSONObject parrot1 = new BSONObject("name", "Grenny")
                        .append("type", "African Grey")
                        .append("male", true)
                        .append("age", 1)
                        .append("birthhdate", new Date())
                        .append("likes", new String[]{"green color", "night", "toys"})
                        .append("extra1", null);

                Calendar calendar = Calendar.getInstance();
                calendar.set(2013, 1, 1, 0, 0, 1);

                BSONObject parrot2 = new BSONObject();
                parrot2.put("name", "Bounty");
                parrot2.put("type", "Cockatoo");
                parrot2.put("male", false);
                parrot2.put("age", 15);
                parrot2.put("birthdate", calendar.getTime());
                parrot2.put("likes", new String[]{"sugar cane"});
                parrot2.put("extra1", null);

                System.out.println("parrot1 =\n\t\t" + parrot1);
                System.out.println("parrot2 =\n\t\t" + parrot2);

                EJDBCollection parrots = ejdb.getCollection("parrots");

                // saving
                parrots.save(parrot1);
                parrots.save(parrot2);

                EJDBQueryBuilder qb;
                EJDBResultSet rs;

                // Below two equivalent queries:
                // Q1
                qb = new EJDBQueryBuilder();
                qb.field("likes", "toys")
                        .orderBy().asc("name").desc("age");

                rs = parrots.createQuery(qb).find();
                System.out.println();
                System.out.println("Results (Q1): " + rs.length());
                for (BSONObject r : rs) {
                    System.out.println("\t" + r);
                }
                rs.close();

                // Q2
                qb = new EJDBQueryBuilder();
                qb.field("likes").eq("toys")
                        .orderBy().add("name", true).add("age", false);

                rs = parrots.createQuery(qb).find();
                System.out.println();
                System.out.println("Results (Q2): " + rs.length());
                for (BSONObject r : rs) {
                    System.out.println("\t" + r);
                }

                // Second way to iterate
                System.out.println();
                System.out.println("Results (Q2): " + rs.length());
                for (int i = 0; i < rs.length(); ++i) {
                    System.out.println("\t" + i + " => " + rs.get(i));
                }

                rs.close();
            } finally {
                if (ejdb.isOpen()) {
                    ejdb.close();
                }
            }
        }
    }

.. seealso::

     `Java language binding <https://github.com/Softmotions/ejdb-java>`_


Go
----

.. code-block:: go

    package ejdbtutorial

    import (
        "fmt"
        "github.com/mkilling/goejdb"
        "labix.org/v2/mgo/bson"
        "os"
    )

    func main() {
        // Create a new database file and open it
        jb, err := goejdb.Open("addressbook", JBOWRITER | JBOCREAT | JBOTRUNC)
        if err != nil {
            os.Exit(1)
        }
        // Get or create collection 'contacts'
        coll, _ := jb.CreateColl("contacts", nil)

        // Insert one record:
        // JSON: {'name' : 'Bruce', 'phone' : '333-222-333', 'age' : 58}
        rec := map[string]interface{} {"name" : "Bruce",
                                       "phone" : "333-222-333", "age" : 58}
        bsrec, _ := bson.Marshal(rec)
        coll.SaveBson(bsrec)
        fmt.Printf("\nSaved Bruce")

        // Now execute query
        // Name starts with 'Bru' string
        res, _ := coll.Find(`{"name" : {"$begin" : "Bru"}}`)
        fmt.Printf("\n\nRecords found: %d\n", len(res))

        // Now print the result set records
        for _, bs := range res {
            var m map[string]interface{}
            bson.Unmarshal(bs, &m)
            fmt.Println(m)
        }

        // Close database
        jb.Close()
    }

.. seealso::

    https://github.com/mkilling/goejdb




Other third-party bindings
--------------------------

.. note::

    :ref:`There are other language bindings provided by our community <thirdparty_bindings>`.
