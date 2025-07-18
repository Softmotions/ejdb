const detox = require('detox');
const adapter = require('detox/runners/mocha/adapter');
const execFile = require('child_process').execFile;
let config = require('../package.json').detox;

if (process.env['ANDROID_AVD'] != null) {
  config = config.replace(/"name":\s?"TestingAVD"/, `"name": ${process.env['ANDROID_AVD']}`)
}

let server;

before(async () => {
  server = execFile('yarn', ['run', 'start'], {});
  await detox.init(config);
});

beforeEach(async function () {
  await adapter.beforeEach(this);
});

afterEach(async function () {
  await adapter.afterEach(this);
});

after(async () => {
  await detox.cleanup();
  execFile('yarn', ['run', 'stop'], {});
});
