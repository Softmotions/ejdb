var EJDB = require("../ejdb.js");

module.exports.testOpenClose = function(test) {
    var jb = EJDB.open("tdb1");
    test.done();
}