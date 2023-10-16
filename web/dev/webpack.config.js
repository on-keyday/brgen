const { copyFile } = require("fs");
const path = require("path");

copyFile(path.resolve(__dirname, "src/lib/src2json.wasm"), path.resolve(__dirname, "out/lib/src2json.wasm"), (err) => {
    if (err) {
        console.log(err);
    }
});

module.exports = {
    mode: "development",
    entry: path.resolve(__dirname, "out/index.js"),
    resolve: {
        extensions: [".js", ".ts"],
    },
    module: {
        rules: [
            {
                test: /\.ts$/,
                loader: "ts-loader",
            },
            {
                test: /\.css$/,
                use: ["style-loader", "css-loader"],
            },
        ],
    },
    output: {
        filename: "index.js",
        path: path.resolve(__dirname, "../script"),
    }, 
}
