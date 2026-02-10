import "destyle.css";
import { WorkerFactory } from "./s2j/worker_factory";
import { fixedWorkerMap } from "./s2j/workers";
import { LanguageList, Language } from "./s2j/msg";
import { UpdateTracer } from "./s2j/update";

// Verify worker layer imports resolve correctly at build time
const _workerFactory = new WorkerFactory();
_workerFactory.addWorker(fixedWorkerMap);
const _updateTracer = new UpdateTracer();
const _languages = LanguageList;

export function App() {
  return (
    <div style={{ padding: "20px", color: "white", backgroundColor: "#1e1e1e", minHeight: "100vh" }}>
      <h1>brgen Web Playground</h1>
      <p>Vite + Preact pipeline working. Worker layer integrated.</p>
      <p style={{ color: "#888", marginTop: "10px" }}>
        Available languages: {_languages.length}
      </p>
      <p style={{ color: "#888", marginTop: "10px" }}>
        Next: adapter layer, stores, and UI components.
      </p>
    </div>
  );
}
