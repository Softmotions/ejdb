var fs = require("fs");
var EJDB = require("../ejdb.js");


var now = new Date();
var jb = null;

module.exports.testSetup = function(test) {
    jb = EJDB.open("var/tdbt2", EJDB.JBOWRITER | EJDB.JBOCREAT | EJDB.JBOTRUNC);
    test.done();
};

module.exports.testSaveLoad = function(test) {
    test.ok(jb);
    test.ok(jb.isOpen());
    var parrot1 = {
        "name" : "Grenny",
        "type" : "African Grey",
        "male" : true,
        "age" : 1,
        "birthdate" : now,
        "likes" : ["green color", "night", "toys"],
        "extra1" : null
    };
    var parrot2 = {
        "name" : "Bounty",
        "type" : "Cockatoo",
        "male" : false,
        "age" : 15,
        "birthdate" : now,
        "likes" : ["sugar cane"],
        "extra1" : null
    };
    jb.save("parrots", [parrot1, null, parrot2], function(err, oids) {
        test.ifError(err);
        test.ok(oids);
        test.equal(oids.length, 3);
        test.equal(parrot1["_id"], oids[0]);
        test.ok(oids[1] == null);
        test.equal(parrot2["_id"], oids[2]);

        jb.load("parrots", parrot2["_id"], function(err, obj) {
            test.ifError(err);
            test.ok(obj);
            test.equal(obj._id, parrot2["_id"]);
            test.equal(obj.name, "Bounty");
            test.done();
        });
    });
};

module.exports.testQuery1 = function(test) {
    test.ok(jb);
    test.ok(jb.isOpen());
    jb.find("parrots", {}, function(err, cursor, count) {
        test.ifError(err);
        test.equal(count, 2);
        test.ok(cursor);
        var c = 0;
        while (cursor.next()) {
            ++c;
            var rv = cursor.object();
            test.ok(rv);
            test.ok(typeof rv["_id"] === "string");
            test.ok(typeof rv["name"] === "string");
            test.ok(typeof rv["age"] === "number");
            test.ok(rv["birthdate"] && typeof rv["birthdate"] === "object");
            test.ok(rv["birthdate"].constructor === Date);
            test.ok(typeof rv["male"] === "boolean");
            test.ok(rv["extra1"] === null);
            test.ok(rv["likes"] && typeof rv["likes"] === "object");
            test.ok(rv["likes"].constructor === Array);
            test.ok(rv["likes"].length > 0);
        }
        test.equal(c, 2);
        test.ifError(err);

        //Query sync
        cursor = jb.find("parrots", {});
        test.ok(cursor);
        test.equal(cursor.length, 2);

        //Query sync, count mode
        var count = jb.find("parrots", {}, {$onlycount : true});
        test.equal(typeof count, "number");
        test.equal(count, 2);

        //Query sync, findOne
        var obj = jb.findOne("parrots");
        test.ok(obj);
        test.ok(typeof obj["name"] === "string");
        test.ok(typeof obj["age"] === "number");

        //Async with one callback
        jb.find("parrots", function(err, cursor, count) {
            test.ifError(err);
            test.equal(count, 2);
            test.ok(cursor);
            test.equal(cursor.length, 2);
            test.done();
        });
    });
};

module.exports.testQuery2 = function(test) {
    test.ok(jb);
    test.ok(jb.isOpen());
    jb.find("parrots",
            {name : /(grenny|bounty)/ig},
            {$orderby : {name : 1}},
            function(err, cursor, count) {
                test.ifError(err);
                test.ok(cursor);
                test.equal(2, count);
                for (var c = 0; cursor.next(); ++c) {
                    var rv = cursor.object();
                    if (c != 0) continue;
                    test.equal(rv["name"], "Bounty");
                    test.equal(cursor.field("name"), "Bounty");
                    test.equal(rv["type"], "Cockatoo");
                    test.equal(cursor.field("type"), "Cockatoo");
                    test.equal(rv["male"], false);
                    test.equal(cursor.field("male"), false);
                    test.equal(rv["age"], 15);
                    test.equal(cursor.field("age"), 15);
                    test.equal("" + rv["birthdate"], "" + now);
                    test.equal("" + cursor.field("birthdate"), "" + now);
                    test.equal(rv["likes"].join(","), "sugar cane");
                    test.equal(cursor.field("likes").join(","), "sugar cane");
                }
                test.done();
            });

};


