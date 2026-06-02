#!/usr/bin/env bun
import { mkdirSync } from "node:fs";
import { dirname, join, normalize } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

type PreviewScreenMode = "home" | "homeFrame" | "boot" | "status" | "menu";

const modes = ["home", "homeFrame", "boot", "status", "menu"] as const;
const scriptDir = dirname(fileURLToPath(import.meta.url));
const repoRoot = normalize(join(scriptDir, "..", "..", ".."));
const buildDir = join(repoRoot, ".peek-preview", "build");
const binaryPath = join(buildDir, "peek-screen-preview");

async function main() {
  const args = parseArgs(process.argv.slice(2));
  const mode = args.mode ?? await chooseMode();
  buildPreviewRunner();
  runPreviewRunner(mode, args.out);
}

function parseArgs(argv: string[]) {
  const args: { mode?: PreviewScreenMode | "all"; out?: string } = {};
  for (let index = 0; index < argv.length; index++) {
    const arg = argv[index];
    if (arg === "--mode" || arg === "-m") {
      args.mode = parseMode(argv[++index]);
    } else if (arg.startsWith("--mode=")) {
      args.mode = parseMode(arg.slice("--mode=".length));
    } else if (arg === "--out" || arg === "-o") {
      args.out = normalize(argv[++index]);
    } else if (arg.startsWith("--out=")) {
      args.out = normalize(arg.slice("--out=".length));
    } else if (arg === "--help" || arg === "-h") {
      printHelp();
      process.exit(0);
    } else {
      throw new Error(`Unknown argument: ${arg}`);
    }
  }
  return args;
}

function parseMode(value: string | undefined): PreviewScreenMode | "all" {
  if (value === "all" || modes.includes(value as PreviewScreenMode)) {
    return value as PreviewScreenMode | "all";
  }
  throw new Error(`Expected --mode to be one of: all, ${modes.join(", ")}`);
}

async function chooseMode(): Promise<PreviewScreenMode | "all"> {
  if (!process.stdin.isTTY || !process.stdout.isTTY) {
    return "all";
  }

  const choices = ["all", ...modes] as const;
  let selected = 0;
  const render = () => {
    process.stdout.write("\x1b[2J\x1b[H");
    process.stdout.write("Firmware screen preview PNG\n\n");
    for (let index = 0; index < choices.length; index++) {
      process.stdout.write(`${index === selected ? ">" : " "} ${choices[index]}\n`);
    }
    process.stdout.write("\nUse arrows or j/k, Enter to generate, q to quit.\n");
  };

  return await new Promise((resolve) => {
    const stdin = process.stdin;
    stdin.setRawMode(true);
    stdin.resume();
    stdin.setEncoding("utf8");
    render();
    stdin.on("data", (key: string) => {
      if (key === "\u0003" || key === "q") {
        process.stdout.write("\n");
        process.exit(key === "\u0003" ? 130 : 0);
      }
      if (key === "\r" || key === "\n") {
        stdin.setRawMode(false);
        stdin.pause();
        process.stdout.write("\x1b[2J\x1b[H");
        resolve(choices[selected]);
        return;
      }
      if (key === "\u001b[A" || key === "k") {
        selected = (selected + choices.length - 1) % choices.length;
        render();
      } else if (key === "\u001b[B" || key === "j") {
        selected = (selected + 1) % choices.length;
        render();
      }
    });
  });
}

function buildPreviewRunner() {
  mkdirSync(buildDir, { recursive: true });
  const compiler = findCompiler();
  const sources = [
    join(repoRoot, "tools", "screen-preview", "src", "main.cpp"),
    join(repoRoot, "tools", "screen-preview", "src", "HostDisplayDriver.cpp"),
    join(repoRoot, "src", "ui", "ScreenRenderer.cpp")
  ];
  const args = [
    "-std=c++17",
    "-DPEEK_HOST_PREVIEW",
    "-I", join(repoRoot, "tools", "screen-preview", "include"),
    "-I", join(repoRoot, "include"),
    ...sources,
    "-lz",
    "-o", binaryPath
  ];

  const result = spawnSync(compiler, args, { cwd: repoRoot, stdio: "inherit" });
  if (result.status !== 0) {
    throw new Error(`Failed to build C++ screen preview runner with ${compiler}`);
  }
}

function runPreviewRunner(mode: PreviewScreenMode | "all", out?: string) {
  const args = ["--mode", mode];
  if (out) {
    args.push("--out", out);
  }
  const result = spawnSync(binaryPath, args, { cwd: repoRoot, stdio: "inherit" });
  if (result.status !== 0) {
    throw new Error("C++ screen preview runner failed");
  }
}

function findCompiler() {
  for (const compiler of ["clang++", "g++", "c++"]) {
    const result = spawnSync(compiler, ["--version"], { stdio: "ignore" });
    if (result.status === 0) {
      return compiler;
    }
  }
  throw new Error("Missing C++ compiler: expected clang++, g++, or c++");
}

function printHelp() {
  console.log(`Usage: bun run preview:png -- [options]

Options:
  -m, --mode <mode>       all, home, homeFrame, boot, status, or menu
  -o, --out <path>        output PNG path; with --mode all this is treated as an output directory
  -h, --help              show this help

Without --mode, an interactive terminal menu is shown. In non-TTY environments,
the tool defaults to --mode all and writes .peek-preview/*.png.`);
}

main().catch((error: unknown) => {
  console.error(error instanceof Error ? error.message : String(error));
  process.exit(1);
});
