#!/bin/bash

# リポジトリ情報を設定（OWNER/REPO 形式）
REPO="on-keyday/rebrgen"

# 保存先ディレクトリを指定
OUTPUT_DIR="./rebrgen"

# 最新のワークフロー実行を取得
echo "Fetching the latest workflow run for $REPO..."
RUN_ID=$(gh run list --repo "$REPO" --limit 1 --json databaseId -q '.[0].databaseId')

# RUN_ID が取得できない場合のエラー処理
if [ -z "$RUN_ID" ]; then
    echo "Error: No workflow runs found for $REPO."
    exit 1
fi

echo "Latest workflow run ID: $RUN_ID"

# ダウンロード用のディレクトリを作成
rm -rf "${OUTPUT_DIR:?}"
mkdir -p "$OUTPUT_DIR"


# bmgen-webアーティファクトをダウンロード
echo "Downloading bmgen-web artifact..."

gh run download "$RUN_ID" --repo "$REPO" --name "bmgen-web" --dir "$OUTPUT_DIR"
echo "bmgen-web artifact downloaded to $OUTPUT_DIR."

echo "Move contents of ./rebrgen/tool to ./web/dev/src/lib/bmgen directory"
mkdir -p ./web/dev/src/lib
mv ./rebrgen/tool/* ./web/dev/src/lib/bmgen
