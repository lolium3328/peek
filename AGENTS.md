# AGENTS.md

- 每次改动都要 commit。
- Python 项目使用 uv。
- 前端项目使用 TypeScript 和 Bun。
- 修改屏幕 UI 或固件屏幕绘制后，运行 `cd apps/web && bun run preview:png -- --mode all` 生成 `.peek-preview/*.png`，检查示意图；最终回复中提供生成路径给用户检查。
