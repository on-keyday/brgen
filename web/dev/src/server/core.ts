import { Hono } from "hono";
import { GeneratorService, GenerateResult, ConfigReader } from "../generator_service.js";
import { Language } from "../s2j/msg.js";
import { languageRegistry, allLanguageIds } from "../languages.js";
import {
    directFixedWorkerMap,
    createDirectBmWorkerMap,
    createDirectEbmWorkerMap,
} from "./workers.js";
import { BM_LANGUAGES } from "../lib/bmgen/bm_caller.js";
import { EBM_LANGUAGES } from "../lib/bmgen/ebm_caller.js";

async function createGeneratorService(): Promise<GeneratorService> {
    const workerMaps = [directFixedWorkerMap];

    if (BM_LANGUAGES.length > 0) {
        workerMaps.push(await createDirectBmWorkerMap());
    }
    if (EBM_LANGUAGES.length > 0) {
        workerMaps.push(await createDirectEbmWorkerMap());
    }

    const service = new GeneratorService(workerMaps);
    await service.init();
    return service;
}

export function createApp(service: GeneratorService): Hono {
    const app = new Hono();

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
            });
        });
    });

    app.get("/api/languages", (c) => {
        return c.json(
            languageRegistry.map((l) => ({
                id: l.id,
                displayName: l.displayName,
                category: l.category,
            })),
        );
    });

    return app;
}

export { createGeneratorService };
