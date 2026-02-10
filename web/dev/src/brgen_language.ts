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
  // Ensure workers are registered before passing the factory to LSP.
  // initLSP will synchronously register the language and providers;
  // the factory itself handles lazy worker creation.
  service.init().then(() => {
    // factory is usable even before init completes for language registration
  });
  initLSP(service.factory);
}
