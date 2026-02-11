import { useState, useCallback } from "preact/hooks";
import { useEditorStore } from "../stores/editorStore";
import { useGeneratorStore } from "../stores/generatorStore";
import styles from "./App.module.css";

/**
 * Action buttons: copy generated code, copy sharing link, GitHub link, privacy notice.
 */
export function ActionBar() {
  return (
    <>
      <CopyCodeButton />
      <CopyLinkButton />
      <Separator />
      <a
        class={styles.linkButton}
        href="https://github.com/on-keyday/brgen"
        target="_blank"
        rel="noopener noreferrer"
      >
        github
      </a>
      <a
        class={styles.linkButton}
        href="./doc/"
        target="_blank"
        rel="noopener noreferrer"
      >
        docs
      </a>
      <PrivacyLink />
    </>
  );
}

function Separator() {
  return <div class={styles.separator} />;
}

function CopyCodeButton() {
  const [label, setLabel] = useState("copy code");
  const result = useGeneratorStore((s) => s.result);

  const handleClick = useCallback(async () => {
    const code = result?.code;
    if (!code) return;
    if (!navigator.clipboard) {
      setLabel("not supported");
      setTimeout(() => setLabel("copy code"), 1000);
      return;
    }
    try {
      await navigator.clipboard.writeText(code);
      setLabel("copied!");
    } catch {
      setLabel("failed");
    }
    setTimeout(() => setLabel("copy code"), 1000);
  }, [result]);

  return (
    <button class={styles.button} onClick={handleClick} title="Copy generated code">
      {label}
    </button>
  );
}

function CopyLinkButton() {
  const [label, setLabel] = useState("copy link");
  const getShareHash = useEditorStore((s) => s.getShareHash);

  const handleClick = useCallback(async () => {
    const hash = getShareHash();
    const link = `${location.origin}${location.pathname}${hash}`;
    if (!navigator.clipboard) {
      setLabel("not supported");
      setTimeout(() => setLabel("copy link"), 1000);
      return;
    }
    try {
      await navigator.clipboard.writeText(link);
      setLabel("copied!");
    } catch {
      setLabel("failed");
    }
    setTimeout(() => setLabel("copy link"), 1000);
  }, [getShareHash]);

  return (
    <button class={styles.button} onClick={handleClick} title="Copy sharing link">
      {label}
    </button>
  );
}

function PrivacyLink() {
  const handleClick = useCallback((e: Event) => {
    e.preventDefault();
    alert(
      "This site uses localStorage to save your source code and configuration. " +
      "These data are not sent to the server except explicitly notified. " +
      "You can delete these data using Dev Tools anytime.",
    );
  }, []);

  return (
    <a class={styles.linkButton} href="#" onClick={handleClick}>
      privacy
    </a>
  );
}
