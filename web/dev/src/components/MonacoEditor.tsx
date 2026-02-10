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
  /** Enable semantic highlighting */
  semanticHighlighting?: boolean;
  /** Callback to receive the editor instance (called on mount, null on unmount) */
  editorRef?: (editor: monaco.editor.IStandaloneCodeEditor | null) => void;
}

/**
 * Wraps a Monaco editor instance as a Preact component.
 * Manages the lifecycle: create on mount, dispose on unmount.
 * Value and language changes are applied without recreating the editor.
 */
export function MonacoEditor({
  value,
  language,
  onChange,
  readOnly = false,
  theme,
  semanticHighlighting = false,
  editorRef: editorRefCallback,
}: MonacoEditorProps) {
  const containerRef = useRef<HTMLDivElement>(null);
  const editorRef = useRef<monaco.editor.IStandaloneCodeEditor | null>(null);
  /** Suppress onChange while we programmatically set value */
  const suppressChange = useRef(false);

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
      theme: theme ?? (readOnly ? undefined : undefined),
      "semanticHighlighting.enabled": semanticHighlighting,
      minimap: { enabled: false },
      scrollBeyondLastLine: false,
    });

    editorRef.current = editor;
    editorRefCallback?.(editor);

    if (onChange) {
      model.onDidChangeContent(() => {
        if (suppressChange.current) return;
        onChange(editor.getValue());
      });
    }

    return () => {
      editorRefCallback?.(null);
      editor.dispose();
      model.dispose();
      editorRef.current = null;
    };
    // Only run on mount/unmount
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // Sync value changes from props (external updates)
  useEffect(() => {
    const editor = editorRef.current;
    if (!editor) return;
    const currentValue = editor.getValue();
    if (currentValue !== value) {
      suppressChange.current = true;
      const model = editor.getModel();
      if (model) {
        // Preserve scroll position
        const scrollTop = editor.getScrollTop();
        model.setValue(value);
        editor.setScrollTop(scrollTop);
      }
      suppressChange.current = false;
    }
  }, [value]);

  // Sync language changes
  useEffect(() => {
    const editor = editorRef.current;
    if (!editor) return;
    const model = editor.getModel();
    if (model) {
      const currentLang = model.getLanguageId();
      if (currentLang !== language) {
        // For read-only editors, swap the model to change language properly
        if (readOnly) {
          const scrollTop = editor.getScrollTop();
          const oldModel = model;
          const newModel = monaco.editor.createModel(
            editor.getValue(),
            language,
          );
          editor.setModel(newModel);
          editor.setScrollTop(scrollTop);
          oldModel.dispose();
        } else {
          monaco.editor.setModelLanguage(model, language);
        }
      }
    }
  }, [language, readOnly]);

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
