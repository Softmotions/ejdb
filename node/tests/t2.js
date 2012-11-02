var fs = require("fs");
var EJDB = require("../ejdb.js");


var jb = null;

module.exports.testSetup = function(test) {
    jb = EJDB.open("var/tdbt2", EJDB.JBOWRITER | EJDB.JBOCREAT | EJDB.JBOTRUNC);
    test.done();
};

module.exports.testSaveLoad = function(test) {
    test.ok(jb.isOpen());
    var parrot1 = {
        "name" : "Grenny",
        "type" : "African Grey",
        "male" : true,
        "age" : 1,
        "likes" : ["green color", "night", "toys"]
    };
    var parrot2 = {
        "name" : "Bounty",
        "type" : "Cockatoo",
        "male" : false,
        "age" : 15,
        "likes" : ["sugar cane"]
    };
    jb.save("parrots", [parrot1, parrot2], function(err, oids) {
        test.ifError(err);
        test.ok(oids);
        test.equal(oids.length, 2);
        test.done();
    });
};

module.exports.testClose = function(test) {
    test.ok(jb);
    jb.close();
    test.done();
};

