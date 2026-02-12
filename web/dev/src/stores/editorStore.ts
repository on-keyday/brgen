import { create } from "zustand";
import { Language } from "../s2j/msg";
import { allLanguageIds } from "../languages";

const STORAGE_KEY_LANG = "lang_mode";
const STORAGE_KEY_SOURCE = "source_code";

const DEFAULT_SOURCE = `
format WebSocketFrame:
    FIN :u1
    RSV1 :u1
    RSV2 :u1
    RSV3 :u1
    Opcode :u4
    Mask :u1
    PayloadLength :u7
    match PayloadLength:
        126 => ExtendedPayloadLength :u16
        127 => ExtendedPayloadLength :u64
    
    if Mask == 1:
        MaskingKey :u32
    
    Payload :[available(ExtendedPayloadLength) ? ExtendedPayloadLength : PayloadLength]u8
`;

function loadLang(): Language {
    try {
        const stored = localStorage.getItem(STORAGE_KEY_LANG);
        if (stored && allLanguageIds.includes(stored)) {
            return stored as Language;
        }
    } catch { /* localStorage may be unavailable */ }
    return Language.CPP;
}

function loadSource(): string {
    try {
        const stored = localStorage.getItem(STORAGE_KEY_SOURCE);
        if (stored !== null) return stored;
    } catch { /* localStorage may be unavailable */ }
    return DEFAULT_SOURCE;
}

/**
 * Parse URL hash parameters (#lang=...&code=...) for sharing links.
 * Returns overrides if present, otherwise undefined for each.
 */
function parseUrlHash(): { lang?: Language; source?: string } {
    const result: { lang?: Language; source?: string } = {};
    try {
        const params = new URLSearchParams(window.location.hash.substring(1));
        const langParam = params.get("lang");
        if (langParam && allLanguageIds.includes(langParam)) {
            result.lang = langParam as Language;
        }
        const codeParam = params.get("code");
        if (codeParam) {
            const bytes = Uint8Array.from(atob(codeParam), c => c.charCodeAt(0));
            result.source = new TextDecoder("utf-8").decode(bytes);
        }
    } catch { /* ignore malformed hashes */ }
    return result;
}

export interface EditorState {
    /** Current .bgn source code */
    source: string;
    /** Currently selected target language */
    language: Language;
    /** Whether to force save language mode if some edits are made */
    forceSaveLanguage: boolean;

    /** Update source code (persists to localStorage) */
    setSource: (source: string) => void;
    /** Change target language (persists to localStorage) */
    setLanguage: (lang: Language) => void;
    /** Generate a sharing URL hash */
    getShareHash: () => string;
}

export const useEditorStore = create<EditorState>()((set, get) => {
    // Apply URL hash overrides on first load
    const urlOverrides = parseUrlHash();
    const initialLang = urlOverrides.lang ?? loadLang();
    const initialSource = urlOverrides.source ?? loadSource();

    /*
    // If URL provided overrides, persist them
    if (urlOverrides.lang) {
        try { localStorage.setItem(STORAGE_KEY_LANG, initialLang); } catch {}
    }
    */

    return {
        source: initialSource,
        language: initialLang,
        forceSaveLanguage: urlOverrides.lang ? true : false,

        setSource: (source: string) => {
            const forceSave = get().forceSaveLanguage;
            if (forceSave) {
                // Persist language mode if forced
                try { localStorage.setItem(STORAGE_KEY_LANG, get().language); } catch {}
            }
            set({ source, forceSaveLanguage: false });
            try { localStorage.setItem(STORAGE_KEY_SOURCE, source); } catch {}
        },

        setLanguage: (lang: Language) => {
            if (!allLanguageIds.includes(lang)) return;
            set({ language: lang, forceSaveLanguage: false });
            try { localStorage.setItem(STORAGE_KEY_LANG, lang); } catch {}
        },

        getShareHash: () => {
            const { source, language } = get();
            const encoded = btoa(String.fromCharCode(...new TextEncoder().encode(source)));
            return `#lang=${encodeURIComponent(language)}&code=${encoded}`;
        },
    };
});
