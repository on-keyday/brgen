import { create } from "zustand";
import { Language } from "../s2j/msg";
import { ConfigKey } from "../types";
import { getGeneratorService, GenerateResult, ConfigReader } from "../generator_service";
import { MappingInfo } from "../s2j/generator";
import { useEditorStore } from "./editorStore";
import { useConfigStore } from "./configStore";

export interface GeneratorState {
    /** Whether a generation is currently in progress */
    generating: boolean;
    /** The most recent generation result */
    result: GenerateResult | null;
    /** Source mapping info from the most recent CPP generation */
    mappingInfo: MappingInfo[] | null;

    /**
     * Trigger code generation using current editor source and language.
     * Reads source/language from editorStore and config from configStore.
     */
    generate: () => Promise<void>;
}

export const useGeneratorStore = create<GeneratorState>()((set) => ({
    generating: false,
    result: null,
    mappingInfo: null,

    generate: async () => {
        const { source, language } = useEditorStore.getState();
        const { getConfig } = useConfigStore.getState();

        set({ generating: true, mappingInfo: null });

        const service = getGeneratorService();
        const configReader: ConfigReader = (lang: Language, key: ConfigKey) => {
            return getConfig(lang, key);
        };

        await service.generate(source, language, configReader, (result) => {
            set({
                generating: false,
                result,
                mappingInfo: result.mappingInfo ?? null,
            });
        });
    },
}));

/**
 * Debounced generation trigger.
 * Call this on editor content changes -- it waits for the user to stop
 * typing before starting generation.
 */
let _debounceTimer: ReturnType<typeof setTimeout> | null = null;

export function debouncedGenerate(delayMs: number = 300): void {
    if (_debounceTimer) clearTimeout(_debounceTimer);
    _debounceTimer = setTimeout(() => {
        _debounceTimer = null;
        useGeneratorStore.getState().generate();
    }, delayMs);
}

/**
 * Immediate generation trigger (e.g., on language change or Ctrl+S).
 */
export function immediateGenerate(): void {
    if (_debounceTimer) {
        clearTimeout(_debounceTimer);
        _debounceTimer = null;
    }
    useGeneratorStore.getState().generate();
}
