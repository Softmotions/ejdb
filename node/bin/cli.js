#!/usr/bin/env node

var cdb = null; //current DB desc
const maxInspectRows = 10;
const maxInspectDepth = 10;
var useColors = true;
var quiet = false;
var cmd = null;
var pkg = require("../../package.json");


//Parse aguments
(function() {
    var args = process.argv;
    for (var i = 2; i < args.length; ++i) {
        var a = args[i];
        if (["--help", "-h"].indexOf(a) !== -1) {
            help();
        } else if (["--no-colors", "-n"].indexOf(a) !== -1) {
            useColors = false;
        } else if (["--quiet", "-q"].indexOf(a) !== -1) {
            quiet = true;
        } else if (["--cmd", "-c"].indexOf(a) !== -1) {
            cmd = a;
        } else if (i === args.length - 1) { //last arg
            cmd = "db.open('" + a + "')";  //todo review
        }
    }
})();

function help() {
    var h = [];
    h.push("EJDB CLI v" + pkg.version);
    h.push("usage: ejdb [options] [dbfile]");
    h.push("options:");
    h.push("\t-h --help\tshow this help tip");
    h.push("\t-n --no-colors\tdo not use colored output");
    h.push("\t-q --quiet\trun in quiet output mode");
    h.push("\t-c --cmd\trun specified javascript command");
    console.error(h.join("\n"));
    process.exit(0);
}

if (!quiet) {
    console.log("Welcome to EJDB CLI v" + pkg.version);
}

var util = require("util");
var path = require("path");
var EJDB = require("../ejdb.js");
var clinspect = require("../clinspect.js");


//Init help hints
Object.defineProperty(EJDB.open, "_help_",
        {value : "(dbFile, [openMode]) Open database"});
Object.defineProperty(EJDB.prototype.close, "_help_",
        {value : "Close database"});
Object.defineProperty(EJDB.prototype.isOpen, "_help_",
        {value : "Check if database in opened state"});
Object.defineProperty(EJDB.prototype.ensureCollection, "_help_",
        {value : "(cname, [copts]) Creates new collection if it does't exists"});
Object.defineProperty(EJDB.prototype.dropCollection, "_help_",
        {value : "(cname, [prune], [cb]) Drop collection, " +
                "if `prune` is true collection db files will be erased from disk."});
Object.defineProperty(EJDB.prototype.save, "_help_",
        {value : "(cname, object|array of object, [opts], [cb]) " +
                "Save/update specified JSON objects in the collection."});
Object.defineProperty(EJDB.prototype.load, "_help_",
        {value : "(cname, oid, [cb]) Loads object identified by OID from the collection"});
Object.defineProperty(EJDB.prototype.remove, "_help_",
        {value : "(cname, oid, [cb]) Removes object from the collection"});
Object.defineProperty(EJDB.prototype.find, "_help_",
        {value : "(cname, [qobj], [qobjarr], [hints], [cb]) Execute query on collection"});
Object.defineProperty(EJDB.prototype.findOne, "_help_",
        {value : "(cname, [qobj], [qobjarr], [hints], [cb]) Retrive one object from the collection"});
Object.defineProperty(EJDB.prototype.update, "_help_",
        {value : "(cname, [qobj], [qobjarr], [hints], [cb]) Perform update query on collection"});
Object.defineProperty(EJDB.prototype.count, "_help_",
        {value : "(cname, [qobj], [qobjarr], [hints], [cb]) Convenient count(*) operation"});
Object.defineProperty(EJDB.prototype.sync, "_help_",
        {value : "Synchronize entire EJDB database with disk"});
Object.defineProperty(EJDB.prototype.dropIndexes, "_help_",
        {value : "(cname, path, [cb]) Drop indexes of all types for JSON field path"});
Object.defineProperty(EJDB.prototype.optimizeIndexes, "_help_",
        {value : "(cname, path, [cb]) Optimize indexes of all types for JSON field path"});

Object.defineProperty(EJDB.prototype.ensureStringIndex, "_help_",
        {value : "(cname, path, [cb]) Ensure String index for JSON field path"});
Object.defineProperty(EJDB.prototype.rebuildStringIndex, "_help_",
        {value : "(cname, path, [cb])"});
Object.defineProperty(EJDB.prototype.dropStringIndex, "_help_",
        {value : "(cname, path, [cb])"});

Object.defineProperty(EJDB.prototype.ensureIStringIndex, "_help_",
        {value : "(cname, path, [cb]) Ensure case insensitive String index for JSON field path"});
