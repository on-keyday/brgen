
import "./worker_pollyfill.js"
import { serve } from '@hono/node-server'
import {createApp} from './core.js'
import { argv } from "process"
import { readFile } from "fs/promises"

const browser_version_path = argv[2] || "./browser_version"

const detectContentType = (path :string) :string | undefined => {
    if(path.endsWith(".html")) return "text/html; charset=utf-8";
    if(path.endsWith(".js")) return "application/javascript; charset=utf-8";
    if(path.endsWith(".css")) return "text/css; charset=utf-8";
    if(path.endsWith(".wasm")) return "application/wasm";
    if(path.endsWith(".json")) return "application/json; charset=utf-8";
    if(path.endsWith(".svg")) return "image/svg+xml; charset=utf-8";
    if(path.endsWith(".png")) return "image/png";
    if(path.endsWith(".jpg") || path.endsWith(".jpeg")) return "image/jpeg";
    if(path.endsWith(".gif")) return "image/gif";
    return undefined;
}

const app = createApp({
    getResource: async (path :string) => {
        console.log(`fetching resource: ${path} (from ${browser_version_path})`);
        return readFile(`${browser_version_path}/${path}`).then((data) => {
            return {data: new Uint8Array(data), contentType: detectContentType(path)};
        }).catch(() => {
            return null;
        });
    }
})

console.log("Server started at 8081")
serve({
    "fetch": app.fetch,
    "port": 8081
})
