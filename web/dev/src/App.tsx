import "destyle.css";
import { getGeneratorService } from "./generator_service";
import { languageRegistry, getLanguagesByCategory, LanguageCategory } from "./languages";

// Verify Phase 2 modules compile and initialize correctly
const _service = getGeneratorService();
const _registry = languageRegistry;
const _byCategory = getLanguagesByCategory();

export function App() {
  const categories = [
    LanguageCategory.ANALYSIS,
    LanguageCategory.GENERATOR,
    LanguageCategory.INTERMEDIATE,
    LanguageCategory.BM,
    LanguageCategory.EBM,
  ];
  return (
    <div style={{ padding: "20px", color: "white", backgroundColor: "#1e1e1e", minHeight: "100vh" }}>
      <h1>brgen Web Playground</h1>
      <p>Vite + Preact pipeline working. Adapter layer integrated.</p>
      <p style={{ color: "#888", marginTop: "10px" }}>
        Total languages: {_registry.length}
      </p>
      <ul style={{ color: "#aaa", marginTop: "10px", listStyle: "none" }}>
        {categories.map(cat => {
          const langs = _byCategory.get(cat) ?? [];
          return (
            <li key={cat}>
              {cat}: {langs.map(l => l.displayName).join(", ")}
            </li>
          );
        })}
      </ul>
      <p style={{ color: "#888", marginTop: "10px" }}>
        Next: Zustand stores and UI components.
      </p>
    </div>
  );
}
