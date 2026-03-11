## 2. Setting Up the Development Environment

### 2.1 Build Instructions
1.  Copy `build_config.template.json` to `build_config.json`.
2.  If you want to build `brgen` tools, set `AUTO_SETUP_BRGEN` to `true` in `build_config.json`.
3.  Initially, run `python script/auto_setup.py`. For subsequent builds, run `python script/build.py native Debug`. This command will build the project in `Debug` mode for your native platform. Executables will be located at `tool/ebmgen.exe`, `tool/ebmcodegen.exe`, etc. (on Windows) or corresponding paths on other OSes.
