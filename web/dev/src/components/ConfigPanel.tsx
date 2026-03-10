import { useState } from "preact/hooks";
import { useConfigStore, ConfigEntry, FileValue, ConfigValue } from "../stores/configStore";
import styles from "./App.module.css";

export interface ConfigPanelProps {
  /** Currently selected language ID */
  language: string;
  /** Called after any config change so caller can trigger regeneration */
  onConfigChange?: () => void;
}

const adjustDisplayName = (x: string) => {
  return x.replaceAll("-", " ").replaceAll("_", " ")
}

/**
 * Per-language configuration controls.
 * Shows a dropdown of config keys for the selected language,
 * with the appropriate input widget (checkbox, text, number) for each.
 */
export function ConfigPanel({ language, onConfigChange }: ConfigPanelProps) {
  const config = useConfigStore((s) => s.config);
  const setConfig = useConfigStore((s) => s.setConfig);
  const save = useConfigStore((s) => s.save);

  const langConfig = config[language];
  if (!langConfig) return null;

  const keys = Object.keys(langConfig);
  if (keys.length === 0) return null;

  const [selectedKey, setSelectedKey] = useState(keys[0]);
  const entry: ConfigEntry | undefined = langConfig[selectedKey];

  const handleChange = (value: ConfigValue) => {
    setConfig(language as any, selectedKey, value);
    save();
    onConfigChange?.();
  };

  return (
    <div class={styles.configPanel}>
      <select
        class={styles.select}
        value={selectedKey}
        onChange={(e) => setSelectedKey(e.currentTarget.value)}
        title="Configuration option"
      >
        {keys.map((k) => (
          <option key={k} value={k}>
            {adjustDisplayName(k)}
          </option>
        ))}
      </select>
      {entry && <ConfigInput entry={entry} onChange={handleChange} />}
    </div>
  );
}

interface ConfigInputProps {
  entry: ConfigEntry;
  onChange: (value: boolean | number | string | FileValue) => void;
}

function ConfigInput({ entry, onChange }: ConfigInputProps) {
  switch (entry.type) {
    case "checkbox":
      return (
        <label
          class={`${styles.configLabel} ${entry.value ? styles.configLabelOn : styles.configLabelOff}`}
          style={{ cursor: "pointer" }}
        >
          <input
            type="checkbox"
            class={styles.configCheckbox}
            checked={entry.value as boolean}
            onChange={(e) => onChange(e.currentTarget.checked)}
          />
          {" "}
          {entry.value ? "ON" : "OFF"}
        </label>
      );
    case "number":
      return (
        <input
          type="number"
          class={styles.configInput}
          value={entry.value as number}
          onChange={(e) => onChange(Number(e.currentTarget.value))}
        />
      );
    case "text":
      return (
        <input
          type="text"
          class={styles.configInput}
          value={entry.value as string}
          onChange={(e) => onChange(e.currentTarget.value)}
          placeholder={entry.name}
        />
      );
    case "choice":
      return (<select
        class={styles.select}
        value={entry.value as string}
        onChange={(e) => onChange(e.currentTarget.value)}
        title={entry.name}
      >
        {entry.candidates?.map((l) => (
          <option key={l} value={l}>
            {l}
          </option>
        ))}
      </select>
      )
    case "file":
      const fileData = entry.value as { fileName: string; data: ArrayBuffer } | null;
      const hasFile = !!(fileData && fileData.fileName);

      return (
        <div
          className={styles.configInput}
          style={{
            position: "relative",
            display: "flex",
            alignItems: "center",
            justifyContent: "space-between",
            paddingRight: "8px" // ボタン用の余白
          }}
        >
          {/* 1. 表示テキスト */}
          <span style={{
            overflow: "hidden",
            textOverflow: "ellipsis",
            "white-space": "nowrap",
            flex: 1,
            color: hasFile ? "inherit" : "#666" // 未選択時は薄く
          }}>
            {hasFile ? fileData.fileName : "(select file)"}
          </span>

          {/* 2. 解除ボタン (ファイルがある時のみ表示) */}
          {hasFile && (
            <button
              type="button"
              onClick={(e) => {
                e.stopPropagation(); // 親のクリックイベント（ファイル選択）を発火させない
                onChange({ fileName: "", data: null });     // 値をクリア
              }}
              style={{
                background: "none",
                border: "none",
                color: "#ff6b6b",
                cursor: "pointer",
                fontSize: "16px",
                lineHeight: 1,
                zIndex: 2, // inputより上に配置
                padding: "0 4px"
              }}
            >
              ×
            </button>
          )}

          {/* 3. 本物の input (透明) */}
          <input
            type="file"
            // 値をクリアした時に同じファイルを再度選択できるように reset するための hack
            value=""
            style={{
              position: "absolute",
              top: 0,
              left: 0,
              width: "100%",
              height: "100%",
              opacity: 0,
              cursor: "pointer",
              zIndex: 1
            }}
            onChange={(e) => {
              const file = e.currentTarget.files?.[0];
              if (file) {
                const reader = new FileReader();
                reader.onload = () => {
                  const data = reader.result as ArrayBuffer;
                  onChange({ fileName: file.name, data });
                };
                reader.readAsArrayBuffer(file);
              }
            }}
          />
        </div>
      );
    default:
      return null;
  }
}
