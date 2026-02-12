import { languageRegistry, getLanguagesByCategory, LanguageCategory, LanguageMeta } from "../languages";
import styles from "./App.module.css";

const categories: LanguageCategory[] = [
  LanguageCategory.ANALYSIS,
  LanguageCategory.GENERATOR,
  LanguageCategory.INTERMEDIATE,
  LanguageCategory.BM,
  LanguageCategory.EBM,
];

const byCategory = getLanguagesByCategory();

export interface LanguageSelectorProps {
  value: string;
  onChange: (lang: string) => void;
}

/**
 * Grouped language dropdown with optgroup categories.
 */
export function LanguageSelector({ value, onChange }: LanguageSelectorProps) {
  return (
    <select
      class={styles.select}
      value={value}
      onChange={(e) => onChange(e.currentTarget.value)}
      title="Target language"
    >
      {categories.map((cat) => {
        const langs: LanguageMeta[] = byCategory.get(cat) ?? [];
        if (langs.length === 0) return null;
        return (
          <optgroup key={cat} label={cat}>
            {langs.map((l) => (
              <option key={l.id} value={l.id}>
                {l.displayName}
              </option>
            ))}
          </optgroup>
        );
      })}
    </select>
  );
}
