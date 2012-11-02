var EJDB = require("../ejdb.js");

module.exports.testOpenClose = function(test) {
    var jb = EJDB.open("var/tdb1");
    test.done();
}