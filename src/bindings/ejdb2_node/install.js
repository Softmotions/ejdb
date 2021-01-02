/**************************************************************************************************
 * EJDB2 Node.js native API binding.
 *
 * MIT License
 *
 * Copyright (c) 2012-2021 Softmotions Ltd <info@softmotions.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *************************************************************************************************/

const { promisify } = require('util');
const fs = require('fs');
const os = require('os');
const path = require('path');
const rimraf = promisify(require('rimraf'));
const extract = promisify(require('extract-zip'));
const readdir = promisify(fs.readdir);

const utils = require('./utils');
const REVISION = require('./package.json')['revision'];

function hasRevision() {
  return REVISION && REVISION.length && REVISION != '@GIT_REVISION@';
}

async function install() {

  let out = await utils.runProcessAndGetOutput('cmake', ['--version']).catch(() => {
    console.error('Unable to find executable');
    process.exit(1);
  });
  console.log(out);

  out = await utils.runProcessAndGetOutput('make', ['--version']).catch(() => {
    console.error('Unable to find executable');
    process.exit(1);
  });
  console.log(out);

  console.log('Building EJDB2 native binding...');
  const wdir = await promisify(fs.mkdtemp)(path.join(os.tmpdir(), 'ejdb2-node'));
  console.log(`Git revision: ${REVISION}`);
  console.log(`Build temp dir: ${wdir}`);

  let dist = path.join(wdir, 'dist.zip');
  await utils.download(`https://github.com/Softmotions/ejdb/archive/${REVISION}.zip`, dist);
  await extract(dist, { dir: wdir });

  dist = (await readdir(wdir)).find(fn => fn.startsWith(`ejdb-${REVISION}`));
  if (dist == null) throw Error(`Invalid distrib dir ${wdir}`);
  dist = path.join(wdir, dist);

  const buildDir = path.join(dist, 'build');
  fs.mkdirSync(buildDir);

  await utils.runProcess(
    'cmake',
    ['..', '-DCMAKE_BUILD_TYPE=Release', '-DBUILD_NODEJS_BINDING=ON', `-DNODE_BIN_ROOT=${__dirname}`],
    buildDir);
  await utils.runProcess('make', [], buildDir);
  await rimraf(wdir);
}

if (process.platform.toLowerCase().indexOf('win') == 0) { // Windows system
  console.error('Building for windows is currently not supported');
  process.exit(1);
}
if (hasRevision() && !fs.existsSync(path.join(utils.binariesDir), 'ejdb2_node.node')) {
  install().catch((err) => {
    console.error(err);
    process.exit(1);
  });
}
