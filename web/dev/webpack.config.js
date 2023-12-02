const { copyFile } = require("fs");
const path = require("path");


const copyWasm = (p) => {
    copyFile(path.resolve(__dirname, "src/lib/",p), path.resolve(__dirname, "out/lib/",p), (err) => {
        if (err) {
            console.log(err);
        }
    });
}

copyWasm("src2json.wasm");
copyWasm("json2cpp.wasm");
copyWasm("json2cpp2.wasm");
copyWasm("json2go.wasm");



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
            }
        ],
    },
    output: {
        filename: "index.js",
        path: path.resolve(__dirname, "../public/script"),
        assetModuleFilename: "[name][ext]",
    }, 
}
