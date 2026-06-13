# Peek 仿真器架构设计

实施分支：`simulator`

## 目标

Peek 需要一个非侵入式仿真版本，用于在没有真实 ESP32-S3 硬件时验证屏幕绘制、交互状态机、资源同步和 Web 控制流程。

仿真器的核心目标：

- 仿真器读取并复用固件代码，固件不反向依赖仿真器。
- 固件屏幕 UI 只保留一份权威实现，优先复用 `ScreenRenderer.cpp`。
- 正式 Web 应用和仿真 Web 应用分离，避免开发工具污染设备控制台。
- 先支持点击一次生成一帧 PNG，后续再升级到常驻进程或 WASM。
- 目录结构清晰区分固件、正式应用、仿真工具和共享协议。

## 非目标

第一阶段不追求完整硬件级仿真：

- 不模拟 ESP32 Wi-Fi 栈、电源管理、真实 Flash 时序。
- 不模拟完整传感器噪声，只提供可控的 IMU 和触摸输入。
- 不做 30fps 实时渲染，先以交互事件驱动的 PNG 预览为主。
- 不在浏览器里重写固件圆屏 UI。

## 依赖方向

目标依赖关系如下：

```text
firmware
  ^
  |
simulator/native
  ^
  |
simulator/web
```

规则：

- `firmware/` 是真实设备固件和屏幕绘制的权威来源。
- `simulator/native/` 可以 include 和编译 `firmware/` 中的纯逻辑、UI 绘制代码。
- `simulator/web/` 只和仿真服务通信，不直接复制固件绘制逻辑。
- `apps/web/` 是正式设备控制台，不依赖 `simulator/`。
- `firmware/` 不允许 include `simulator/` 下的任何文件。

## 目标文件结构

最终建议的仓库结构：

```text
peek/
  AGENTS.md
  README.md

  firmware/
    platformio.ini
    partitions.csv
    include/
      app/
        AppController.h
        PetState.h
      assets/
        fonts/
      config/
        DeviceConfig.h
        DeviceStatus.h
      drivers/
        DisplayDriver.h
        ImuDriver.h
        MotorDriver.h
        TouchSensor.h
      events/
        TouchEvent.h
      physics/
        CubePhysics.h
      services/
        BackendClient.h
        ConfigStore.h
        FileSystemService.h
        NetworkService.h
        ProvisioningService.h
      storage/
        AssetStore.h
        FileSystem.h
      ui/
        RadialMenuController.h
        ScreenRenderer.h
        ScreenTypes.h
      util/
        Math.h
      Pins.h
    src/
      app/
        AppController.cpp
        PetState.cpp
      drivers/
        DisplayDriver.cpp
        ImuDriver.cpp
        MotorDriver.cpp
        TouchSensor.cpp
      physics/
        CubePhysics.cpp
      services/
        BackendClient.cpp
        ConfigStore.cpp
        FileSystemService.cpp
        NetworkService.cpp
        ProvisioningService.cpp
      storage/
        AssetStore.cpp
        FileSystem.cpp
      ui/
        ScreenRenderer.cpp
        menu/
          RadialMenuController.cpp
      main.cpp

  apps/
    web/
      README.md
      package.json
      bun.lock
      tsconfig.json
      vite.config.ts
      index.html
      server/
        config.ts
        gifConverter.ts
        index.ts
        network.ts
        routes.ts
        state.ts
        static.ts
        tls.ts
        types.ts
        websocket.ts
      scripts/
        esp-fs.ts
      src/
        icons.ts
        main.ts
        manage.ts
        shared.ts
        styles.css
        util.ts
        vite-env.d.ts

  simulator/
    README.md
    protocol/
      render-request.ts
      render-response.ts
      simulator-state.ts
    native/
      README.md
      package.json
      bun.lock
      include/
        Arduino.h
        HostPreview.h
        LittleFS.h
      scripts/
        render.ts
      src/
        HostDisplayDriver.cpp
        main.cpp
      fixtures/
        home.json
        status.json
        menu.json
      build/
        .gitkeep
    web/
      README.md
      package.json
      bun.lock
      tsconfig.json
      vite.config.ts
      index.html
      server/
        index.ts
        render.ts
      src/
        main.ts
        simulator.ts
        styles.css
        transport.ts

  docs/
    plan.md
    simulator-architecture.md
    hardware/
      gc9a01-datasheet.pdf

  scripts/
    upload-firmware.sh
```

