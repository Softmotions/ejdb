var butils = require("../benchutils.js");
var EJDB = require("ejdb");
var jb = EJDB.open("bench1", EJDB.DEFAULT_OPEN_MODE | EJDB.JBOTRUNC);


function run(opts, prepareCb, saveCb) {
    var rows = opts["rows"];  //max number of rows for each iteration
    var queries = opts["queries"]; //max number of queries
    if (rows == null || isNaN(rows)) {
        throw new Error("Missing required 'rows' option");
    }
    if (queries == null || isNaN(queries)) {
        throw new Error("Missing required 'queries' option");
    }
    if (queries > rows) {
        queries = rows;
    }
    var maxfields = opts["maxfields"] || 10;       //max number of fields on each object nesting level
    var maxlevels = opts["maxlevels"] || 3;        //max number of nested object levels
    var lvlindexedfields = opts["lvlindexedfields"] != null ? opts["lvlindexedfields"] : 2; //max number of indexed field for each level

    var qfields = {};
    var types = ["s", "i", "f", "a", "ia", "o"];
    var parents = [];
    parents[0] = {};
    for (var l = 0; l < maxlevels; ++l) {
        var inum = lvlindexedfields;
        for (var f = 0; f < maxfields; ++f) {
            var fname = butils.randomField(32);
            var fo = {
                "_t" : types[butils.randomInt32(0, types.length - 1)]
            };
            if (fo._t !== "o" && inum > 0) {
                fo._i = true; //field indexed
                --inum;
            }
            if (fo._t === "o") {
                parents[l + 1] = fo;
            }
            if (parents[l]) {
                parents[l][fname] = fo;
            }
        }
    }
    function constructObj(meta, out, fstack, qinc) {
        if (fstack == null) {
            fstack = [];
        }
        for (var k in meta) {
            if (k[0] === "_" || typeof meta[k] !== "object") {
                continue;
            }
            fstack.push(k);
            var fo = meta[k];
            switch (fo._t) {
                case "s":
                    out[k] = butils.randomString(butils.randomInt32(1, 256));
                    break;
                case "a":
                    out[k] = butils.randomStringArr(butils.randomInt32(1, 64), 128);
                    break;
                case "i":
                    out[k] = butils.randomInt32();
                    break;
                case "f":
                    out[k] = butils.randomFloat();
                    break;
                case "ia":
                    out[k] = butils.randomIntArr(butils.randomInt32(1, 64));
                    break;
                case "o":
                    out[k] = constructObj(fo, {}, fstack, qinc);
                    break;
            }
            if (fo._t !== "o") {
                var fname = fstack.join(".");
                if (qfields[fname] == null) {
                    qfields[fname] = {_i : !!fo._i, _t : fo._t, _v : []};
                }
                if (qinc) {
                    qfields[fname]._v.push(out[k]);
                }
            }
            fstack.pop();
        }
        return out;
    }

    var util = require("util");
    var st = +new Date();


    constructObj(parents[0], {}, [], false); //populate qfields
    prepareCb(opts, qfields);

    var qratio = Math.ceil(rows / queries);
    for (var r = 0; r < rows; ++r) {
        var isq = ((r % qratio) == 0);
        var obj = constructObj(parents[0], {}, [], isq);
        saveCb(opts, obj);
    }


    return +new Date() - st;
}

var time = run({
            cname : "run1",
            rows : 100000,
            queries : 1000,
            maxfields : 10,
            maxlevels : 3,
            lvlindexedfields : 2
        }, function(opts, qfields) {
            jb.ensureCollection(opts.cname, {records : opts.rows, large : true});
            for (var k in qfields) {
                var qf = qfields[k];
                if (!qf._i) continue;
                switch (qf._t) {
                    case "i":
                    case "f":
                        jb.ensureNumberIndex(opts.cname, k);
                        break;
                    case "s":
                        jb.ensureStringIndex(opts.cname, k);
                        break;
                    case "a":
                    case "ia":
                        jb.ensureArrayIndex(opts.cname, k);
                }
            }
        },
        function(opts, obj) {
            jb.save(opts.cname, obj);
        });

console.log("run1 time=%d", time);

//cleanup
if (false) {
    var c = 0;
    jb.getDBMeta()["collections"].forEach(function(co) {
        console.log("Drop collection %s", co.name);
        ++c;
        jb.dropCollection(co.name, true, function() {
            --c;
            if (c == 0) {
                jb.close();
            }

        });
    });
} else {
    jb.close();
}


