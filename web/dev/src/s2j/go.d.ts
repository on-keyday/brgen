
declare class Go {
    constructor();
    run(instance: WebAssembly.Instance): Promise<void>;
    importObject: WebAssembly.Imports;
    // for use in the browser
    execGenerator(args :string[]) : { stdout: string , stderr: string , code: number};
}
