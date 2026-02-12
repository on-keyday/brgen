import { useState, useEffect, useRef, useCallback, useMemo } from "preact/hooks";
import styles from "./App.module.css";

export interface FilePickerProps {
  /** Whether the picker is currently visible */
  open: boolean;
  /** Called when the picker should close (Escape, backdrop click, or selection) */
  onClose: () => void;
  /** Called when the user selects a file; receives the file path (e.g. "example/udp.bgn") */
  onSelect: (filePath: string) => void;
}

/** Cached file list so we only fetch index.txt once */
let cachedFiles: string[] | null = null;

async function fetchExampleFiles(): Promise<string[]> {
  if (cachedFiles) return cachedFiles;
  const res = await fetch("example/index.txt");
  if (!res.ok) {
    console.error("Failed to fetch example/index.txt:", res.status);
    return [];
  }
  const contentType = res.headers.get("content-type") ?? "";
  if (contentType.includes("text/html")) {
    // SPA fallback returned index.html instead of the real file
    console.error("example/index.txt returned HTML (likely SPA fallback). Check publicDir config.");
    return [];
  }
  const text = await res.text();
  cachedFiles = text
    .replace(/\r\n/g, "\n")
    .split("\n")
    .filter((line) => line !== "");
  return cachedFiles;
}

/**
 * Quick-open style file picker modal for loading example .bgn files.
 * Shows a search input and a filterable list of files.
 * Keyboard navigation: arrow keys, Enter to select, Escape to close.
 */
export function FilePicker({ open, onClose, onSelect }: FilePickerProps) {
  const [files, setFiles] = useState<string[]>([]);
  const [query, setQuery] = useState("");
  const [activeIndex, setActiveIndex] = useState(0);
  const inputRef = useRef<HTMLInputElement>(null);
  const listRef = useRef<HTMLDivElement>(null);

  // Fetch file list on first open
  useEffect(() => {
    if (!open) return;
    setQuery("");
    setActiveIndex(0);
    fetchExampleFiles().then(setFiles);
  }, [open]);

  // Focus input when opened
  useEffect(() => {
    if (open) {
      // Short delay so the modal is rendered before focusing
      requestAnimationFrame(() => inputRef.current?.focus());
    }
  }, [open]);

  const filtered = useMemo(() => {
    if (!query) return files;
    const lower = query.toLowerCase();
    return files.filter((f) => f.toLowerCase().includes(lower));
  }, [files, query]);

  // Clamp active index when filtered list changes
  useEffect(() => {
    if (activeIndex >= filtered.length) {
      setActiveIndex(Math.max(0, filtered.length - 1));
    }
  }, [filtered.length, activeIndex]);

  // Scroll active item into view
  useEffect(() => {
    if (!listRef.current) return;
    const activeEl = listRef.current.children[activeIndex] as HTMLElement | undefined;
    activeEl?.scrollIntoView({ block: "nearest" });
  }, [activeIndex]);

  const handleSelect = useCallback(
    (filePath: string) => {
      onSelect(filePath);
      onClose();
    },
    [onSelect, onClose],
  );

  const handleKeyDown = useCallback(
    (e: KeyboardEvent) => {
      switch (e.key) {
        case "ArrowDown":
          e.preventDefault();
          setActiveIndex((prev) => Math.min(prev + 1, filtered.length - 1));
          break;
        case "ArrowUp":
          e.preventDefault();
          setActiveIndex((prev) => Math.max(prev - 1, 0));
          break;
        case "Enter":
          e.preventDefault();
          if (filtered[activeIndex]) {
            handleSelect(filtered[activeIndex]);
          }
          break;
        case "Escape":
          e.preventDefault();
          onClose();
          break;
      }
    },
    [filtered, activeIndex, handleSelect, onClose],
  );

  if (!open) return null;

  // Extract just the filename from the full path for display
  const displayName = (path: string) => {
    const parts = path.split("/");
    return parts[parts.length - 1];
  };

  const displayDir = (path: string) => {
    const parts = path.split("/");
    if (parts.length <= 1) return "";
    return parts.slice(0, -1).join("/") + "/";
  };

  return (
    <div class={styles.filePickerOverlay} onClick={onClose}>
      <div
        class={styles.filePickerModal}
        onClick={(e) => e.stopPropagation()}
      >
        <input
          ref={inputRef}
          type="text"
          class={styles.filePickerInput}
          placeholder="Search example files..."
          value={query}
          onInput={(e) => {
            setQuery((e.target as HTMLInputElement).value);
            setActiveIndex(0);
          }}
          onKeyDown={handleKeyDown}
        />
        <div class={styles.filePickerList} ref={listRef}>
          {filtered.length === 0 && (
            <div class={styles.filePickerEmpty}>No matching files</div>
          )}
          {filtered.map((file, i) => (
            <div
              key={file}
              class={`${styles.filePickerItem} ${i === activeIndex ? styles.filePickerItemActive : ""}`}
              onClick={() => handleSelect(file)}
              onMouseEnter={() => setActiveIndex(i)}
            >
              <span class={styles.filePickerFileName}>{displayName(file)}</span>
              <span class={styles.filePickerFileDir}>{displayDir(file)}</span>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}
