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

/*

module.exports.testQuery1 = function(test) {
    test.ok(jb);
    test.ok(jb.isOpen());
    jb.query("parrots", {}, function(err, cursor, count) {
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
        test.done();
    });
};

*/

module.exports.testQuery2 = function(test) {
    test.ok(jb);
    test.ok(jb.isOpen());
    jb.query("parrots",
            {name : /(grenny|bounty)/ig},
            {$orderby : {name : 1}},
            function(err, cursor, count) {
                test.ifError(err);
                console.log("count=" + count);
                test.done();
            });

};

module.exports.testClose = function(test) {
    test.ok(jb);
    jb.close();
    test.done();
};

