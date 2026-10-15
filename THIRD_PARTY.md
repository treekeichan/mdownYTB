# 第三方组件

- Qt 6：本机 C:/Qt/6.11.0/mingw_64，使用动态库。分发前应按实际 Qt 安装许可履行 LGPL/GPL 或商业许可要求，包括许可文本、可替换动态库及对应源码获取信息。https://www.qt.io/licensing/
- yt-dlp：官方 Windows 独立包，https://github.com/yt-dlp/yt-dlp/releases 。项目采用 Unlicense；打包依赖的许可另见官方 THIRD_PARTY_LICENSES.txt。https://github.com/yt-dlp/yt-dlp/blob/master/THIRD_PARTY_LICENSES.txt
- FFmpeg / FFprobe：来自 FFmpeg 官网列出的 Gyan Windows essentials 构建，https://www.gyan.dev/ffmpeg/builds/ 。具体配置、版本和许可证以 `ffmpeg -version` 与随构建分发的 LICENSE 文件为准；当前构建可能适用 GPL。源码与构建信息：https://www.gyan.dev/ffmpeg/builds/ 、https://ffmpeg.org/legal.html 。
- Node.js：使用本机已安装运行时，不随应用复制。https://nodejs.org/ 、https://github.com/nodejs/node/blob/main/LICENSE

当前 dist 目录为本机验证包，尚未完成对外发布所需的全部许可材料整理、签名及干净机器验证。