当前仓库还没有迁移到该结构。第一阶段可以先新增 `simulator/`，继续让固件留在根目录，等仿真器稳定后再搬 `platformio.ini`、`src/`、`include/` 和 `partitions.csv` 到 `firmware/`。

## 固件内部边界

固件目录迁移后应逐步形成三层：

```text
firmware/src/app
  业务状态和设备主状态机。

firmware/src/ui
  屏幕模型和绘制代码。

firmware/src/drivers, services, storage
  ESP32 硬件和系统服务适配。
```

仿真器优先复用这些文件：

- `firmware/include/ui/ScreenTypes.h`
- `firmware/include/ui/ScreenRenderer.h`
- `firmware/src/ui/ScreenRenderer.cpp`
- `firmware/include/ui/RadialMenuController.h`
- `firmware/src/ui/menu/RadialMenuController.cpp`
- `firmware/include/app/PetState.h`
- `firmware/src/app/PetState.cpp`

仿真器暂时不复用这些硬件依赖较重的文件：

- `firmware/src/app/AppController.cpp`
- `firmware/src/drivers/*`
- `firmware/src/services/NetworkService.cpp`
- `firmware/src/services/ProvisioningService.cpp`
- `firmware/src/services/BackendClient.cpp`

等后续状态机需要更高一致性时，再把 `AppController` 中的纯逻辑抽到新的 core 层。

## 仿真器结构

### `simulator/protocol`

放置仿真 Web 和 native runner 共享的数据结构。

建议包含：

- `simulator-state.ts`：仿真状态，例如模式、触摸、IMU、网络、电量、宠物状态。
- `render-request.ts`：渲染请求结构，例如 screen 类型、状态快照、输出格式。
- `render-response.ts`：渲染响应结构，例如 PNG 路径、宽高、事件日志。

协议层只描述数据，不依赖浏览器、Bun 服务或 C++ runner。

### `simulator/native`

native runner 负责在主机上编译固件屏幕绘制代码，并把 JSON 输入渲染成 PNG。

第一阶段 runner 行为：

```bash
cd simulator/native
bun run render -- --input fixtures/home.json --out ../../.peek-preview/simulator-home.png
```

内部流程：

```text
read JSON
  -> build HomeScreenModel / StatusScreenModel / RadialMenuModel
  -> call firmware ScreenRenderer.cpp
  -> write PNG
```

host adapter 放在 `simulator/native/include` 和 `simulator/native/src`，用于替代 Arduino、LittleFS、DisplayDriver 等主机不可用依赖。

### `simulator/web`

仿真 Web 是独立开发工具，不放进正式 `apps/web`。

页面职责：

- 提供触摸、长按、摇晃、倾斜、低电量、离线、切换宠物等控制。
- 维护仿真状态。
- 请求 `simulator/native` 渲染 PNG。
- 显示最新 PNG 和事件日志。

页面不负责重新实现圆屏 UI。

## 第一阶段落地结构

为了避免一次性大搬家，第一阶段只做新增和轻量搬迁：

```text
peek/
  simulator/
    README.md
    protocol/
      simulator-state.ts
      render-request.ts
      render-response.ts
    native/
      package.json
      bun.lock
      include/
        Arduino.h
        HostPreview.h
        LittleFS.h
      scripts/
        render.ts
      src/
        HostDisplayDriver.cpp
        main.cpp
      fixtures/
        home.json
        status.json
        menu.json
    web/
      package.json
      bun.lock
      tsconfig.json
      vite.config.ts
      index.html
      server/
        index.ts
        render.ts
      src/
        main.ts
        simulator.ts
        styles.css
```

