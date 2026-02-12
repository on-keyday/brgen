import * as monaco from "monaco-editor";
import { MappingInfo } from "../s2j/generator";

/**
 * Source map overlay colors -- same palette as the legacy implementation.
 * Each color maps source lines to generated lines using the same hue.
 */
const COLORS = [
  "rgba(255, 0, 0, 0.3)",     // red
  "rgba(0, 0, 255, 0.3)",     // blue
  "rgba(255, 255, 0, 0.3)",   // yellow
  "rgba(0, 255, 0, 0.3)",     // green
  "rgba(255, 0, 255, 0.3)",   // purple
  "rgba(255, 165, 0, 0.3)",   // orange
];

// Register CSS classes for each color once
let cssRegistered = false;
function ensureCssRegistered() {
  if (cssRegistered) return;
  cssRegistered = true;
  const style = document.createElement("style");
  style.textContent = COLORS.map(
    (color, i) =>
      `.brgen-map-${i} { background-color: ${color}; }`
  ).join("\n");
  document.head.appendChild(style);
}

/**
 * Manages source map decorations on a pair of Monaco editors.
 * Uses `createDecorationsCollection` (public API) instead of
 * the legacy MutationObserver + DOM manipulation approach.
 */
export class SourceMapOverlay {
  #sourceDecorations: monaco.editor.IEditorDecorationsCollection | null = null;
  #generatedDecorations: monaco.editor.IEditorDecorationsCollection | null = null;

  /**
   * Apply source-to-generated line mapping decorations to both editors.
   *
   * @param sourceEditor    The source (.bgn) editor
   * @param generatedEditor The generated code editor
   * @param mappingInfo     Array of source→generated line mappings
   */
  apply(
    sourceEditor: monaco.editor.IStandaloneCodeEditor | null,
    generatedEditor: monaco.editor.IStandaloneCodeEditor | null,
    mappingInfo: MappingInfo[] | null,
  ): void {
    ensureCssRegistered();

    // Clear previous decorations
    this.clear();
    console.log("Applying source map overlay decorations.");

    if (!sourceEditor || !generatedEditor || !mappingInfo || mappingInfo.length === 0) {
      return;
    }

    const sourceModel = sourceEditor.getModel();
    const generatedModel = generatedEditor.getModel();
    if (!sourceModel || !generatedModel) return;

    const sourceDecos: monaco.editor.IModelDeltaDecoration[] = [];
    const generatedDecos: monaco.editor.IModelDeltaDecoration[] = [];

    // Track which source lines we've already decorated (avoid duplicate overlapping decorations)
    const seenSourceLines = new Set<number>();
    const seenGeneratedLines = new Set<number>();

    for (const mapping of mappingInfo) {
      const sourceLine = mapping.loc.line;
      const generatedLine = mapping.line;
      const colorIndex = sourceLine % COLORS.length;
      const className = `brgen-map-${colorIndex}`;

      // Only decorate within valid line ranges
      if (sourceLine >= 1 && sourceLine <= sourceModel.getLineCount() && !seenSourceLines.has(sourceLine)) {
        seenSourceLines.add(sourceLine);
        sourceDecos.push({
          range: new monaco.Range(sourceLine, 1, sourceLine, sourceModel.getLineMaxColumn(sourceLine)),
          options: {
            isWholeLine: true,
            className,
          },
        });
      }

      if (generatedLine >= 1 && generatedLine <= generatedModel.getLineCount() && !seenGeneratedLines.has(generatedLine)) {
        seenGeneratedLines.add(generatedLine);
        generatedDecos.push({
          range: new monaco.Range(generatedLine, 1, generatedLine, generatedModel.getLineMaxColumn(generatedLine)),
          options: {
            isWholeLine: true,
            className,
          },
        });
      }
    }

    console.log(`Applying ${sourceDecos.length} source and ${generatedDecos.length} generated decorations.`);
    this.#sourceDecorations = sourceEditor.createDecorationsCollection(sourceDecos);
    this.#generatedDecorations = generatedEditor.createDecorationsCollection(generatedDecos);
  }

  /**
   * Remove all source map decorations from both editors.
   */
  clear(): void {
    this.#sourceDecorations?.clear();
    this.#generatedDecorations?.clear();
    this.#sourceDecorations = null;
    this.#generatedDecorations = null;
  }
}
