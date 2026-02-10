import { WorkerFactory } from "./s2j/worker_factory";
import { fixedWorkerMap } from "./s2j/workers";
import { UpdateTracer } from "./s2j/update";
import { UIModel, MappingInfo, updateGenerated } from "./s2j/generator";
import { Language, JobResult } from "./s2j/msg";
import { ConfigKey } from "./types";
import { BM_LANGUAGES } from "./lib/bmgen/bm_caller.js";
import { EBM_LANGUAGES } from "./lib/bmgen/ebm_caller.js";

/**
 * Result of a code generation request.
 */
export interface GenerateResult {
    /** Generated code, or an error message */
    code: string;
    /** Monaco editor language ID for syntax highlighting */
    monacoLang: string;
    /** Whether the result is an error (stderr / crash) */
    isError: boolean;
    /** Source mapping info, if available (currently only CPP with source_map enabled) */
    mappingInfo?: MappingInfo[];
    /** The language that was generated */
    lang: string;
}

/**
 * Callback type for receiving generation results.
 * Called when a generation completes (or fails).
 */
export type GenerateCallback = (result: GenerateResult) => void;

/**
 * Configuration reader function type.
 * The GeneratorService doesn't own config state -- it asks the caller
 * for config values on each generation via this callback.
 */
export type ConfigReader = (lang: Language, key: ConfigKey) => any;

/**
 * GeneratorService owns the worker infrastructure and provides a clean
 * async generation API that Zustand stores can call.
 *
 * It bridges to the existing `updateGenerated()` function by constructing
 * a `UIModel` on each call. This avoids rewriting the existing generator
 * dispatch while giving the new Preact UI a simple interface.
 */
export class GeneratorService {
    readonly factory: InstanceType<typeof WorkerFactory>;
    readonly updateTracer: UpdateTracer;
    #initPromise: Promise<void> | null = null;

    constructor() {
        this.factory = new WorkerFactory();
        this.updateTracer = new UpdateTracer();
    }

    /**
     * Register all worker maps. Call once at app startup.
     * Returns a promise that resolves when all workers (including
     * dynamically imported BM/EBM workers) are registered.
     */
    init(): Promise<void> {
        if (this.#initPromise) return this.#initPromise;
        this.#initPromise = this.#doInit();
        return this.#initPromise;
    }

    async #doInit(): Promise<void> {
        // Core workers are always available synchronously
        this.factory.addWorker(fixedWorkerMap);

        // BM/EBM workers are dynamically imported (they may be stubs with empty arrays)
        const loaders: Promise<void>[] = [];

        if (BM_LANGUAGES.length > 0) {
            loaders.push(
                import("./lib/bmgen/bm_workers.js").then(m => {
                    this.factory.addWorker(m.bm_workers);
                })
            );
        }
        if (EBM_LANGUAGES.length > 0) {
            loaders.push(
                import("./lib/bmgen/ebm_workers.js").then(m => {
                    this.factory.addWorker(m.ebm_workers);
                })
            );
        }

        await Promise.all(loaders);
    }

    /**
     * Run code generation for the given source and language.
     *
     * This constructs a UIModel that captures the result via callbacks,
     * then delegates to the existing `updateGenerated()` pipeline.
     *
     * @param source      The .bgn source code
     * @param lang        The target language identifier
     * @param getConfig   Function to read per-language config values
     * @param onResult    Callback receiving the generation result
     */
    async generate(
        source: string,
        lang: Language,
        getConfig: ConfigReader,
        onResult: GenerateCallback,
    ): Promise<void> {
        await this.init();

        let result: GenerateResult = {
            code: "(generated code)",
            monacoLang: "text/plain",
            isError: false,
            lang: lang,
        };

        const ui: UIModel = {
            getWorkerFactory: () => this.factory,
            getUpdateTracer: () => this.updateTracer,
            getValue: () => source,
            setDefault: () => {
                result = {
                    code: "(generated code)",
                    monacoLang: "text/plain",
                    isError: false,
                    lang: lang,
                };
            },
            setGenerated: (s: string, monacoLang: string) => {
                const isError = monacoLang === "text/plain" && (
                    s.startsWith("(no output") ||
                    s.startsWith("// failed") ||
                    s.includes("error")
                );
                result = {
                    code: s,
                    monacoLang: monacoLang,
                    isError: isError,
                    lang: lang,
                };
            },
            getLanguageConfig: (configLang: Language, key: ConfigKey) => {
                return getConfig(configLang, key);
            },
            mappingCode: (mappingInfo: MappingInfo[], _s: JobResult, _lang: Language, _offset: number) => {
                result.mappingInfo = mappingInfo;
            },
        };

        try {
            await updateGenerated(ui, lang);
        } catch (error: any) {
            result = {
                code: `// failed to generate code: ${error?.message ?? error}`,
                monacoLang: "text/plain",
                isError: true,
                lang: lang,
            };
        }

        onResult(result);
    }
}

/** Singleton instance. Created once, shared across the app. */
let _instance: GeneratorService | null = null;

export function getGeneratorService(): GeneratorService {
    if (!_instance) {
        _instance = new GeneratorService();
    }
    return _instance;
}
