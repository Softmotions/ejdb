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

import { spawn } from "node:child_process";
import { createWriteStream } from "node:fs";
import Gauge from "gauge";
import request from "request";
import progress from "request-progress";

let platform = process.platform.toLowerCase();
if (platform.indexOf("win") === 0) {
  platform = "windows";
}

export const binariesDir = `${platform}-${process.arch}`;

/**
 * @param {String} name
 * @param {Array<String>} [args]
 * @param {String} [cwd]
 * @return {Promise<void>}
 */
export function runProcess(name, args, cwd) {
  args = args || [];
  console.log(`Spawn: ${name} ${args}`);
  return awaitProcess(spawn(name, args, {
    stdio: ["ignore", process.stdout, process.stderr],
    cwd
  }));
}

export function runProcessAndGetOutput(name, args, cwd) {
  const res = [];
  args = args || [];
  console.log(`Spawn: ${name} ${args}`);
  const proc = spawn(name, args, {
    stdio: ["ignore", "pipe", "pipe"],
    cwd
  });
  proc.stdout.on("data", (data) => res.push(data));
  proc.stderr.on("data", (data) => res.push(data));
  return awaitProcess(proc).then(() => {
    return res.join();
  });
}

/**
 * @return {Promise<void>}
 */
export function awaitProcess(proc) {
  return new Promise((resolve, reject) => {
    proc.once("exit", (code) => {
      if (code === 0) {
        resolve(undefined);
      } else {
        reject(new Error("Exit with error code: " + code));
      }
    });
    proc.once("error", (err) => reject(err));
  });
}

/**
 * @param {String} url
 * @param {String} dest
 * @return {Promise<String>}
 */
export function download(url, dest, name) {
  const gauge = new Gauge();
  return new Promise((resolve, reject) => {
    let completed = false;
    function handleError(err) {
      if (!completed) {
        gauge.hide();
        reject(err);
        completed = true;
      }
    }
    progress(request(url), {
      throttle: 500,
      delay: 0
    })
      .on("progress", (state) => gauge.show(name || url, state.percent))
      .on("error", (err) => handleError(err))
      .on("end", () => {
        if (!completed) {
          gauge.hide();
          resolve(dest);
          completed = true;
        }
      })
      .on("response", function (response) {
        if (response.statusCode >= 400) {
          handleError(`HTTP ${response.statusCode}: ${url}`);
        }
      })
      .pipe(createWriteStream(dest));
  });
}
