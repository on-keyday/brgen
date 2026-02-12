import { initLSP } from "./lsp/brgen_lsp";
import { getGeneratorService } from "./generator_service";

let initialized = false;

/**
 * Initialize the brgen language mode (registration, theme, semantic tokens, hover)
 * by wiring up the LSP to the GeneratorService's worker factory.
 *
 * Safe to call multiple times -- only runs once.
 */
export function initBrgenLanguage(): void {
  if (initialized) return;
  initialized = true;

  const service = getGeneratorService();
  // Core workers are registered synchronously in the constructor,
  // so factory.getWorker() works immediately for src2json etc.
  // Kick off BM/EBM dynamic imports in the background.
  service.init();
  initLSP(service.factory);
}
