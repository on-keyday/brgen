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

const dbVersion = 1;


interface DBRequest<T,E> {
    onsuccess :null | ((...arg:any) => void);
    onerror :null | ((...arg:any) => void);
    readonly result :T;
    readonly error :E;
}

function wait<T,E>(r :DBRequest<T,E>) {
    return new Promise<T>((resolve,reject) => {
        r.onsuccess = () => {
            resolve(r.result);
        }
        r.onerror = () => {
            reject(r.error);
        }
    });
}

class dbStorage {
    #storage :IDBFactory = globalThis.indexedDB;
    #db :IDBDatabase | undefined = undefined;

    async #openDB() {
        const request = this.#storage.open("s2j",dbVersion);
        request.onupgradeneeded = (event) => {
            const db = request.result;
            db.createObjectStore("data")
        }
        const db = await wait(request);
        return this.#db = db;
    }

    async #getDB() {
        if(this.#db !== undefined) return this.#db;
        return await this.#openDB();
    }

    async getItem(key :StorageKey) {
        const db = await this.#getDB();
        const transaction = db.transaction("data","readonly");
        const store = transaction.objectStore("data");
        return await wait(store.get(key));
    }

    async setItem(key :StorageKey,value :any) {
        const db = await this.#getDB();
        const transaction = db.transaction("data","readwrite");
        const store = transaction.objectStore("data");
        return await wait(store.put(value,key));
    }

    async clear() { 
        const db = await this.#getDB();
        const transaction = db.transaction("data","readwrite");
        const store = transaction.objectStore("data");
        return await wait(store.clear());
    }
}

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
