const process = require("process");
const path = require("path");
const webpack = require("webpack");

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
    alias: {
      "node:module": false,
      "node:fs": false,
      "node:path": false,
    },
    fallback: {
      // これらNode.jsの組み込みモジュールは、バンドルに含めず、エラーも出さないようにする
      module: false,
      fs: false,
      path: false,
    },
  },
  plugins: [
    new webpack.DefinePlugin({
      "process.env.NODE_ENV": JSON.stringify(mode),
    }),
  ],
};

if (mode === "development") {
  module.exports.devtool = "hidden-source-map";
}
