## 2. 開発環境のセットアップ

### 2.1 ビルド手順
1.  `build_config.template.json`を`build_config.json`にコピーします。
2.  `brgen`ツールをビルドしたい場合は、`build_config.json`の`AUTO_SETUP_BRGEN`を`true`に設定します。
3.  最初に`python script/auto_setup.py`を実行します。その後のビルドでは、`python script/build.py native Debug`を実行します。このコマンドは、ネイティブプラットフォーム向けに`Debug`モードでプロジェクトをビルドします。実行可能ファイルは、`tool/ebmgen.exe`、`tool/ebmcodegen.exe`など（Windowsの場合）または他のOSの対応するパスに配置されます。
