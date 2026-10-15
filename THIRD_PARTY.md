# Third-party components

- **Qt 6.11.0**: dynamically linked from the locally installed Qt MinGW build. Windows release packages include the Qt DLLs and Qt license text. Qt is available under LGPL/GPL or commercial terms; see the exact obligations and source/relinking information in `licenses/RELEASE_SOURCES.txt`. https://www.qt.io/development/open-source-lgpl-obligations
- **MinGW runtime**: release packages include GCC runtime DLLs and winpthreads. Corresponding license texts and upstream source locations are included under `licenses/`.
- **yt-dlp, FFmpeg, FFprobe**: not included in the public release archive. The optional `fetch-tools.ps1` downloads them from their upstream publishers into a user-selected directory and checks upstream checksums. yt-dlp is distributed under Unlicense; FFmpeg builds may use GPL components. Review each publisher's current license and notices before redistributing downloaded binaries. https://github.com/yt-dlp/yt-dlp/releases · https://www.gyan.dev/ffmpeg/builds/ · https://ffmpeg.org/legal.html
- **Node.js**: not included; install Node.js 22+ separately if needed. https://nodejs.org/

The release ZIP contains only the application, dynamically linked Qt/runtime files, documentation, license texts, and the dependency-fetch script. It excludes tools, browser data, Cookies, user configuration, logs, and downloaded media. See `licenses/RELEASE_SOURCES.txt` for exact source links and Qt library replacement information.

The `dist/` directory created by the ordinary developer packaging mode is a local test package and may contain third-party executables from `tools/`; do not treat it as the public release package.