现有 `tools/screen-preview` 可以先作为实现来源迁移到 `simulator/native`。迁移完成前，`apps/web/scripts/render-screen-preview.ts` 仍可继续存在，避免影响当前固件 UI 预览命令。

## 第二阶段目标结构

第二阶段再移动固件目录：

```text
platformio.ini      -> firmware/platformio.ini
partitions.csv      -> firmware/partitions.csv
include/            -> firmware/include/
src/                -> firmware/src/
```

同时更新：

- `README.md`
- `scripts/upload-firmware.sh`
- `apps/web/scripts/render-screen-preview.ts`
- `simulator/native` 的 include path
- PlatformIO 构建命令

迁移后固件构建命令变为：

```bash
cd firmware
uvx --with pip --from platformio platformio run
```

## 开发命令

目标命令：

```bash
# 固件
cd firmware
uvx --with pip --from platformio platformio run

# 正式 Web 控制台
cd apps/web
bun install
bun run build
bun run start

# 仿真器 Web
cd simulator/web
bun install
bun run dev

# 仿真器 native 渲染
cd simulator/native
bun run render -- --input fixtures/home.json --out ../../.peek-preview/simulator-home.png
```

在本机没有全局 Bun 时，可以继续使用：

```bash
npx bun run build
npx bun run dev
```

## 仿真状态模型草案

第一版仿真状态建议：

```ts
export type SimulatorScreenMode =
  | "home"
  | "status"
  | "radialMenu"
  | "sleeping";

export interface SimulatorState {
  screenMode: SimulatorScreenMode;
  petIndex: number;
  touchPressed: boolean;
  wifiConnected: boolean;
  backendConnected: boolean;
  lowBattery: boolean;
  imuReady: boolean;
  rollDeg: number;
  pitchDeg: number;
  yawDeg: number;
  cubeOffsetX: number;
  cubeOffsetY: number;
  cubeScale: number;
  hintText: string;
  eventLog: string[];
}
```

native runner 不需要知道按钮 UI 细节，只接收最终状态快照并渲染。

## 代码共享策略

短期共享：

- 屏幕模型类型：`ScreenTypes.h`
- 屏幕绘制：`ScreenRenderer.cpp`
- 径向菜单计算：`RadialMenuController`
- 宠物状态：`PetState`

中期共享：

- 把 `AppController` 中和硬件无关的状态转换逻辑抽为 `app/core`。
- 固件和仿真器都调用同一套 core 输入处理。
- ESP32 的硬件驱动继续留在 `drivers` 和 `services`。

长期可选：

- 把 native runner 改成常驻服务，避免每次渲染重新启动。
- 把 `ScreenRenderer.cpp` 编译到 WASM，用于浏览器内实时渲染。
- 给仿真状态增加录制和回放，用于复现交互 bug。

## 迁移验收标准

第一阶段完成标准：

- `simulator/native` 可以从 JSON 生成至少 `home`、`status`、`menu` 三类 PNG。
- 生成 PNG 使用真实 `ScreenRenderer.cpp`。
- `simulator/web` 可以操作状态并显示 PNG。
- `apps/web` 不依赖 `simulator/`。
- 固件原构建命令仍然通过。

第二阶段完成标准：

- 固件源码已移动到 `firmware/`。
- `cd firmware && uvx --with pip --from platformio platformio run` 通过。
- `cd apps/web && bun run build` 通过。
- `simulator/native` 仍能从 `firmware/` 读取绘制代码并生成 PNG。
- `README.md` 和脚本中的路径全部更新。

## 约束

- 每次代码或文档改动都要 commit。
- Python 相关工具继续使用 uv。
- 前端继续使用 TypeScript 和 Bun。
- 修改屏幕 UI 或固件屏幕绘制后，必须运行：

```bash
cd apps/web
bun run preview:png -- --mode all
```

并在最终回复里提供 `.peek-preview/*.png` 路径给用户检查。
