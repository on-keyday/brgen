const process = require("process");
const path = require("path");

require("./wasmCopy");

let mode = "development";
if (process.env.WEB_PRODUCTION === "production") {
  mode = "production";
}

module.exports = {
  mode: mode,
  entry: path.resolve(__dirname, "out/index.js"),
  resolve: {
    extensions: [".ts", ".tsx", ".js"],
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
  resolve: {
    fallback: {
      // これらNode.jsの組み込みモジュールは、バンドルに含めず、エラーも出さないようにする
      module: false,
    },
  },
};

if (mode === "development") {
  module.exports.devtool = "hidden-source-map";
}
