const { copyFile } = require("fs");
const process = require("process")
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
copyWasm("json2c.wasm");
copyWasm("json2ts.wasm");

let mode = "development";
if (process.env.WEB_PRODUCTION === "production") {
    mode = "production";
}

module.exports = {
    mode: mode,
    entry: path.resolve(__dirname, "out/index.js"),
    resolve: {
        extensions: [".ts",".tsx",".js"],
    },
    module: {
        rules: [
            {
                test: /\.(ts|tsx)$/,
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
        path: path.resolve(__dirname, "../public/script"),
        assetModuleFilename: "[name][ext]",
    }, 
    stats: {
        errorDetails: true,
    },
}

if (mode === "development") {
    module.exports.devtool = "hidden-source-map"
}
