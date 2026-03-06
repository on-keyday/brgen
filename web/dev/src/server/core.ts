import { Hono } from "hono";
import { WebStandardStreamableHTTPServerTransport } from "@modelcontextprotocol/sdk/server/webStandardStreamableHttp.js";
import { GeneratorService, GenerateResult, ConfigReader } from "../generator_service.js";
import { Language } from "../s2j/msg.js";
import { languageRegistry, allLanguageIds } from "../languages.js";
/*
import {
    directFixedWorkerMap,
    createDirectBmWorkerMap,
    createDirectEbmWorkerMap,
} from "./workers.js";
import { BM_LANGUAGES } from "../lib/bmgen/bm_caller.js";
import { EBM_LANGUAGES } from "../lib/bmgen/ebm_caller.js";
*/
import { createMcpServer } from "./mcp.js";
import { UpdateTracer } from "../s2j/update.js";

import WebWorker  from "web-worker";

export const patchWebWorker = (stdioHook? :(level: string, ...args: any[]) => void) => {

    if (typeof globalThis.Worker === "undefined") {
        const OriginalWorker = WebWorker;
        if(!stdioHook) {
            globalThis.Worker = OriginalWorker;
            return;
        }

        // @ts-ignore
        globalThis.Worker = class {

            worker : InstanceType<typeof OriginalWorker>;

            messageListener: ((this: any, ev: MessageEvent) => any) | null = null;
            errorListener: ((this: any, ev: ErrorEvent) => any) | null = null;
            
            constructor(scriptURL: string | URL, options?: any) {
                const originalPath = scriptURL.toString();

                // 注入する初期化コード
                // 1. console.log をカスタム関数に置き換え
                // 2. 本来のスクリプトを import する
                const bootstrapCode = `
                    (async function() {
                        const { parentPort, isMainThread } = await import('worker_threads');
        
                        // console メソッドを一括置換
                        ['log', 'warn', 'error', 'info', 'debug'].forEach(method => {
                            console[method] = (...args) => {
                                // メインスレッドへログデータを送信
                                /*
                                parentPort.postMessage({
                                    type: 'WORKER_LOG',
                                    method: method,
                                    payload: args,
                                    threadId: import('worker_threads').threadId
                                });
                                */
                            };
                        });

                        /*
                        console.log(!isMainThread ? "Worker environment detected" : "Not in a worker environment");
                        console.log("postMessage is", globalThis.postMessage.toString(),globalThis.postMessage === global.self.postMessage);

                        globalThis.postMessage = (message, transfer) => {
                            console.trace("Worker postMessage");
                            try {
                                parentPort.postMessage(message, transfer);
                            } catch (err) {
                                console.error("Failed to post message from worker:", err);
                            }
                        }
                        */

                        // 本来の Worker スクリプトを非同期で読み込み
                        await import('${originalPath}').catch(err => {
                            console.error('Worker load error:', err);
                        });
                    })();
                `;

                console.warn("Initializing patched Worker with bootstrap code",bootstrapCode);

                // コードを Base64 化して Data URI として渡す
                const blob = `data:text/javascript;base64,${Buffer.from(bootstrapCode).toString('base64')}`;
                
                const worker = new OriginalWorker(blob, options);
                this.worker = worker;
                /*

                const originalDispatchEvent = worker.dispatchEvent.bind(worker);

                worker.dispatchEvent = (event: any): boolean => {
                    console.trace(`Worker event:`, event);
                    if (event?.data?.type === 'WORKER_LOG') {
                        const { method, payload, threadId } = event.data;
                        stdioHook?.(method, `[Worker ${threadId}]`, ...payload);
                        return true; // indicate that we've handled the event
                    }
                    if(event?.type === "message" && this.messageListener) {
                        console.warn(`Handling message event`,event.data);
                        this.messageListener(event);
                        return true;
                    }
                    if(event?.type === "error" && this.errorListener) {
                        console.warn(`Handling error event`);
                        this.errorListener(event);
                        return true;
                    }
                    return originalDispatchEvent(event);
                }
                */
            }

            postMessage(message: any, transfer?: any) {
                //console.trace(`Posting message to worker:`, message);
                this.worker.postMessage(message, transfer);
            }

            addEventListener(type: string, listener: (this: any, ev: MessageEvent) => any) {
                /*
                console.trace(`add ${type} listener`)
                if (type === "message") {
                    if(this.messageListener) {
                        console.warn("Overwriting existing message listener");
                    }
                    this.messageListener = listener;
                } else if (type === "error") {
                    if(this.errorListener) {
                        console.warn("Overwriting existing error listener");
                    }
                    this.errorListener = listener as any;
                }
                */
               this.worker.addEventListener(type as any, listener);
            }
        };
    }
}

export async function createGeneratorService(): Promise<GeneratorService> {
    /*
    const workerMaps = [directFixedWorkerMap];

    if (BM_LANGUAGES.length > 0) {
        workerMaps.push(await createDirectBmWorkerMap());
    }
    if (EBM_LANGUAGES.length > 0) {
        workerMaps.push(await createDirectEbmWorkerMap());
    }
    */

    const service = new GeneratorService();
    await service.init();
    return service;
}

export function createApp(service: GeneratorService): Hono {
    const app = new Hono();

    // ---- MCP endpoint (Streamable HTTP, stateless) ----
    // A new transport + server is created per request because stateless mode
    // prohibits reusing the same transport across multiple requests.
    app.all("/mcp", async (c) => {
        const transport = new WebStandardStreamableHTTPServerTransport({
            sessionIdGenerator: undefined, // stateless mode
            enableJsonResponse: true,      // return JSON instead of SSE for simple tool calls
        });
        const mcpServer = createMcpServer(service);
        await mcpServer.connect(transport);
        return transport.handleRequest(c.req.raw);
    });

    // ---- REST endpoints ----

    app.post("/api/generate", async (c) => {
        const body = await c.req.json<{
            source: string;
            lang: string;
            options?: Record<string, any>;
        }>();

        if (!body.source || !body.lang) {
            return c.json({ error: "source and lang are required" }, 400);
        }

        if (!allLanguageIds.includes(body.lang)) {
            return c.json({ error: `unsupported language: ${body.lang}` }, 400);
        }

        const options = body.options ?? {};

        const getConfig: ConfigReader = (_lang, key) => {
            return options[key];
        };

        const tracer = new UpdateTracer();

        return new Promise<Response>((resolve) => {
            service.generate(body.source, body.lang as Language, getConfig, (result: GenerateResult) => {
                resolve(
                    c.json({
                        code: result.code,
                        lang: result.lang,
                        isError: result.isError,
                        ...(result.mappingInfo ? { mappingInfo: result.mappingInfo } : {}),
                    }),
                );
            },tracer);
        });
    });

    app.get("/api/languages", (c) => {
        return c.json(
            languageRegistry.map((l) => ({
                id: l.id,
                displayName: l.displayName,
                category: l.category,
                options: l.options.map((o) => ({
                    key: o.key,
                    type: o.type,
                    ...(o.candidates ? { candidates: o.candidates } : {}),
                })),
            })),
        );
    });

    return app;
}
