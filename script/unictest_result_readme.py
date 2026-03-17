import json
import re
import sys

def generate_markdown_table(data :dict) -> str:
    results = data['results']
    # ユニークなrunnerとinput_nameを抽出（MECEな分類）
    runners = sorted(list(set(r['runner'] for r in results)))
    inputs = sorted(list(set(r['input_name'] for r in results)))

    # ヘッダー作成
    header = "| INPUT_NAME | " + " | ".join(runners) + " |"
    separator = "| :--- | " + " | ".join([":---:"] * len(runners)) + " |"
    
    # 行データ作成
    rows = []
    for input_name in inputs:
        row = f"| {input_name} |"
        for runner in runners:
            # 該当するテスト結果を抽出
            match = next((r for r in results if r['input_name'] == input_name and r['runner'] == runner), None)
            if match:
                icon = "✅" if match['success'] else f"❌ {match.get('output_status', '')}"
                row += f" {icon} |"
            else:
                row += " - |"
        rows.append(row)

    return "\n".join([header, separator] + rows)

result_path = sys.argv[1] if len(sys.argv) > 1 else 'test_results.json'

# READMEの書き換え
with open('README.base.md', 'r', encoding='utf-8') as f:
    readme = f.read()

with open(result_path, 'r', encoding='utf-8') as f:
    data = json.load(f)

table_content = generate_markdown_table(data)
START_MARKER = "<!-- START_TEST_RESULTS -->"
END_MARKER = "<!-- END_TEST_RESULTS -->"
new_readme = re.sub(
    f"{START_MARKER}.*?{END_MARKER}",
    f"{START_MARKER}\n\n{table_content}\n\n{END_MARKER}",
    readme,
    flags=re.DOTALL
)

print(new_readme)
