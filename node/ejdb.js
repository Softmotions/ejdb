
var ejdblib;
try {
    ejdblib = require("../build/Release/ejdb_native.node");
} catch(e) {
    console.error("Warning: Using the DEBUG version of EJDB nodejs binding");
    ejdblib = require("../build/Debug/ejdb_native.node");
}
var EJDBImpl = ejdblib.NodeEJDB;

const DEFAULT_OPEN_MODE = (ejdblib.JBOWRITER | ejdblib.JBOCREAT);
var EJDB = function(dbFile, openMode) {
    this._impl = new EJDBImpl(dbFile, (openMode > 0) ? openMode : DEFAULT_OPEN_MODE);
    return this;
};

for (var k in ejdblib) { //Export constants
    if (k.indexOf("JB") === 0) {
        EJDB[k] = ejdblib[k];
    }
}
EJDB.DEFAULT_OPEN_MODE = DEFAULT_OPEN_MODE;

EJDB.open = function(dbFile, openMode) {
    return new EJDB(dbFile, openMode);
};

EJDB.prototype.close = function() {
    return this._impl.close();
};


module.exports = EJDB;

