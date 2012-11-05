var fs = require("fs");
var EJDB = require("../ejdb.js");

module.exports.testOpenClose = function(test) {
    test.ok(EJDB.JBOWRITER);
    test.ok(EJDB.JBOCREAT);
    test.ok(EJDB.JBOTRUNC);
    var jb = EJDB.open("var/tdbt11", EJDB.JBOWRITER | EJDB.JBOCREAT | EJDB.JBOTRUNC);
    test.ok(jb);
    test.equal(jb.isOpen(), true);
    jb.close();
    test.equal(jb.isOpen(), false);
    test.ok(fs.existsSync("var/tdbt11"));
    test.done();
};


module.exports.testEnsureAndRemoveCollection = function(test) {
    var jb = EJDB.open("var/tdbt12", EJDB.JBOWRITER | EJDB.JBOCREAT | EJDB.JBOTRUNC);
    test.equal(jb.isOpen(), true);
    var c1opts = {
        cachedrecords : 10000,
        compressed : true,
        large : true,
        records : 1000000
    };
    jb.ensureCollection("c1", c1opts);
    test.ok(fs.existsSync("var/tdbt12_c1"));

    jb.removeCollection("c1", true, function(err) {
        test.ifError(err);
        test.ok(!fs.existsSync("var/tdbt12_c1"));

        //Test operation on closed database instance
        jb.close();
        var err = null;
        try {
            jb.removeCollection("c1", true);
        } catch (e) {
            err = e;
        }
        test.ok(err);
        test.done();
    });
};




