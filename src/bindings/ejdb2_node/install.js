/**************************************************************************************************
 * EJDB2 Node.js native API binding.
 *
 * MIT License
 *
 * Copyright (c) 2012-2022 Softmotions Ltd <info@softmotions.com>
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

import { access, mkdtemp, readdir, rm } from "node:fs/promises";
import { createRequire } from "node:module";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import extract from "extract-zip";
import { binariesDir, download, runProcess } from "./utils.js";

const REVISION = createRequire(import.meta.url)("./package.json").revision;
const moduleDir = dirname(fileURLToPath(import.meta.url));

function hasRevision() {
  return REVISION && REVISION.length && REVISION != "@META_REVISION@";
}

async function install() {
  console.log("Building EJDB2 native binding...");
  const wdir = await mkdtemp(join(tmpdir(), "ejdb2-node"));
  console.log(`Git revision: ${REVISION}`);
  console.log(`Build temp dir: ${wdir}`);

  let dist = join(wdir, "dist.zip");
  await download(`https://github.com/Softmotions/ejdb/archive/${REVISION}.zip`, dist);
  await extract(dist, { dir: wdir });

  dist = (await readdir(wdir)).find((fn) => fn.startsWith(`ejdb-${REVISION}`));
  if (dist == null) throw Error(`Invalid distrib dir ${wdir}`);
  dist = join(wdir, dist);

  await runProcess(
    "build.sh",
    ["-DBUILD_TYPE=Release", "-DBUILD_BINDING_NODEJS=1", `-DEJDB_NODE_BIN_ROOT=${moduleDir}`],
    dist
  );
  await rm(wdir, { recursive: true, force: true });
}

async function main() {
  if (!hasRevision()) {
    return;
  }
  try {
    await access(join(binariesDir, "ejdb2_node.node"));
  } catch {
    await install();
  }
}

if (process.platform.toLowerCase().indexOf("win") == 0) {
  // Windows system
  console.error("Building for windows is currently not supported");
  process.exit(1);
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
