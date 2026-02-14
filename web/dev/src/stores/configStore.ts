import { create } from "zustand";
import { Language } from "../s2j/msg";
import { ConfigKey } from "../types";
import { setBMUIConfig } from "../lib/bmgen/bm_caller.js";
import { setEBMUIConfig } from "../lib/bmgen/ebm_caller.js";

const STORAGE_KEY = "lang_specific_option";

/** Matches the existing InputListElement shape from ui.tsx */
export interface ConfigEntry {
    name?: string;
    readonly type: "checkbox" | "number" | "text";
    value: boolean | number | string;
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

    // C++
    config["cpp"] = {
        ...common(),
        [ConfigKey.CPP_SOURCE_MAP]: { type: "checkbox", value: false },
        [ConfigKey.CPP_EXPAND_INCLUDE]: { type: "checkbox", value: false },
        [ConfigKey.CPP_USE_ERROR]: { type: "checkbox", value: false },
        [ConfigKey.CPP_USE_RAW_UNION]: { type: "checkbox", value: false },
        [ConfigKey.CPP_CHECK_OVERFLOW]: { type: "checkbox", value: false },
        [ConfigKey.CPP_ENUM_STRINGER]: { type: "checkbox", value: false },
        [ConfigKey.CPP_USE_CONSTEXPR]: { type: "checkbox", value: false },
        [ConfigKey.CPP_ADD_VISIT]: { type: "checkbox", value: false },
        [ConfigKey.CPP_FORCE_OPTIONAL_GETTER]: { type: "checkbox", value: false },
    };

    // Go
    config["go"] = {
        ...common(),
        [ConfigKey.GO_OMIT_MUST_ENCODE]: { type: "checkbox", value: false },
        [ConfigKey.GO_OMIT_DECODE_EXACT]: { type: "checkbox", value: false },
        [ConfigKey.GO_OMIT_VISITOR]: { type: "checkbox", value: false },
        [ConfigKey.GO_OMIT_MARSHAL_JSON]: { type: "checkbox", value: false },
    };

    // C
    config["c"] = {
        ...common(),
        [ConfigKey.C_MULTI_FILE]: { type: "checkbox", value: false },
        [ConfigKey.C_OMIT_ERROR_CALLBACK]: { type: "checkbox", value: false },
        [ConfigKey.C_USE_MEMCPY]: { type: "checkbox", value: false },
        [ConfigKey.C_ZERO_COPY]: { type: "checkbox", value: false },
    };

    // Rust
    config["rust"] = { ...common() };

    // TypeScript
    config["typescript"] = {
        ...common(),
        [ConfigKey.TS_JAVASCRIPT]: { type: "checkbox", value: false },
    };

    // Kaitai Struct
    config["kaitai struct"] = { ...common() };

    // Binary Module
    config["binary module"] = {
        ...common(),
        [ConfigKey.BM_PRINT_INSTRUCTION]: { type: "checkbox", value: false },
    };

    // Extended Binary Module
    config["ebm"] = {
        ...common(),
        [ConfigKey.EBM_PRINT_INSTRUCTION]: { type: "checkbox", value: false },
        [ConfigKey.EBM_NO_OUTPUT]: { type: "checkbox", value: false },
        [ConfigKey.EBM_CONTROL_FLOW_GRAPH]: { type: "checkbox", value: false },
    };

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
