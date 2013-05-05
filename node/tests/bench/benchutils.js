//var crypto = require("crypto");

if (typeof module != "undefined") {
    module.exports.randomFloat = randomFloat;
    module.exports.randomInt32 = randomInt32;
    module.exports.randomString = randomString;
    module.exports.randomStringArr = randomStringArr;
    module.exports.randomIntArr = randomIntArr;
    module.exports.randomField = randomField;
}

function randomFloat(min, max) {
    if (min == null || max == null) {
        //return crypto.randomBytes(64).readDoubleLE(0);
        return 2147483647 * Math.random() - 2147483647 * Math.random();
    } else {
        return Math.random() * (max - min) + min;
    }
}

function randomInt32(min, max) {
    if (min == null || max == 0) {
        //return crypto.randomBytes(32).readInt32LE(0);
        return Math.ceil(2147483647 * Math.random()) - Math.ceil(2147483647 * Math.random());
    } else {
        return Math.floor(Math.random() * (max - min + 1)) + min;
    }
}

function randomIntArr(maxarrlen, min, max) {
    var arr = new Array(maxarrlen);
    for (var i = 0; i < maxarrlen; ++i) {
        arr[i] = randomInt32(min, max);
    }
    return arr;
}

function randomStringArr(maxarrlen, maxstrlen) {
    var arr = new Array(maxarrlen);
    for (var i = 0; i < maxarrlen; ++i) {
        arr[i] = randomString(randomInt32(1, maxstrlen));
    }
    return arr;
}

function randomString(maxlen) {
    maxlen = (maxlen != null) ? maxlen : 256;
    var carr = new Buffer(maxlen);
    for (var i = 0; i < maxlen; ++i) {
        carr[i] = (0x21 + Math.floor(Math.random() * (0x7e - 0x21 + 1)));
    }
    return carr.toString("ascii");
}


function randomField() {
    var maxlen = randomInt32(1, 48);
    var carr = new Buffer(maxlen);
    for (var i = 0; i < maxlen; ++i) {
        carr[i] = (0x61 + Math.floor(Math.random() * (0x7a - 0x61 + 1)));
    }
    return carr.toString("ascii");
}
