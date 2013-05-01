var cmd = process.argv[2];
var exec = require("child_process").exec;
var spawn = require("child_process").spawn;
var fs = require("fs");
var path = require("path");
var http = require("http");
var util = require("util");

if (process.platform === "win32") {
    win();
} else {
    nix();
}

function exithandler(cmd, cb) {
    return function(code) {
        if (code != null && code !== 0) {
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
            var config = {};
            fs.writeFileSync("configure.gypi", JSON.stringify(config));

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
                        console.log("\n%d bytes received", len);
                        setTimeout(processArchive, 2000);
                    });
                    res.pipe(wf);
                });
                req.end();
            } else {
                processArchive();
            }

            function processArchive() {
                var AdmZip = require("adm-zip");
                console.log("Unzip archive '%s'", zfile);
                var azip = new AdmZip(zfile);
                azip.extractAllTo(sdir, true);
                sdir = path.resolve(sdir);

                var config = {};
                config["variables"] = {
                    "EJDB_HOME" : sdir
                };
                fs.writeFileSync("configure.gypi", JSON.stringify(config));

                var args = ["configure", "rebuild"];
                console.log("node-gyp %j", args);
                var ng = spawn("node-gyp.cmd", args, {stdio : "inherit"});
                ng.on("error", function(ev) {
                    console.log("Spawn error: " + ev);
                    process.exit(1);
                });
                ng.on("close", exithandler("node-gyp", function() {
                    copyFile(path.join(sdir, "lib/tcejdbdll.dll"),
                            "build/Release/tcejdbdll.dll",
                            exithandler("copy tcejdbdll.dll"));
                }));
            }
        }
    }
}


function copyFile(source, target, cb) {
    var cbCalled = false;
    var rd = fs.createReadStream(source);
    rd.on("error", function(err) {
        done(err);
    });
    var wr = fs.createWriteStream(target);
    wr.on("error", function(err) {
        done(err);
    });
    wr.on("close", function(ex) {
        done();
    });
    rd.pipe(wr);
    function done(err) {
        if (!cbCalled) {
            cb(err);
            cbCalled = true;
        }
    }
}