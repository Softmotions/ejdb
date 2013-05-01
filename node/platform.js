var cmd = process.argv[2];
var exec = require("child_process").exec;
var spawn = require("child_process").spawn;
var fs = require("fs");
var path = require("path");
var http = require("http");
var util = require("util");
var AdmZip = require("adm-zip");

if (process.platform === "win32") {
    win();
} else {
    nix();
}

function exithandler(cmd, cb) {
    return function(code) {
        if (code !== 0) {
            console.log("" + cmd + " process exited with code " + code);
            process.exit(code);
        }
        if (cb) {
            cb();
        }
    }
}

function nix() {

    switch (cmd) {

        case "preinstall":
        {
            console.log("Building EJDB...");
            var m = spawn("make", ["all"], {"stdio" : "inherit"});
            m.on("close", exithandler("make all", function() {
                var ng = spawn("node-gyp", ["rebuild"], {stdio : "inherit"});
                ng.on("close", exithandler("node-gyp"));
            }));
            break;
        }
        case "test":
        {
            console.log("Tesing Node EJDB...");
            var m = spawn("make", ["-f", "tests.mk", "check-all"], {stdio : "inherit"});
            m.on("close", exithandler("make"));
        }
    }
}


function win() {

    switch (cmd) {

        case "preinstall":
        {
            var dlurl = process.env["npm_package_config_windownloadurl"] || "http://dl.dropboxusercontent.com/u/4709222/ejdb/tcejdb-1.1.3-mingw32-i686.zip";
            if (dlurl == null) {
                console.log("Invalid package configuration, missing windows binaries download url");
                process.exit(1);
            }
            var sdir = "ejdbdll";
            try {
                fs.statSync(sdir);
            } catch (e) {
                if ("ENOENT" !== e.code) {
                    throw e;
                }
                fs.mkdirSync(sdir);
            }

            var zfileExist = false;
            var zfile = path.join(sdir, path.basename(dlurl));
            try {
                fs.statSync(zfile);
                zfileExist = true;
            } catch (e) {
                if ("ENOENT" !== e.code) {
                    throw e;
                }
            }

            if (!zfileExist) {
                console.log("Downloading windows binaries from: %s ...", dlurl);
                console.log("File: %s", zfile);
                var req = http.get(dlurl, function(res) {
                    if (res.statusCode !== 200) {
                        console.log("Invalid response code %d", res.statusCode);
                        process.exit(1);
                    }
                    var len = 0;
                    var cnt = 0;
                    var wf = fs.createWriteStream(zfile);
                    var eh = function(ev) {
                        console.log("Error receiving data from %s Error: %s", dlurl, ev);
                        process.exit(1);
                    };
                    wf.on("error", eh);
                    res.on("error", eh);
                    res.on("data", function(chunk) {
                        if (++cnt % 80 == 0) {
                            process.stdout.write("\n");
                        }
                        len += chunk.length;
                        process.stdout.write(".");
                    });
                    res.on("end", function() {
                        console.log("\n%d bytes downloaded", len);
                        processArchive();
                    });
                    res.pipe(wf);
                });
                req.end();
            } else {
                processArchive();
            }

            function processArchive() {
                console.log("Unzip archive '%s'", zfile);
                var azip = new AdmZip(zfile);
                azip.extractAllTo(sdir, true);
                sdir = path.resolve(sdir);
                var args = ["rebuild", util.format("-DEJDB_HOME=%s", sdir)];
                console.log("node-gyp %j", args);
                var ng = spawn("node-gyp", args, {stdio : "inherit"});
                ng.on("close", exithandler("node-gyp"));
            }
        }
    }


}