Object.defineProperty(EJDB.prototype.rebuildIStringIndex, "_help_",
        {value : "(cname, path, [cb])"});
Object.defineProperty(EJDB.prototype.dropIStringIndex, "_help_",
        {value : "(cname, path, [cb])"});

Object.defineProperty(EJDB.prototype.ensureNumberIndex, "_help_",
        {value : "(cname, path, [cb]) Ensure index presence of Number type for JSON field path"});
Object.defineProperty(EJDB.prototype.rebuildNumberIndex, "_help_",
        {value : "(cname, path, [cb])"});
Object.defineProperty(EJDB.prototype.dropNumberIndex, "_help_",
        {value : "(cname, path, [cb])"});

Object.defineProperty(EJDB.prototype.ensureArrayIndex, "_help_",
        {value : "(cname, path, [cb]) Ensure index presence of Array type for JSON field path"});
Object.defineProperty(EJDB.prototype.rebuildArrayIndex, "_help_",
        {value : "(cname, path, [cb])"});
Object.defineProperty(EJDB.prototype.dropArrayIndex, "_help_",
        {value : "(cname, path, [cb])"});

Object.defineProperty(EJDB.prototype.getDBMeta, "_help_",
        {value : "Get description of EJDB database and its collections"});

Object.defineProperty(EJDB.prototype.beginTransaction, "_help_",
        {value : "Begin collection transaction"});

Object.defineProperty(EJDB.prototype.commitTransaction, "_help_",
        {value : "Commit collection transaction"});

Object.defineProperty(EJDB.prototype.rollbackTransaction, "_help_",
        {value : "Rollback collection transaction"});

Object.defineProperty(EJDB.prototype.getTransactionStatus, "_help_",
        {value : "Get collection transaction status"});

repl = require("repl").start({
    prompt : "ejdb> ",
    input : process.stdin,
    output : process.stdout,
    terminal : true,
    writer : function(obj) {
        return clinspect.inspect(obj, maxInspectDepth, useColors)
    }
});

//console.log("MF=" +  module.filename);

var dbctl = {
    open : function(dbpath) {
        if (dbpath == null) {
            return error("No file path specified");
        }
        if (cdb) {
            return error("Database already opened: " + cdb.dbpath);
        }
        dbpath = path.resolve(dbpath);
        cdb = {
            dbpath : dbpath,
            jb : EJDB.open(dbpath)
        };
        syncdbctx();
        return dbstatus(cdb);
    },

    status : function() {
        return dbstatus(cdb);
    },

    close : function() {
        if (!cdb || !cdb.jb) {
            return error("Database already closed");
        }
        try {
            cdb.jb.close();
        } finally {
            cdb = null;
        }
        syncdbctx();
    }
};
Object.defineProperty(dbctl.open, "_help_", {value : EJDB.open._help_});
Object.defineProperty(dbctl.close, "_help_", {value : EJDB.prototype.close._help_});
Object.defineProperty(dbctl.status, "_help_", {value : "Get current database status"});

repl.on("exit", function() {
    dbctl.close();
    console.log("Bye!");
});

function dbstatus(cdb) {
    if (cdb) {
        return cdb.jb.getDBMeta();
    } else {
        return {};
    }
}

function syncdbctx() {
    var db = {};
    repl.resetContext();
    if (cdb && cdb.jb) {
        db.__proto__ = cdb.jb;
        db.close = dbctl.close;
        db.status = dbctl.status;
        db.find = function() {
            var ret = cdb.jb.find.apply(cdb.jb, arguments);
            if (typeof ret === "object") {
                if (!quiet) println("Found " + ret.length + " records");
                for (var i = 0; ret.next() && i < maxInspectRows; ++i) {
                    println(repl.writer(ret.object()));
                }
                ret.reset();
                if (ret.length > maxInspectRows) {
                    if (!quiet) println("Shown only first " + maxInspectRows);
                }
                if (!quiet) println("\nReturned cursor:");
            }
            return ret;
        };
        Object.defineProperty(db.find, "_help_", {value : EJDB.prototype.find._help_});
    } else {
        db.__proto__ = dbctl;
    }
    repl.context.db = db;
    repl.context.EJDB = EJDB;
}

function println(msg) {
    repl.outputStream.write(msg + "\n");
}

function error(msg) {
    return "ERROR: " + msg;
}

syncdbctx();


if (cmd) {
    repl.rli.write(cmd + "\n");
}
