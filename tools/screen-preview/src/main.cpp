#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "HostPreview.h"
#include "drivers/DisplayDriver.h"
#include "ui/ScreenRenderer.h"

namespace {
constexpr const char *kDefaultOutDir = ".peek-preview";

struct Args {
  std::string mode = "all";
  std::string out;
};

void printHelp() {
  std::cout << "Usage: peek-screen-preview [options]\n\n"
            << "Options:\n"
            << "  -m, --mode <mode>   all, home, homeFrame, boot, status, or menu\n"
            << "  -o, --out <path>    output PNG path; with --mode all this is treated as an output directory\n"
            << "  -h, --help          show this help\n";
}

bool isMode(const std::string &mode) {
  return mode == "all" || mode == "home" || mode == "homeFrame" || mode == "boot" || mode == "status" || mode == "menu";
}

Args parseArgs(int argc, char **argv) {
  Args args;
  for (int index = 1; index < argc; ++index) {
    const std::string arg = argv[index];
    if (arg == "--help" || arg == "-h") {
      printHelp();
      std::exit(0);
    }
    if (arg == "--mode" || arg == "-m") {
      if (index + 1 >= argc) {
        throw std::runtime_error("Missing value for " + arg);
      }
      args.mode = argv[++index];
      continue;
    }
    if (arg.rfind("--mode=", 0) == 0) {
      args.mode = arg.substr(7);
      continue;
    }
    if (arg == "--out" || arg == "-o") {
      if (index + 1 >= argc) {
        throw std::runtime_error("Missing value for " + arg);
      }
      args.out = argv[++index];
      continue;
    }
    if (arg.rfind("--out=", 0) == 0) {
      args.out = arg.substr(6);
      continue;
    }
    throw std::runtime_error("Unknown argument: " + arg);
  }
  if (!isMode(args.mode)) {
    throw std::runtime_error("Expected --mode to be one of: all, home, homeFrame, boot, status, menu");
  }
  return args;
}

std::vector<std::string> selectedModes(const std::string &mode) {
  if (mode == "all") {
    return {"home", "homeFrame", "boot", "status", "menu"};
  }
  return {mode};
}

HomeScreenModel homeModel(bool cubeVisible = true) {
  HomeScreenModel model;
  model.primaryText = cubeVisible ? "zzz..." : "imu?";
  model.hintText = "sleeping";
  model.localWeather = "--";
  model.peerWeather = "--";
  model.localLabel = "A";
  model.peerLabel = "B";
  model.wifiConnected = false;
  model.backendConnected = false;
  model.poseAlert = false;
  model.cubeVisible = cubeVisible;
  model.cubeRollDeg = 0.0f;
  model.cubePitchDeg = 0.0f;
  model.cubeYawDeg = 0.0f;
  model.cubeOffsetX = 0.0f;
  model.cubeOffsetY = 0.0f;
  model.cubeScale = 32.0f;
  return model;
}

void renderMode(ScreenRenderer &renderer, const std::string &mode) {
  if (mode == "boot") {
    BootScreenModel model;
    model.title = "Peek";
    model.message = "imu missing";
    renderer.renderBoot(model);
    return;
  }

  if (mode == "status") {
    StatusScreenModel model;
    model.buttonPressed = false;
    model.wifiRssi = 0;
    model.backendConnected = false;
    model.imuReady = true;
    model.imuAddress = 0;
    model.imuAccelZ = 0;
    model.imuRollDeg = 0.0f;
    model.imuPitchDeg = 0.0f;
    renderer.renderStatus(model);
    return;
  }

  HomeScreenModel model = homeModel(true);
  renderer.renderHome(model);
  if (mode == "homeFrame") {
    renderer.renderHomeFrame(model);
  } else if (mode == "menu") {
    RadialMenuModel radial;
    radial.selectedItem = RadialMenuItem::Info;
    radial.cursorX = 32.0f;
    radial.cursorY = -32.0f;
    radial.imuReady = true;
    renderer.renderRadialMenu(radial);
  }
}

std::filesystem::path outputPath(const Args &args, const std::vector<std::string> &modes, const std::string &mode) {
  if (!args.out.empty() && modes.size() == 1) {
    return args.out;
  }
  const std::filesystem::path outDir = args.out.empty() ? std::filesystem::path(kDefaultOutDir) : std::filesystem::path(args.out);
  return outDir / (mode + ".png");
}
} // namespace

int main(int argc, char **argv) {
  try {
    const Args args = parseArgs(argc, argv);
    const std::vector<std::string> modes = selectedModes(args.mode);
    std::vector<std::filesystem::path> outputs;

    for (const std::string &mode : modes) {
      DisplayDriver display;
      ScreenRenderer renderer(display);
      renderMode(renderer, mode);

      const std::filesystem::path path = outputPath(args, modes, mode);
      std::filesystem::create_directories(path.parent_path().empty() ? "." : path.parent_path());
      if (!writeDisplayPng(display, path.string().c_str())) {
        throw std::runtime_error("Failed to write " + path.string());
      }
      outputs.push_back(path);
    }

    std::cout << "Generated " << outputs.size() << " firmware screen preview" << (outputs.size() == 1 ? "" : "s") << ":\n";
    for (const auto &path : outputs) {
      std::cout << "- " << std::filesystem::absolute(path).string() << "\n";
    }
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << "\n";
    return 1;
  }
}
