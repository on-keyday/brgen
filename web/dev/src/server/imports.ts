import type { ExtraSourceFile } from "../s2j/msg.js";

export class ImportValidationError extends Error {
    constructor(message: string) {
        super(message);
        this.name = "ImportValidationError";
    }
}

/**
 * Normalize a user-supplied `imports` map (filename → content) into the
 * ExtraSourceFile[] shape expected by the worker layer.
 *
 * Rejects path-traversal patterns and absolute paths so that the in-memory
 * Emscripten FS stays predictable. Only `.bgn` files are accepted because
 * src2json only consumes that extension.
 */
export function normalizeImports(
    imports: Record<string, string> | undefined | null,
): ExtraSourceFile[] | undefined {
    if (!imports) {
        return undefined;
    }

    const result: ExtraSourceFile[] = [];
    for (const [rawName, content] of Object.entries(imports)) {
        if (typeof content !== "string") {
            throw new ImportValidationError(
                `imports[${JSON.stringify(rawName)}] must be a string`,
            );
        }
        const fileName = rawName.replace(/\\/g, "/");
        if (fileName === "" || fileName.startsWith("/")) {
            throw new ImportValidationError(
                `imports key must be a relative path: ${JSON.stringify(rawName)}`,
            );
        }
        if (fileName.split("/").some((seg) => seg === "..")) {
            throw new ImportValidationError(
                `imports key must not contain '..': ${JSON.stringify(rawName)}`,
            );
        }
        if (!fileName.endsWith(".bgn")) {
            throw new ImportValidationError(
                `imports key must end with '.bgn': ${JSON.stringify(rawName)}`,
            );
        }
        result.push({ fileName, content });
    }
    return result.length > 0 ? result : undefined;
}
