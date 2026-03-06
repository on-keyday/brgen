import { create } from "zustand";
import { Language } from "../s2j/msg";
import { ConfigKey } from "../common/types";
import { setBMUIConfig } from "../lib/bmgen/bm_caller.js";
import { setEBMUIConfig } from "../lib/bmgen/ebm_caller.js";
import { coreOptionDefs } from "../common/option_defs";

const STORAGE_KEY = "lang_specific_option";

/** Matches the existing InputListElement shape from ui.tsx */
export interface ConfigEntry {
    name?: string;
    readonly type: "checkbox" | "number" | "text" | "choice";
    value: boolean | number | string;
    candidates? :string[];
    help?: string;
    data?: any;
}

/**
 * Per-language configuration state.
 * Mirrors the existing `commonUI.config` Map<Language, { config: Map<string, InputListElement> }>`.
 * The store flattens this to `Record<string, Record<string, ConfigEntry>>` for serialization.
 */
export interface ConfigState {
    /** config[language][key] = ConfigEntry */
    config: Record<string, Record<string, ConfigEntry>>;

    /** Get a config value for a language+key. Returns undefined if not found. */
    getConfig: (lang: Language, key: ConfigKey | string) => any;

    /** Set a config value for a language+key. */
    setConfig: (lang: Language, key: string, value: boolean | number | string) => void;

    /** Persist current config to localStorage. */
    save: () => void;
}

/**
 * Build the default config entries for all core languages.
 * This mirrors the imperative setup in index.tsx lines 676-873.
 */
function buildDefaultConfig(): Record<string, Record<string, ConfigEntry>> {
    const config: Record<string, Record<string, ConfigEntry>> = {};

    const common = (): Record<string, ConfigEntry> => ({
        [ConfigKey.COMMON_FILE_NAME]: { type: "text", value: "editor.bgn" },
        // [ConfigKey.COMMON_SAVE_LOCATION]: { type: "text", value: "" },
    });

    // Analysis languages -- common config only
    config["tokens"] = { ...common() };
    config["json ast"] = { ...common() };
    config["json ast (debug)"] = { ...common() };

    // Languages with no generator-specific options (common only)
    config["rust"] = { ...common() };
    config["kaitai struct"] = { ...common() };

    // Build config entries from the SSOT option definitions
    for (const [lang, defs] of Object.entries(coreOptionDefs)) {
        config[lang] = { ...common() };
        for (const def of defs) {
            config[lang][def.key] = {
                type: def.type,
                value: def.defaultValue,
                ...(def.candidates ? { candidates: def.candidates } : {}),
            };
        }
    }

    // BM-based languages (dynamically registered from auto-generated bm_caller.js)
    setBMUIConfig({
        set_flags: (lang: string, setter: (nest: (name: string, elem: ConfigEntry) => void) => void) => {
            const langConfig: Record<string, ConfigEntry> = { ...common() };
            setter((name, elem) => { langConfig[name] = elem; });
            config[lang] = langConfig;
        },
    });

    // EBM-based languages
    setEBMUIConfig({
        set_flags: (lang: string, setter: (nest: (name: string, elem: ConfigEntry) => void) => void) => {
            const langConfig: Record<string, ConfigEntry> = { ...common() };
            setter((name, elem) => { langConfig[name] = elem; });
            config[lang] = langConfig;
        },
    });

    return config;
}

function loadSavedConfig(config: Record<string, Record<string, ConfigEntry>>): void {
    try {
        const saved = localStorage.getItem(STORAGE_KEY);
        if (!saved) return;
        const parsed = JSON.parse(saved) as Record<string, Record<string, { value: any; data?: any }>>;
        for (const [lang, entries] of Object.entries(parsed)) {
            if (!config[lang]) continue;
            for (const [key, saved] of Object.entries(entries)) {
                if (!config[lang][key]) continue;
                config[lang][key].value = saved.value;
                if (saved.data !== undefined) {
                    config[lang][key].data = saved.data;
                }
            }
        }
    } catch { /* ignore corrupt storage */ }
}

export const useConfigStore = create<ConfigState>()((set, get) => {
    const defaultConfig = buildDefaultConfig();
    loadSavedConfig(defaultConfig);

    return {
        config: defaultConfig,

        getConfig: (lang: Language, key: ConfigKey | string) => {
            const langConfig = get().config[lang];
            if (!langConfig) return undefined;
            return langConfig[key]?.value;
        },

        setConfig: (lang: Language, key: string, value: boolean | number | string) => {
            set(state => {
                const langConfig = state.config[lang];
                if (!langConfig || !langConfig[key]) return state;
                return {
                    config: {
                        ...state.config,
                        [lang]: {
                            ...langConfig,
                            [key]: { ...langConfig[key], value },
                        },
                    },
                };
            });
        },

        save: () => {
            try {
                localStorage.setItem(STORAGE_KEY, JSON.stringify(get().config));
            } catch { /* localStorage may be unavailable */ }
        },
    };
});
