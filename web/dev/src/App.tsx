import "destyle.css";
import { useEditorStore } from "./stores/editorStore";
import { useConfigStore } from "./stores/configStore";
import { useGeneratorStore, immediateGenerate } from "./stores/generatorStore";
import { languageRegistry, getLanguagesByCategory, LanguageCategory } from "./languages";

const categories = [
  LanguageCategory.ANALYSIS,
  LanguageCategory.GENERATOR,
  LanguageCategory.INTERMEDIATE,
  LanguageCategory.BM,
  LanguageCategory.EBM,
];
const byCategory = getLanguagesByCategory();

export function App() {
  const source = useEditorStore(s => s.source);
  const language = useEditorStore(s => s.language);
  const setLanguage = useEditorStore(s => s.setLanguage);
  const generating = useGeneratorStore(s => s.generating);
  const result = useGeneratorStore(s => s.result);

  return (
    <div style={{ padding: "20px", color: "white", backgroundColor: "#1e1e1e", minHeight: "100vh" }}>
      <h1>brgen Web Playground</h1>
      <p>Vite + Preact + Zustand pipeline working. Stores integrated.</p>

      <div style={{ marginTop: "10px" }}>
        <label style={{ color: "#aaa" }}>
          Language:{" "}
          <select
            value={language}
            onChange={e => {
              setLanguage(e.currentTarget.value as any);
              immediateGenerate();
            }}
            style={{ backgroundColor: "#333", color: "white", padding: "4px" }}
          >
            {categories.map(cat => {
              const langs = byCategory.get(cat) ?? [];
              if (langs.length === 0) return null;
              return (
                <optgroup key={cat} label={cat}>
                  {langs.map(l => (
                    <option key={l.id} value={l.id}>{l.displayName}</option>
                  ))}
                </optgroup>
              );
            })}
          </select>
        </label>
      </div>

      <p style={{ color: "#888", marginTop: "10px" }}>
        Source: {source.length} chars | Total languages: {languageRegistry.length}
        {generating && " | Generating..."}
      </p>

      {result && (
        <div style={{ marginTop: "10px" }}>
          <p style={{ color: result.isError ? "#f66" : "#6f6" }}>
            Result: {result.monacoLang} ({result.code.length} chars)
          </p>
        </div>
      )}

      <p style={{ color: "#888", marginTop: "10px" }}>
        Next: Preact UI components (Monaco editor panels, config panel, etc.)
      </p>
    </div>
  );
}
