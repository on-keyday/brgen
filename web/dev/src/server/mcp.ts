import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { z } from "zod";
import { GeneratorService } from "../generator_service.js";
import { Language } from "../s2j/msg.js";
import { languageRegistry, allLanguageIds } from "../languages.js";
import { UpdateTracer } from "../s2j/update.js";

/**
 * Creates and configures an MCP server exposing brgen's code generation
 * pipeline as callable tools.
 *
 * Tools:
 *   - generate_code  : Generate code from .bgn source in a target language
 *   - list_languages : List all available target languages with metadata
 */
export function createMcpServer(service: GeneratorService,logger?: (...args: any[]) => void): McpServer {
    const server = new McpServer({
        name: "brgen",
        version: "1.0.0",
    });

    // ---- Tool: generate_code ----
    server.registerTool(
        "generate_code",
        {
            description:
                "Generate code from brgen binary format (.bgn) source. " +
                "Parses the .bgn source, builds an AST, then emits code in the requested language. " +
                "Use list_languages to discover available targets.",
            inputSchema: {
                source: z.string().describe(
                    "The .bgn source code to generate from. " +
                    "Example: 'format Packet:\\n  length :u16\\n  data :[length]u8\\n'"
                ),
                lang: z.string().describe(
                    "Target language ID. Common values: 'cpp', 'go', 'c', 'rust', 'typescript', " +
                    "'kaitai struct', 'json ast'. Use list_languages for the full list."
                ),
                options: z.record(z.string(), z.any()).optional().describe(
                    "Language-specific generation options. " +
                    "Examples for C++: { use_error: true, enum_stringer: true }. " +
                    "Examples for Go: { omit_must_encode: true }."
                ),
            },
        },
        async ({ source, lang, options = {} }) => {
            if (!allLanguageIds.includes(lang)) {
                return {
                    content: [{
                        type: "text" as const,
                        text: `Unsupported language: '${lang}'. ` +
                              `Call list_languages to see available options.`,
                    }],
                    isError: true,
                };
            }

            return new Promise((resolve) => {
                const tracer = new UpdateTracer();
                service.generate(
                    source,
                    lang as Language,
                    (_l, key) => options[key],
                    (result) => {
                        resolve({
                            content: [{
                                type: "text" as const,
                                text: result.code,
                            }],
                            isError: result.isError,
                        });
                    },
                    tracer,
                    logger ? (...args) => logger("[Generation]", ...args) : undefined,
                );
            });
        },
    );

    // ---- Tool: list_languages ----
    server.registerTool(
        "list_languages",
        {
            description:
                "List all available code generation target languages, grouped by category. " +
                "Returns language IDs to pass to generate_code.",
            inputSchema: {},
        },
        async () => {
            const grouped: Record<string, typeof languageRegistry[number][]> = {};

            for (const lang of languageRegistry) {
                if (!grouped[lang.category]) {
                    grouped[lang.category] = [];
                }
                grouped[lang.category].push(lang);
            }

            const lines: string[] = ["Available languages:\n"];
            for (const [category, langs] of Object.entries(grouped)) {
                lines.push(`[${category}]`);
                for (const l of langs) {
                    lines.push(`  ${l.id}  (${l.displayName})`);
                    for (const o of l.options) {
                        lines.push(`    option: ${o.key}  type: ${o.type}${o.candidates ? `  values: ${o.candidates.join("|")}` : ""}`);
                    }
                }
                lines.push("");
            }

            return {
                content: [{ type: "text" as const, text: lines.join("\n") }],
            };
        },
    );

    return server;
}
