# mdownYTB

使用本机 Qt 6 / C++17 开发的 Windows YouTube 单视频下载工具。

## 已实现

- 中文 Qt Widgets 界面；解析视频标题、频道、时长和可用清晰度。
- 视频 + 音频下载、MP4 / MKV 封装；仅音频输出 M4A；可选人工字幕。
- 顺序队列、实时进度/速度/预计剩余时间、停止、重试和打开目录。
- 进程异步运行；Windows Job Object 在可用时负责下载引擎与子进程清理。
- 本机配置、代理、工具路径与最近 500 条任务记录；程序重启后任务需手动重试。
- 日志隐藏网络 URL；不收集、不上传下载记录。

## 编译与运行

本机已验证工具链：Qt 6.11.0 mingw_64、MinGW 13.1、CMake 3.30.5。当前版本 v0.2.2。

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Package
.\dist\mdownYTB-v0.2.2\mdownYTB.exe
```

也可使用 Qt Creator 打开 CMakeLists.txt，选择对应 Qt 6 MinGW Kit。不要混用 Qt 5 MSVC 和 Qt 6 MinGW 库。

## 下载依赖

将 yt-dlp.exe、ffmpeg.exe、ffprobe.exe 放入项目 tools 目录；也可在界面“设置与依赖”中手动指定路径。Node.js 22+ 从 PATH 自动识别。官方 yt-dlp Windows 独立包包含 EJS；其他安装方式请按官方文档配置：https://github.com/yt-dlp/yt-dlp/wiki/EJS 。

源码工程可运行 `powershell -ExecutionPolicy Bypass -File .\scripts\fetch-tools.ps1` 下载并按发布方清单校验工具；已有文件默认保留，`-Force` 用于手动更新。下载和校验都成功后才替换 yt-dlp 引擎。

### Windows 便携版

从 GitHub Releases 下载 `mdownYTB-v0.2.2-windows-x64.zip`，解压后运行 `mdownYTB.exe`。发布压缩包不内置下载引擎；在解压目录打开 PowerShell，执行 `powershell -ExecutionPolicy Bypass -File .\fetch-tools.ps1 -TargetDirectory .\tools`，即可把 yt-dlp、FFmpeg 和 FFprobe 下载到此目录并校验。Node.js 22+ 需单独安装。第三方源码与许可说明随包放在 `licenses` 目录。

官方来源及许可见 THIRD_PARTY.md。工具更新后建议重新运行测试并验证真实链接；软件不会自动下载或执行更新。

网络模式包括系统默认（继承引擎/环境代理）、直连（显式禁用代理）及指定代理。代理可以填写 `http://127.0.0.1:7890` 或 `socks5://127.0.0.1:1080`；当前版本不保存需要账号密码的代理。旧版本已填写的代理自动沿用为指定代理。

## 平台验证与 Cookies（v0.2）

出现“Sign in to confirm you're not a bot”时，在“设置与依赖”中选择登录会话方式：

- **Cookies 文件（推荐）**：选择自己导出的 Mozilla/Netscape 格式 cookies.txt。保存及执行前校验文件存在、可读、首行格式和记录结构；失效后需要重新导出。
- **读取我选择的浏览器**：主动选择 Chrome / Edge / Firefox，并可指定配置文件名或目录。程序只在解析或下载时让 yt-dlp 读取该会话。若数据库被占用或无法解密，请使用文件方式。
- **不使用登录会话**：默认行为，不主动读取浏览器数据。

解析和下载使用一致的会话与网络参数。配置仅保存路径、浏览器选择及网络选项，不将 Cookies 内容保存到任务历史、日志或运行包。Cookies 文件可能被 yt-dlp 更新，请不要上传或分享该文件。使用账号进行下载存在平台账号限制风险，按需使用即可。

可点击“打开视频完成验证”，在浏览器完成验证后，刷新 Cookies 并重新解析；浏览器与软件应使用相同网络出口。登录会话不能保证所有视频均可下载。设置中的“检测工具版本”会异步检查 yt-dlp、FFmpeg、FFprobe 和 Node.js；EJS 的实际可用性仍需真实解析验证。

官方指导：https://github.com/yt-dlp/yt-dlp/wiki/Extractors#exporting-youtube-cookies 。

## 使用

1. 粘贴完整 YouTube / youtu.be / Shorts 视频链接，点击“解析链接”。
2. 选择内容、清晰度上限、封装格式、字幕及保存目录。
3. 加入队列；等待任务会自动顺序执行。双击失败行查看原因，双击完成行打开文件。
4. 停止任务保留临时文件；“重试选中”会在可行时续传。合并阶段通常没有可用百分比，显示处理中。
5. “清空历史记录”移除已完成、失败和已停止的记录，并同步保存；正在运行和等待的任务保留，不删除下载文件或临时文件。

清晰度是上限，平台可能未提供指定分辨率对应的完整音视频组合。MP4 优先 MP4 视频 + M4A 音频；不可用时可尝试 MKV。文件名包含视频 ID、封装和清晰度选项，避免不同下载版本互相覆盖；已有同名文件不会覆盖。

当前未实现播放列表、自动字幕、自动更新、并发下载、封面预览和安装器。网络可达性及平台验证可能导致真实下载失败；不保证所有视频可下载。请仅下载自己拥有或获授权的内容。

## 验证

构建脚本自动运行 Qt Test：域名检查、格式筛选、参数隔离、真实子进程的 JSON/UTF-8分片读取、错误处理和停止。`mdownYTB.exe --smoke-test` 会保存 smoke-window.png 并退出，供界面检查。

任务记录：Qt AppLocalDataLocation 下 tasks.json；配置：当前用户 QSettings INI 文件。日志仅保留在窗口内。
