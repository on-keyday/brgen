import { JobResult,Language,LanguageList } from "./s2j/msg.js";
const enum StorageKey {
    LANG_MODE = "lang_mode",
    SOURCE_CODE = "source_code",
    LANG_SPECIFIC_OPTION = "lang_specific_option",
}

const sample =`
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
`

class storageManager {
    #storage :Storage =  globalThis.localStorage;
    #langMode :Language | undefined = undefined;

    getLangMode() {
        if(this.#langMode !== undefined){ 
            return this.#langMode;
        }
        const mode = this.#storage.getItem(StorageKey.LANG_MODE);
        if(mode === null) return Language.CPP;
        if(LanguageList.includes(mode as Language)){
            this.#langMode = mode as Language;
            return mode as Language;
        }
        this.#langMode = Language.CPP;
        return Language.CPP;
    }
    
    getSourceCode() {
        const code = this.#storage.getItem(StorageKey.SOURCE_CODE);
        if(code === null) return sample;
        return code;
    }

    setLangMode(mode:Language) {
        this.#storage.setItem(StorageKey.LANG_MODE,mode);
        this.#langMode = mode;
    }

    setSourceCode(code:string) {
        this.#storage.setItem(StorageKey.SOURCE_CODE,code);
    }
}

export const storage = new storageManager();
