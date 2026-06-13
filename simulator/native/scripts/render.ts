#!/usr/bin/env bun
import { mkdirSync } from "node:fs";
import { dirname, join, normalize } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const scriptDir = dirname(fileURLToPath(import.meta.url));
const nativeRoot = normalize(join(scriptDir, ".."));
const repoRoot = normalize(join(nativeRoot, "..", ".."));
const buildDir = join(repoRoot, ".peek-preview", "simulator-build");
const binaryPath = join(buildDir, "peek-simulator-render");

function main() {
  const args = process.argv.slice(2);
  buildRunner();
  runRunner(args);
}

function buildRunner() {
  mkdirSync(buildDir, { recursive: true });
  const compiler = findCompiler();
  const sources = [
    join(nativeRoot, "src", "main.cpp"),
    join(nativeRoot, "src", "HostDisplayDriver.cpp"),
    join(repoRoot, "src", "ui", "ScreenRenderer.cpp")
  ];
  const args = [
    "-std=c++17",
    "-DPEEK_HOST_PREVIEW",
    "-I", join(nativeRoot, "include"),
    "-I", join(repoRoot, "include"),
    ...sources,
    "-lz",
    "-o", binaryPath
  ];

  const result = spawnSync(compiler, args, { cwd: repoRoot, stdio: "inherit" });
  if (result.status !== 0) {
    throw new Error(`Failed to build native simulator runner with ${compiler}`);
  }
}

function runRunner(args: string[]) {
  const result = spawnSync(binaryPath, args, { cwd: nativeRoot, stdio: "inherit" });
  if (result.status !== 0) {
    throw new Error("Native simulator runner failed");
  }
}

function findCompiler() {
  for (const compiler of ["clang++", "g++", "c++"]) {
    const result = spawnSync(compiler, ["--version"], { stdio: "ignore" });
    if (result.status === 0) return compiler;
  }
  throw new Error("Missing C++ compiler: expected clang++, g++, or c++");
}

main();
