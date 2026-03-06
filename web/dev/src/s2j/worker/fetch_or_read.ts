export const fetchOrReadWasm = async (url: any) => {
    const isNode = typeof process !== "undefined" && process.versions != null && process.versions.node != null;
    if (isNode) {
        // Node.js環境ではfsモジュールを使ってローカルファイルを読み込む
        const fs = await import("fs/promises");
        return fs.readFile(url);
    }
    return fetch(url).then((r) => r.arrayBuffer());
};