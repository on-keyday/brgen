#!/bin/bash

# リポジトリ情報を設定（OWNER/REPO 形式）
REPO="on-keyday/rebrgen"

# 保存先ディレクトリを指定
OUTPUT_DIR="./rebrgen"
# ダウンロード用のディレクトリを作成
rm -rf "${OUTPUT_DIR:?}"
mkdir -p "$OUTPUT_DIR"

if [ "$1" == "" ];then
    # 最新のワークフロー実行を取得
    echo "Fetching the latest workflow run for $REPO..."

    INDEX=0
    MAX_RETRIES=5
    while [ $INDEX -lt $MAX_RETRIES ]; do
        # gh CLIを使用して最新のワークフロー実行を取得
        RUN_ID=$(gh run list --repo "$REPO" --branch main --limit "$((INDEX + 1))" --json databaseId -q ".[$INDEX].databaseId")

        if [ $? -ne 0 ]; then
            # 失敗した場合
            echo "Failed to fetch workflow run. Retrying... ($((INDEX + 1))/$MAX_RETRIES)"
            sleep 2
            # インデックスを増加させて次の実行を取得
            INDEX=$((INDEX + 1))
            continue
        fi

        # RUN_ID が取得できない場合のエラー処理
        if [ -z "$RUN_ID" ]; then
            echo "No workflow run found. Retrying... ($((INDEX + 1))/$MAX_RETRIES)"
            sleep 2
            # インデックスを増加させて次の実行を取得
            INDEX=$((INDEX + 1))
            continue
        fi

        # bmgen-webアーティファクトをダウンロードを試みる
        echo "Downloading bmgen-web artifact from run ID: $RUN_ID..."

        if gh run download "$RUN_ID" --repo "$REPO" --name "bmgen-web" --dir "$OUTPUT_DIR"; then
            # ダウンロード成功
            echo "Download successful."
            break
        else
            # ダウンロード失敗
            echo "Failed to download bmgen-web artifact. Retrying... ($((INDEX + 1))/$MAX_RETRIES)"
            sleep 2
            # インデックスを増加させて次の実行を取得
            INDEX=$((INDEX + 1))
        fi
    done
    # すべての試行が失敗した場合
    if [ $INDEX -eq $MAX_RETRIES ]; then
        echo "Failed to download bmgen-web artifact after $MAX_RETRIES attempts."
        exit 1
    fi
    echo "Move contents of ./rebrgen/tool to ./web/dev/src/lib/bmgen directory"
    mkdir -p ./web/dev/src/lib/bmgen
    mv ./rebrgen/tool/* ./web/dev/src/lib/bmgen
else
    cp -vru "$1/." ./web/dev/src/lib/bmgen || exit 1
fi


echo "Appending wasm ./web/dev/src/lib/bmgen/wasmCopy.js.txt to ./web/dev/wasmCopy.js"
cp ./web/dev/wasmCopy.js.txt ./web/dev/wasmCopy.js
cat ./web/dev/src/lib/bmgen/wasmCopy.js.txt ./web/dev/src/lib/bmgen/ebmWasmCopy.js.txt >> ./web/dev/wasmCopy.js