//Test with OR, cursor.reset
module.exports.testQuery3 = function(test) {
    test.ok(jb);
    test.ok(jb.isOpen());
    jb.find("parrots",
            {}, //main query selector
            [
                //OR joined conditions
                {name : "Grenny"},
                {name : "Bounty"}
            ],
            {$orderby : {name : 1}},
            function(err, cursor, count) {
                test.ifError(err);
                test.ok(cursor);
                test.equal(count, 2);
                for (var c = 0; cursor.next(); ++c) {
                    var rv = cursor.object();
                    if (c != 1) continue;
                    test.equal(rv["name"], "Grenny");
                    test.equal(cursor.field("name"), "Grenny");
                    test.equal(rv["type"], "African Grey");
                    test.equal(cursor.field("type"), "African Grey");
                    test.equal(rv["male"], true);
                    test.equal(cursor.field("male"), true);
                    test.equal(rv["age"], 1);
                    test.equal(cursor.field("age"), 1);
                    test.equal("" + rv["birthdate"], "" + now);
                    test.equal("" + cursor.field("birthdate"), "" + now);
                    test.equal(rv["likes"].join(","), "green color,night,toys");
                    test.equal(cursor.field("likes").join(","), "green color,night,toys");
                }

                //test cursor reset
                cursor.reset();
                for (c = 0; cursor.next(); ++c);
                test.equal(c, 2);

                //explicit cursor close
                cursor.close();
                test.done();
            });
};

module.exports.testCircular = function(test) {
    test.ok(jb);
    test.ok(jb.isOpen());

    //Circular query object
    var cirQuery = {};
    cirQuery.cq = cirQuery;
    var err = null;
    try {
        jb.find("parrots", cirQuery, function(err, cursor, count) {
        });
    } catch (e) {
        err = e;
    }
    test.ok(err);
    test.equal(err.message, "Converting circular structure to JSON");

    err = null;
    try {
        jb.save("parrots", [cirQuery]);
    } catch (e) {
        err = e;
    }
    test.ok(err);
    test.equal(err.message, "Converting circular structure to JSON");
    test.done();
};


module.exports.testSaveLoadBuffer = function(test) {
    test.ok(jb);
    test.ok(jb.isOpen());

    var sally = {
        "name" : "Sally",
        "mood" : "Angry",
        "secret" : new Buffer("Some binary secrect", "utf8")
    };
    var molly = {
        "name" : "Molly",
        "mood" : "Very angry",
        "secret" : null
    };

    jb.save("birds", sally, function(err, oids) {
        test.ifError(err);
        test.ok(oids);
        test.ok(oids.length === 1);
        test.ok(sally["_id"]);
        var sallyOid = sally["_id"];
        jb.load("birds", sallyOid, function(err, obj) {
            test.ifError(err);
            test.ok(obj["secret"] instanceof Buffer);
            test.equal(obj["secret"], "Some binary secrect");
            jb.save("birds", [sally, molly], function(err, oids) {
                test.ifError(err);
                test.ok(oids);
                test.ok(oids.indexOf(sallyOid) !== -1);
                test.done();
            });
        });
    });
};

module.exports.testUseStringIndex = function(test) {
    test.ok(jb);
    test.ok(jb.isOpen());
    jb.find("birds", {"name" : "Molly"}, {"$explain" : true}, function(err, cursor, count, log) {
        test.ifError(err);
        test.ok(cursor);
        test.ok(count == 1);
        test.ok(log);
        test.ok(log.indexOf("RUN FULLSCAN") !== -1);
        //Now set the name string index
        jb.ensureStringIndex("birds", "name", function(err) {
            test.ifError(err);
            jb.find("birds", {"name" : "Molly"}, {"$explain" : true}, function(err, cursor, count, log) {
                test.ok(log.indexOf("MAIN IDX: 'sname'") !== -1);
                test.done();
            });
        });
    });
};

module.exports.testCMeta = function(test) {
    var dm = jb.getDBMeta();
    //console.log("dm=" + JSON.stringify(dm));
    test.ok(dm);
    test.equal(dm["file"], "var/tdbt2");
    test.ok(dm["collections"]);
    test.ok(dm["collections"].constructor == Array);
    var parrots = dm["collections"][1];
    test.ok(parrots);
    test.equal(parrots["name"], "parrots");
    //todo...
    test.done();
};

module.exports.testUpdate1 = function(test) {
    test.ok(jb);
    test.ok(jb.isOpen());
    jb.update("parrots", {"name" : {"$icase" : "GRENNY"}, "$inc" : {"age" : 10}},
            {"$explain" : true},
            function(err, count, log) {
                test.ifError(err);
                test.equal(count, 1);
                test.ok(log);
                test.ok(log.indexOf("UPDATING MODE: YES") !== -1);
                jb.findOne("parrots", {"age" : 11}, function(err, obj) { //age was incremented 1 + 10
                    test.ifError(err);
                    test.ok(obj);
                    test.equal(obj["name"], "Grenny");
                    jb.save("parrots", {"_id" : obj["_id"], "extra1" : 1}, {"$merge" : true}, function(err, ids) {
                        test.ifError(err);
                        test.ok(ids);
                        test.equal(ids.length, 1);
                        jb.load("parrots", ids[0], function(err, obj) {
                            test.ifError(err);
                            test.ok(obj);
                            test.equal(obj["name"], "Grenny");
                            test.equal(obj["extra1"], 1);
                            var q = {"_id" : {"$in" : ids}, "$set" : {"stime" : +new Date}};
                            jb.update("parrots", q, function(err, count) {
                                test.ifError(err);
                                test.equal(count, 1);
                                test.done();
                            });
                        });
                    });
                });
            });
};


