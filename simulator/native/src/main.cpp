#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>

#include "HostPreview.h"
#include "drivers/DisplayDriver.h"
#include "ui/ScreenRenderer.h"

namespace {
struct Args {
  std::string input;
  std::string out;
};

struct HomeText {
  std::string primary;
  std::string hint;
  std::string localWeather;
  std::string peerWeather;
  std::string localLabel;
  std::string peerLabel;
  std::string animationPath;
};

void printHelp() {
  std::cout << "Usage: peek-simulator-render --input <state.json> --out <frame.png>\n";
}

Args parseArgs(int argc, char **argv) {
  Args args;
  for (int index = 1; index < argc; ++index) {
    const std::string arg = argv[index];
    if (arg == "--help" || arg == "-h") {
      printHelp();
      std::exit(0);
    }
    if (arg == "--input" || arg == "-i") {
      if (index + 1 >= argc) throw std::runtime_error("Missing value for " + arg);
      args.input = argv[++index];
      continue;
    }
    if (arg.rfind("--input=", 0) == 0) {
      args.input = arg.substr(8);
      continue;
    }
    if (arg == "--out" || arg == "-o") {
      if (index + 1 >= argc) throw std::runtime_error("Missing value for " + arg);
      args.out = argv[++index];
      continue;
    }
    if (arg.rfind("--out=", 0) == 0) {
      args.out = arg.substr(6);
      continue;
    }
    throw std::runtime_error("Unknown argument: " + arg);
  }
  if (args.input.empty()) throw std::runtime_error("Missing --input");
  if (args.out.empty()) throw std::runtime_error("Missing --out");
  return args;
}

std::string readTextFile(const std::string &path) {
  std::ifstream file(path);
  if (!file) throw std::runtime_error("Failed to open " + path);
  return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::string unescapeJsonString(const std::string &value) {
  std::string result;
  result.reserve(value.size());
  for (size_t index = 0; index < value.size(); ++index) {
    const char c = value[index];
    if (c != '\\' || index + 1 >= value.size()) {
      result.push_back(c);
      continue;
    }
    const char next = value[++index];
    if (next == 'n') result.push_back('\n');
    else if (next == 't') result.push_back('\t');
    else result.push_back(next);
  }
  return result;
}

std::string jsonString(const std::string &json, const std::string &key, const std::string &fallback) {
  const std::regex pattern("\"" + key + "\"\\s*:\\s*\"((?:[^\"\\\\]|\\\\.)*)\"");
  std::smatch match;
  if (!std::regex_search(json, match, pattern)) return fallback;
  return unescapeJsonString(match[1].str());
}

bool jsonBool(const std::string &json, const std::string &key, bool fallback) {
  const std::regex pattern("\"" + key + "\"\\s*:\\s*(true|false)");
  std::smatch match;
  if (!std::regex_search(json, match, pattern)) return fallback;
  return match[1].str() == "true";
}

double jsonNumber(const std::string &json, const std::string &key, double fallback) {
  const std::regex pattern("\"" + key + "\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?)");
  std::smatch match;
  if (!std::regex_search(json, match, pattern)) return fallback;
  return std::stod(match[1].str());
}

int jsonInt(const std::string &json, const std::string &key, int fallback) {
  return static_cast<int>(jsonNumber(json, key, fallback));
}

RadialMenuItem radialItem(const std::string &value) {
  if (value == "info") return RadialMenuItem::Info;
  if (value == "previousPet") return RadialMenuItem::PreviousPet;
  if (value == "nextPet") return RadialMenuItem::NextPet;
  return RadialMenuItem::Cancel;
}

HomeScreenModel homeModel(const std::string &json, HomeText &text) {
  const std::string mode = jsonString(json, "screenMode", "home");
  const int petIndex = jsonInt(json, "petIndex", 1);

  text.primary = jsonString(json, "primaryText", mode == "sleeping" ? "zzz..." : "Pet " + std::to_string(petIndex));
  text.hint = jsonString(json, "hintText", mode == "sleeping" ? "sleeping" : "hold + shake");
  text.localWeather = jsonString(json, "localWeather", "--");
  text.peerWeather = jsonString(json, "peerWeather", "--");
  text.localLabel = jsonString(json, "localLabel", "A");
  text.peerLabel = jsonString(json, "peerLabel", "B");
  text.animationPath = jsonString(json, "petAnimationPath", "");

  HomeScreenModel model;
  model.primaryText = text.primary.c_str();
  model.hintText = text.hint.c_str();
  model.localWeather = text.localWeather.c_str();
  model.peerWeather = text.peerWeather.c_str();
  model.localLabel = text.localLabel.c_str();
  model.peerLabel = text.peerLabel.c_str();
  model.wifiConnected = jsonBool(json, "wifiConnected", false);
  model.backendConnected = jsonBool(json, "backendConnected", false);
  model.poseAlert = jsonBool(json, "lowBattery", false);
  model.cubeVisible = jsonBool(json, "cubeVisible", true);
  model.cubeRollDeg = static_cast<float>(jsonNumber(json, "rollDeg", 0.0));
  model.cubePitchDeg = static_cast<float>(jsonNumber(json, "pitchDeg", 0.0));
  model.cubeYawDeg = static_cast<float>(jsonNumber(json, "yawDeg", 0.0));
  model.cubeOffsetX = static_cast<float>(jsonNumber(json, "cubeOffsetX", 0.0));
  model.cubeOffsetY = static_cast<float>(jsonNumber(json, "cubeOffsetY", 0.0));
  model.cubeScale = static_cast<float>(jsonNumber(json, "cubeScale", 32.0));
  model.petThrowActive = jsonBool(json, "petThrowActive", false);
  model.petAnimationVisible = jsonBool(json, "petAnimationVisible", false);
  model.petAnimationPath = text.animationPath.c_str();
  return model;
}

void renderStatus(ScreenRenderer &renderer, const std::string &json) {
  StatusScreenModel model;
  model.buttonPressed = jsonBool(json, "touchPressed", false);
  model.wifiRssi = static_cast<int8_t>(jsonInt(json, "wifiRssi", -55));
  model.backendConnected = jsonBool(json, "backendConnected", false);
  model.imuReady = jsonBool(json, "imuReady", true);
  model.imuAddress = static_cast<uint8_t>(jsonInt(json, "imuAddress", 104));
  model.imuAccelZ = static_cast<int16_t>(jsonInt(json, "imuAccelZ", 16384));
  model.imuRollDeg = static_cast<float>(jsonNumber(json, "rollDeg", 0.0));
  model.imuPitchDeg = static_cast<float>(jsonNumber(json, "pitchDeg", 0.0));
  renderer.renderStatus(model);
}

void renderRadialMenu(ScreenRenderer &renderer, const std::string &json) {
  HomeText text;
  renderer.renderHome(homeModel(json, text));

  RadialMenuModel model;
  model.selectedItem = radialItem(jsonString(json, "selectedItem", "info"));
  model.cursorX = static_cast<float>(jsonNumber(json, "cursorX", 32.0));
  model.cursorY = static_cast<float>(jsonNumber(json, "cursorY", -32.0));
  model.imuReady = jsonBool(json, "imuReady", true);
  renderer.renderRadialMenu(model);
}

void renderFrame(ScreenRenderer &renderer, const std::string &json) {
  const std::string mode = jsonString(json, "screenMode", "home");
  if (mode == "status") {
    renderStatus(renderer, json);
    return;
  }
  if (mode == "radialMenu") {
    renderRadialMenu(renderer, json);
    return;
  }
  HomeText text;
  renderer.renderHome(homeModel(json, text));
}
} // namespace

int main(int argc, char **argv) {
  try {
    const Args args = parseArgs(argc, argv);
    const std::string json = readTextFile(args.input);

    DisplayDriver display;
    ScreenRenderer renderer(display);
    renderFrame(renderer, json);

    const std::filesystem::path output(args.out);
    std::filesystem::create_directories(output.parent_path().empty() ? "." : output.parent_path());
    if (!writeDisplayPng(display, output.string().c_str())) {
      throw std::runtime_error("Failed to write " + output.string());
    }

    std::cout << std::filesystem::absolute(output).string() << "\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << "\n";
    return 1;
  }
}
