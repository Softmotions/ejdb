//EJDB console


var path = require("path");
var EJDB = require("ejdb");

var cdb = null; //current DB desc

repl = require("repl").start({
    prompt : "ejc> ",
    input : process.stdin,
    output : process.stdout,
    terminal : true,
    useColors : true
});

//console.log("repl.context=" + repl.context);

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

repl.on("exit", function() {
    dbctl.close();
    console.log("Bye!");
});

function dbstatus(cdb) {
    if (cdb) {
        var status = {
            dbpath : cdb.dbpath,
            opened : !!(cdb.jb && cdb.jb.isOpen()),
            collections : []
        };
        //todo fill collections and indexes
        return status;
    } else {
        return {};
    }
}

function syncdbctx() {
    var db = {};
    repl.resetContext();
    if (cdb && cdb.jb) {
        db.__proto__ = cdb.jb;
        db["close"] = dbctl.close.bind(dbctl);
        db["status"] = dbctl.status.bind(dbctl);
        repl.context.db = db;
    } else {
        db.__proto__ = dbctl;
        repl.context.db = db;
    }
}

function error(msg) {
    return "ERROR: " + msg;
}

syncdbctx();





















