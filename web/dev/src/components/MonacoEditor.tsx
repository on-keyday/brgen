import { useRef, useEffect, useCallback } from "preact/hooks";
import * as monaco from "monaco-editor";

import "monaco-editor/esm/vs/language/json/monaco.contribution";
import "monaco-editor/esm/vs/basic-languages/cpp/cpp.contribution";
import "monaco-editor/esm/vs/basic-languages/go/go.contribution";

// Ensure Monaco workers are configured
if (typeof window !== "undefined" && !window.MonacoEnvironment) {
  window.MonacoEnvironment = {
    getWorker: (_moduleId: string, label: string) => {
      if (label === "json") {
        return new Worker(
          new URL(
            "../../node_modules/monaco-editor/esm/vs/language/json/json.worker",
            import.meta.url,
          ),
          { type: "module" },
        );
      }
      if (label === "editorWorkerService") {
        return new Worker(
          new URL(
            "../../node_modules/monaco-editor/esm/vs/editor/editor.worker",
            import.meta.url,
          ),
          { type: "module" },
        );
      }
      throw new Error(`Cannot create worker for label ${label}`);
    },
  };
}

export interface MonacoEditorProps {
  /** Current editor text */
  value: string;
  /** Monaco language ID (e.g. "cpp", "json", "brgen") */
  language: string;
  /** Called when the user edits the content (only for writable editors) */
  onChange?: (value: string) => void;
  /** Whether the editor is read-only */
  readOnly?: boolean;
  /** Optional Monaco theme name */
  theme?: string;
  /** Callback to receive the editor instance (called on mount, null on unmount) */
  editorRef?: (editor: monaco.editor.IStandaloneCodeEditor | null) => void;
}

/**
 * Wraps a Monaco editor instance as a Preact component.
 * Manages the lifecycle: create on mount, dispose on unmount.
 * Value and language changes are applied without recreating the editor.
 *
 * Design notes on value synchronization:
 * - For writable editors: the editor's internal state is the source of truth.
 *   The onChange callback notifies the parent of user edits. When `value` prop
 *   changes externally (e.g., loading a file), we apply it via `applyEdits`
 *   to preserve undo history.
 * - For read-only editors: `value` prop is the source of truth. We use
 *   `model.setValue()` for simplicity since undo/cursor don't matter.
 */
export function MonacoEditor({
  value,
  language,
  onChange,
  readOnly = false,
  theme,
  editorRef: editorRefCallback,
}: MonacoEditorProps) {
  const containerRef = useRef<HTMLDivElement>(null);
  const editorRef = useRef<monaco.editor.IStandaloneCodeEditor | null>(null);

  // Track the latest onChange in a ref so the model's onDidChangeContent
  // listener always calls the current callback without re-subscribing.
  const onChangeRef = useRef(onChange);
  onChangeRef.current = onChange;

  // Track whether a programmatic edit is in progress to avoid feedback loops.
  const suppressChange = useRef(false);

  // Track the last value we pushed INTO the editor from props, so we can
  // distinguish "our prop changed" from "editor echoed back what we set".
  const lastPushedValue = useRef(value);

  // Create editor on mount
  useEffect(() => {
    const container = containerRef.current;
    if (!container) return;

    const model = monaco.editor.createModel(value, language);
    const editor = monaco.editor.create(container, {
      model,
      lineHeight: 20,
      automaticLayout: true,
      colorDecorators: true,
      readOnly,
      hover: { enabled: true },
      theme,
      // Always enable semantic highlighting for all editors. Monaco's
      // standalone configurationService is a global singleton: the LAST
      // editor created wins. If one editor passes `false` (the default),
      // it overwrites the global config and cancels the scheduled
      // _fetchDocumentSemanticTokens for ALL editors — including ones
      // that need it (e.g., the brgen source editor).
      "semanticHighlighting.enabled": true,
      minimap: { enabled: false },
      scrollBeyondLastLine: false,
    });

    editorRef.current = editor;
    editorRefCallback?.(editor);

    // Listen for user edits (writable editors only)
    const disposable = model.onDidChangeContent(() => {
      if (suppressChange.current) return;
      const val = editor.getValue();
      lastPushedValue.current = val;
      onChangeRef.current?.(val);
    });

    return () => {
      editorRefCallback?.(null);
      disposable.dispose();
      editor.dispose();
      model.dispose();
      editorRef.current = null;
    };
    // Only run on mount/unmount
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // Sync value changes from props (external updates only)
  useEffect(() => {
    const editor = editorRef.current;
    if (!editor) return;
    const model = editor.getModel();
    if (!model) return;

    // Skip if this is just the editor echoing back a user edit we already
    // forwarded via onChange (the Zustand round-trip).
    if (value === lastPushedValue.current) return;

    // The value genuinely changed externally (file load, sharing link, etc.)
    lastPushedValue.current = value;
    suppressChange.current = true;

    if (readOnly) {
      // For read-only editors, setValue is fine (no undo/cursor to preserve)
      model.setValue(value);
    } else {
      // For writable editors, use applyEdits to preserve undo stack.
      // Replace the entire content as a single edit operation.
      const fullRange = model.getFullModelRange();
      model.applyEdits([{
        range: fullRange,
        text: value,
      }]);
    }

    suppressChange.current = false;
  }, [value, readOnly]);

  // Sync language changes
  useEffect(() => {
    const editor = editorRef.current;
    if (!editor) return;
    const model = editor.getModel();
    if (model && model.getLanguageId() !== language) {
      monaco.editor.setModelLanguage(model, language);
    }
  }, [language]);

  return (
    <div
      ref={containerRef}
      style={{ width: "100%", height: "100%", position: "absolute", top: 0, left: 0 }}
    />
  );
}

/**
 * Get a reference to the underlying editor instance.
 * Useful for advanced operations like decorations, actions, etc.
 */
export function useMonacoEditorRef() {
  const ref = useRef<monaco.editor.IStandaloneCodeEditor | null>(null);
  const setRef = useCallback((editor: monaco.editor.IStandaloneCodeEditor | null) => {
    ref.current = editor;
  }, []);
  return [ref, setRef] as const;
}
