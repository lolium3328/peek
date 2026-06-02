#!/usr/bin/env bun
import { execSync } from "node:child_process";
import { closeSync, openSync, readFileSync, readSync, writeFileSync, writeSync } from "node:fs";

const command = process.argv[2];
const args = process.argv.slice(3);

function usage() {
  console.error("Usage:");
  console.error("  bun esp-fs download <remote-path> <local-path>");
  console.error("  bun esp-fs upload <local-path> <remote-path>");
  console.error("  bun esp-fs stat");
  console.error("");
  console.error("Environment:");
  console.error("  PEEK_ESP_PORT    Serial port (default: /dev/ttyACM0)");
  process.exit(1);
}

if (!command) usage();

const port = process.env.PEEK_ESP_PORT || "/dev/ttyACM0";

function configurePort() {
  try {
    execSync(`stty -f "${port}" 115200 raw -echo 2>/dev/null`, { stdio: "ignore" });
  } catch {
    try {
      execSync(`stty -F "${port}" 115200 raw -echo 2>/dev/null`, { stdio: "ignore" });
    } catch {
      console.error(`Failed to configure serial port: ${port}`);
      console.error("Try: stty -f ${port} 115200 raw -echo");
      process.exit(1);
    }
  }
}

function sleep(ms: number) {
  const target = Date.now() + ms;
  while (Date.now() < target) {
    // busy wait
  }
}

function readResponse(fd: number): string {
  const buf = Buffer.alloc(1);
  // Skip any non-">" prefixed lines (stale log output)
  while (true) {
    let c = 0;
    while (c !== 0x3e /* '>' */) {
      if (readSync(fd, buf, 0, 1, null) !== 1) continue;
      c = buf[0];
    }
    // Read until newline
    let line = "";
    while (true) {
      if (readSync(fd, buf, 0, 1, null) !== 1) continue;
      if (buf[0] === 0x0a) break; // '\n'
      line += String.fromCharCode(buf[0]);
    }
    return line;
  }
}

function sendCommand(fd: number, cmd: string): string {
  writeSync(fd, Buffer.from(cmd + "\n"));
  return readResponse(fd);
}

function cmdStat() {
  configurePort();
  const fd = openSync(port, "r+");
  try {
    const resp = sendCommand(fd, ">STAT");
    console.log(resp);
  } finally {
    closeSync(fd);
  }
}

function cmdDownload(remotePath: string, localPath: string) {
  configurePort();
  const fd = openSync(port, "r+");
  try {
    const resp = sendCommand(fd, `>READ ${remotePath}`);
    if (resp.startsWith("OK ")) {
      const base64Data = resp.slice(3);
      const buffer = Buffer.from(base64Data, "base64");
      writeFileSync(localPath, buffer);
      console.log(`Downloaded ${buffer.length} bytes to ${localPath}`);
    } else {
      console.error("Error:", resp);
      process.exit(1);
    }
  } finally {
    closeSync(fd);
  }
}

function cmdUpload(localPath: string, remotePath: string) {
  const fileBuffer = readFileSync(localPath);
  const base64Data = fileBuffer.toString("base64");

  configurePort();
  const fd = openSync(port, "r+");
  try {
    const resp = sendCommand(fd, `>WRITE ${remotePath} ${base64Data}`);
    if (resp === "OK") {
      console.log(`Uploaded ${fileBuffer.length} bytes to ${remotePath}`);
    } else {
      console.error("Error:", resp);
      process.exit(1);
    }
  } finally {
    closeSync(fd);
  }
}

try {
  if (command === "stat") {
    cmdStat();
  } else if (command === "download") {
    if (args.length < 2) usage();
    cmdDownload(args[0], args[1]);
  } else if (command === "upload") {
    if (args.length < 2) usage();
    cmdUpload(args[0], args[1]);
  } else {
    usage();
  }
} catch (err) {
  console.error("Error:", (err as Error).message);
  process.exit(1);
}