module.exports.test_id$nin = function(test) {
    jb.findOne("parrots", {}, function(err, obj) {
        test.ifError(err);
        test.ok(obj);
        jb.find("parrots", {"_id" : {"$in" : [obj["_id"]]}}, function(err, cursor, count) {
            test.ifError(err);
            test.equal(count, 1);
            test.ok(cursor.hasNext());
            test.ok(cursor.next());
            test.equal(cursor.field("_id"), obj["_id"]);
            jb.find("parrots", {"_id" : {"$nin" : [obj["_id"]]}}, {"$explain" : true}, function(err, cursor, count, log) {
                test.ifError(err);
                test.ok(count > 0);
                while (cursor.next()) {
                    test.ok(cursor.field("_id"));
                    test.ok(cursor.field("_id") != obj["_id"]);
                    test.ok(log.indexOf("RUN FULLSCAN") !== -1);
                }
                test.done();
            });
        });
    });
};


module.exports.testRemove = function(test) {
    test.ok(jb);
    test.ok(jb.isOpen());
    jb.findOne("birds", {"name" : "Molly"}, function(err, obj) {
        test.ifError(err);
        test.ok(obj["_id"]);
        test.equal(obj["mood"], "Very angry");
        //Bye bye Molly!
        jb.remove("birds", obj["_id"], function(err) {
            test.ifError(err);
            jb.findOne("birds", {"name" : "Molly"}, function(err, obj) {
                test.ifError(err);
                test.ok(obj === null);
                test.done();
            });
        });
    });
};


module.exports.testSync = function(test) {
    test.ok(jb);
    test.ok(jb.isOpen());
    jb.sync(function(err) {
        test.ifError(err);
        test.done();
    });
};

module.exports.testRemoveColls = function(test) {
    jb.dropCollection("birds", function(err) {
        test.ifError(err);
        jb.find("birds", {}, function(err, cursor, count) { //Query on not existing collection
            test.ifError(err);
            test.equal(count, 0);
            test.ok(!cursor.next());
            test.done();
        });
    });
};


module.exports.testTx1 = function(test) {
    test.ok(jb);
    test.ok(jb.isOpen());
    var obj = {
        foo : "bar"
    };
    test.ok(jb.getTransactionStatus("bars") === false);
    jb.beginTransaction("bars", function(err) {
        test.ifError(err);
        jb.save("bars", obj);
        var id = obj["_id"];
        test.ok(id);
        obj = jb.load("bars", obj["_id"]);
        test.ok(obj);
        test.ok(jb.getTransactionStatus("bars") === true);
        jb.rollbackTransaction("bars");
        test.ok(jb.getTransactionStatus("bars") === false);
        obj = jb.load("bars", obj["_id"]);
        test.ok(obj == null);
        jb.beginTransaction("bars", function(err) {
            test.ifError(err);
            test.ok(jb.getTransactionStatus("bars") === true);
            test.ok(jb.load("bars", id) == null);
            obj = {
                foo : "bar"
            };
            jb.save("bars", obj);
            id = obj["_id"];
            test.ok(id);
            test.ok(jb.load("bars", id));
            jb.commitTransaction("bars", function(err) {
                test.ifError(err);
                jb.getTransactionStatus("bars", function(err, status) {
                    test.ifError(err);
                    test.ok(status === false);
                    test.ok(jb.load("bars", id));
                    test.done();
                });
            });
        });
    });
};

module.exports.testCreateCollectionOn$upsert = function(test) {
    test.ok(jb);
    test.ok(jb.isOpen());
    jb.update("upsertcoll", {foo : "bar", $upsert : {foo : "bar"}}, function(err, count) {
        test.ifError(err);
        test.equal(count, 1);
        jb.findOne("upsertcoll", {foo : "bar"}, function(err, obj) {
            test.ifError(err);
            test.ok(obj);
            test.equal(obj.foo, "bar");
            test.done();
        });
    });
};


module.exports.testFPIssue = function(test) {
    test.ok(jb);
    jb.save("test", {x: 2.3434343});
    var x = jb.findOne("test");
    test.equal(x.x, 2.3434343);
    test.done();
};

module.exports.testEJDBCommand = function(test) {
    jb.command({
        "ping" : {}
    }, function(err, pong) {
        test.ifError(err);
        test.ok(pong);
        test.equal("pong", pong["log"]);
        test.done();
    })
};


module.exports.testClose = function(test) {
    test.ok(jb);
    jb.close();
    test.done();
};

