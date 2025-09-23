
import { updateGenerated } from "../s2j/generator.js";
import { Hono } from 'hono'
import { RequestLanguage } from "../s2j/msg.js";
import { WorkerFactory } from "../s2j/worker_factory.js";
import { fixedWorkerMap } from "../s2j/workers.js";
import {bm_workers} from "../lib/bmgen/bm_workers.js"
import {ebm_workers} from "../lib/bmgen/ebm_workers.js"
import { UpdateTracer } from "../s2j/update.js";


interface GenerateRequest {
    source_code :string;
    lang :string;
    options :{ [key: string]: string|boolean }
}

function isGenerateRequest(x :any): x is GenerateRequest {
    return typeof x?.source_code === "string" &&
         typeof x?.lang === "string" &&
        typeof x?.options === "object"&&
        !Array.isArray(x?.options);
}

interface IFileSystem {
    getResource(path :string) :Promise<{data: Uint8Array<ArrayBuffer>, contentType? :string} | null>;
}

export const createApp = (fs :IFileSystem) => {
    const worker = new WorkerFactory();

    worker.addWorker(fixedWorkerMap)
    worker.addWorker(bm_workers)
    worker.addWorker(ebm_workers)

    const app = new Hono()
    app.get("/brgen/:filename{.*}",(c) => {
        const path = c.req.param("filename") || "index.html";
        return fs.getResource(path).then((data) => {
            if(data === null) {
                return c.text("Not Found",404);
            }
            return c.body(data.data,{headers: data.contentType ? { 
                "Content-Type": data.contentType,
                "Cross-Origin-Opener-Policy": "same-origin",
                "Cross-Origin-Embedder-Policy": "require-corp"
             } : {}});
        });
    })

    app.post('/generate', async(c) => {
        const body = await c.req.json()
        if(!isGenerateRequest(body)){
            return c.json({"error": "invalid json"},400)
        }
        let result :string | null = null
        try {
            const updateTracer = new UpdateTracer(); // per request
            await updateGenerated({
                getValue: () => {
                    return body.source_code
                },
                setDefault: ()=> {},
                setGenerated: (s, lang) => {
                    result = s;
                },
                getLanguageConfig: (lang,key) => {
                    return body.options[key]
                },
                mappingCode: () => {},
                getWorkerFactory: () => {
                    return worker
                },
                getUpdateTracer: () => {
                    return updateTracer;
                }
            },body.lang as RequestLanguage)
        }catch(e :any) {
            console.log(e);
            return c.json({"error": e.toString()},500)
        }
        return c.json({"result": result})
    })
    return app;
}
