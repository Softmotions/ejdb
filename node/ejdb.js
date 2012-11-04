var ejdblib;
try {
    ejdblib = require("../build/Release/ejdb_native.node");
} catch (e) {
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

EJDB.prototype.isOpen = function() {
    return this._impl.isOpen();
};

EJDB.prototype.ensureCollection = function(cname, copts) {
    return this._impl.ensureCollection(cname, copts || {});
};

EJDB.prototype.rmCollection = function(cname, prune) {
    return this._impl.rmCollection(cname, !!prune);
};

EJDB.prototype.save = function(cname, jsarr, cb) {
    if (!jsarr) {
        return;
    }
    if (jsarr.constructor !== Array) {
        jsarr = [jsarr];
    }
    return this._impl.save(cname, jsarr, function(err, oids) {
        if (err) {
            if (cb) {
                cb(err);
            }
            return;
        }
        //Assign _id property for newly created objects
        for (var i = jsarr.length - 1; i >= 0; --i) {
            var so = jsarr[i];
            if (so != null && so["_id"] !== oids[i]) {
                so["_id"] = oids[i];
            }
        }
        if (cb) {
            cb(err, oids);
        }
    });
};

EJDB.prototype.load = function(cname, oid, cb) {
    return this._impl.load(cname, oid, cb);
};


EJDB.prototype.remove = function(cname, oid, cb) {
    return this._impl.remove(cname, oid, cb);
};

/**
 *
 * Calling variations:
 *       - query(cname, qobj, qobjarr, hints, cb)
 *       - query(cname, qobj, hints, cb)
 *       - query(cname, qobj, cb)
 *
 * @param cname
 * @param qobj
 * @param orarr
 * @param hints
 * @param cb
 */
EJDB.prototype.query = function(cname, qobj, orarr, hints, cb) {
    if (arguments.length == 4) {
        cb = hints;
        hints = orarr;
        orarr = [];
    } else if (arguments.length == 3) {
        cb = orarr;
        orarr = [];
        hints = {};
    }
    if (typeof cb !== "function") {
        throw new Error("Callback 'cb' argument must be specified");
    }
    if (typeof cname !== "string") {
        throw new Error("Collection name 'cname' argument must be specified");
    }
    if (typeof hints !== "object") {
        hints = {};
    }
    if (typeof qobj !== "object") {
        qobj = {};
    }
    return this._impl.query(cname,
            [qobj].concat(orarr, hints),
            (hints["onlycount"] ? ejdblib.JBQRYCOUNT : 0),
            cb);
};


module.exports = EJDB;

