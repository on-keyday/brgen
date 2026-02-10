import "destyle.css";
import { useEffect, useCallback, useRef } from "preact/hooks";
import * as monaco from "monaco-editor";
import { useEditorStore } from "./stores/editorStore";
import { useGeneratorStore, debouncedGenerate, immediateGenerate } from "./stores/generatorStore";
import { MonacoEditor } from "./components/MonacoEditor";
import { LanguageSelector } from "./components/LanguageSelector";
import { ConfigPanel } from "./components/ConfigPanel";
import { ActionBar } from "./components/ActionBar";
import { SourceMapOverlay } from "./components/SourceMapOverlay";
import styles from "./components/App.module.css";
import { initBrgenLanguage } from "./brgen_language";

export function App() {
  const source = useEditorStore((s) => s.source);
  const language = useEditorStore((s) => s.language);
  const setSource = useEditorStore((s) => s.setSource);
  const setLanguage = useEditorStore((s) => s.setLanguage);
  const generating = useGeneratorStore((s) => s.generating);
  const result = useGeneratorStore((s) => s.result);
  const mappingInfo = useGeneratorStore((s) => s.mappingInfo);

  // Editor instance refs for source map overlay
  const sourceEditorRef = useRef<monaco.editor.IStandaloneCodeEditor | null>(null);
  const generatedEditorRef = useRef<monaco.editor.IStandaloneCodeEditor | null>(null);
  const overlayRef = useRef(new SourceMapOverlay());

  const handleSourceEditorRef = useCallback(
    (editor: monaco.editor.IStandaloneCodeEditor | null) => {
      sourceEditorRef.current = editor;
    },
    [],
  );

  const handleGeneratedEditorRef = useCallback(
    (editor: monaco.editor.IStandaloneCodeEditor | null) => {
      generatedEditorRef.current = editor;
    },
    [],
  );

  // Initialize brgen language mode and LSP once
  useEffect(() => {
    initBrgenLanguage();
  }, []);

  // Run initial generation on mount
  useEffect(() => {
    immediateGenerate();
  }, []);

  // Apply source map decorations when mappingInfo changes
  useEffect(() => {
    overlayRef.current.apply(
      sourceEditorRef.current,
      generatedEditorRef.current,
      mappingInfo,
    );
    return () => {
      overlayRef.current.clear();
    };
  }, [mappingInfo]);

  // Ctrl+S handler
  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      if (e.ctrlKey && e.key === "s") {
        e.preventDefault();
        immediateGenerate();
      }
    };
    window.addEventListener("keydown", handler);
    return () => window.removeEventListener("keydown", handler);
  }, []);

  const handleSourceChange = useCallback(
    (newSource: string) => {
      setSource(newSource);
      debouncedGenerate();
    },
    [setSource],
  );

  const handleLanguageChange = useCallback(
    (lang: string) => {
      setLanguage(lang as any);
      immediateGenerate();
    },
    [setLanguage],
  );

  const handleConfigChange = useCallback(() => {
    immediateGenerate();
  }, []);

  // Determine the Monaco language for the generated output
  const generatedLang = result?.monacoLang ?? "text/plain";
  const generatedCode = result?.code ?? "(generated code)";

  return (
    <div class={styles.app}>
      {/* Title bar */}
      <div class={styles.titleBar}>
        <span class={styles.titleText}>brgen</span>

        <LanguageSelector value={language} onChange={handleLanguageChange} />

        <ConfigPanel language={language} onConfigChange={handleConfigChange} />

        <div class={styles.titleBarSpacer} />

        {generating && (
          <span class={`${styles.statusIndicator} ${styles.statusGenerating}`}>
            generating...
          </span>
        )}

        <ActionBar />
      </div>

      {/* Editor panels */}
      <div class={styles.editorArea}>
        {/* Source editor */}
        <div class={styles.editorPanel}>
          <MonacoEditor
            value={source}
            language="brgen"
            onChange={handleSourceChange}
            theme="brgen-theme"
            semanticHighlighting
            editorRef={handleSourceEditorRef}
          />
        </div>

        {/* Generated output editor */}
        <div class={styles.editorPanel}>
          <MonacoEditor
            value={generatedCode}
            language={generatedLang}
            readOnly
            editorRef={handleGeneratedEditorRef}
          />
        </div>
      </div>
    </div>
  );
}
