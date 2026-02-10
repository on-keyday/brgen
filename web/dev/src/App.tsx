import "destyle.css";

export function App() {
  return (
    <div style={{ padding: "20px", color: "white", backgroundColor: "#1e1e1e", minHeight: "100vh" }}>
      <h1>brgen Web Playground</h1>
      <p>Vite + Preact pipeline working. Phase 0 complete.</p>
      <p style={{ color: "#888", marginTop: "10px" }}>
        Next: migrate worker layer, adapter layer, stores, and UI components.
      </p>
    </div>
  );
}
