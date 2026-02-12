import { useState } from "preact/hooks";
import { useConfigStore, ConfigEntry } from "../stores/configStore";
import styles from "./App.module.css";

export interface ConfigPanelProps {
  /** Currently selected language ID */
  language: string;
  /** Called after any config change so caller can trigger regeneration */
  onConfigChange?: () => void;
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

  const handleChange = (value: boolean | number | string) => {
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
            {k}
          </option>
        ))}
      </select>
      {entry && <ConfigInput entry={entry} onChange={handleChange} />}
    </div>
  );
}

interface ConfigInputProps {
  entry: ConfigEntry;
  onChange: (value: boolean | number | string) => void;
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
    default:
      return null;
  }
}
