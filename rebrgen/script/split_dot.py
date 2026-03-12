import pydot
import os


def split_dot_file_by_subgraph(dot_file_path: str, output_prefix: str):
    """
    指定されたDOTファイルをサブグラフごとに分割し、個別のDOTファイルとSVGファイルを出力する。
    """
    print(f"{dot_file_path}を読込中...")
    try:
        graphs = pydot.graph_from_dot_file(dot_file_path)
        if not graphs:
            print("エラー: DOTファイルが読み込めませんでした。")
            return
    except Exception as e:
        print(f"エラー: DOTファイルの解析中に問題が発生しました - {e}")
        return

    main_graph = graphs[0]

    # メイングラフ直下のサブグラフを抽出
    subgraphs = [
        sub
        for sub in main_graph.get_subgraphs()
        if sub.get_parent_graph() == main_graph
    ]

    if not subgraphs:
        print("情報: サブグラフが見つかりませんでした。")
        return

    for subgraph in subgraphs:
        subgraph_name = subgraph.get_name().strip('"')

        # 新しいグラフオブジェクトを作成
        new_graph = pydot.Dot(graph_type="digraph", prog="dot")

        # 元のサブグラフを新しいグラフに追加
        new_graph.add_subgraph(subgraph)

        # ファイル名として無効な文字を'_'に置換
        safe_subgraph_name = "".join(
            [c if c.isalnum() or c in [".", "_"] else "_" for c in subgraph_name]
        )

        # 個別のDOTファイルとして保存

        dot_output_path = os.path.join(output_prefix, f"{safe_subgraph_name}.dot")
        new_graph.write(dot_output_path, format="raw")

        # 個別のSVGファイルとして保存
        svg_output_path = os.path.join(output_prefix, f"{safe_subgraph_name}.svg")
        new_graph.write(svg_output_path, format="svg")

        print(f"サブグラフ '{subgraph_name}' を {svg_output_path} に保存しました。")


if __name__ == "__main__":
    import sys

    try:
        dot_file = sys.argv[1]
        output_dir = sys.argv[2]
    except Exception as e:
        print("Usage: python split_dot.py [dot_file] [output_dir]")
        exit(1)
    os.makedirs(output_dir, exist_ok=True)
    split_dot_file_by_subgraph(dot_file, output_dir)
