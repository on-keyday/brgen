export {};
import * as puppet from "puppeteer";

import { env, cwd } from "process";

import { readdir } from "fs/promises";

import { LanguageList } from "../out/s2j/msg.js";
import { BM_LANGUAGES } from "../out/lib/bmgen/bm_caller.js";
import { EBM_LANGUAGES } from "../out/lib/bmgen/ebm_caller.js";

const selectTarget = LanguageList.concat(BM_LANGUAGES).concat(EBM_LANGUAGES);

const wait = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

const headless = env.TEST_ENV === "github";

console.log("starting puppeteer...");
console.log("headless:", headless);

const browser = await puppet.launch({ headless: headless });

const context = await browser.defaultBrowserContext();
await context.overridePermissions("http://localhost:8080", [
  "clipboard-write",
  "clipboard-sanitized-write",
]);
const page = await browser.newPage();
await page.setJavaScriptEnabled(true);
await page.goto("http://localhost:8080");
await page.waitForSelector("#language-select");

console.log("page loaded");

const resolver = {
  resolve: undefined,
};
const exported =
  (name) =>
  async (...args) => {
    console.log(name, "called", " args:", args);
    if (resolver.resolve !== undefined) {
      resolver.resolve();
      resolver.resolve = undefined;
    }
  };
await page.exposeFunction(
  "onPuppeteerCodeGenerated",
  exported("onPuppeteerCodeGenerated")
);
await page.exposeFunction(
  "onPuppeteerSampleLoaded",
  exported("onPuppeteerSampleLoaded")
);
await page.exposeFunction(
  "onPuppeteerCandidatesLoaded",
  exported("onPuppeteerCandidatesLoaded")
);

const waitForEvent = async () => {
  return Promise.race([
    new Promise((resolve) => {
      resolver.resolve = resolve;
    }),
  ]);
};

let prevText = "";
let prevLang = "";
let handlingFile = "(default)";
const clickSelect = async (lang) => {
  const generated = waitForEvent();
  await page.select("#language-select", lang);
  await generated;
  await page.click("button#copy-button");
  const text = await page.evaluate(() => navigator.clipboard.readText());
  if (text === prevText) {
    console.warn(
      "text is the same",
      "current:",
      lang,
      "prev:",
      prevLang,
      "file:",
      handlingFile
    );
  }
  prevLang = lang;
  prevText = text;
};

const setJS = async () => {
  await page.select("#input-list-select", "javascript");
  await page.click(
    "div#title_bar div#inputContainer-input-list input[type='checkbox']"
  );
};

const selectLanguages = async () => {
  for (let lang of selectTarget) {
    if (lang == "json ast (debug)") {
      continue; // skip
    }
    console.log("selecting language:", lang);
    await clickSelect(lang);
    if (lang === "typescript") {
      await setJS();
      await clickSelect(lang);
      await setJS(); // reset
    }
  }
};

await selectLanguages();

let PWD = cwd();
console.log("PWD:", PWD);
if (!PWD.endsWith("/")) {
  PWD += "/";
}
const examples = await readdir(PWD + "../../example", { recursive: true })
  .then((x) => x.map((e) => e.replace(/\\/g, "/")))
  .then((x) => x.filter((e, i) => i < 20)); // limit to 20 files for testing
console.log("examples:", examples);

const writeCommandPalette = async (fileName) => {
  await page.click("#container1 .monaco-editor");
  await page.keyboard.press("F1");
  const waitForCandidates = waitForEvent();
  await page.keyboard.type("Load Example File\n");
  await waitForCandidates;
  const waitForLoaded = waitForEvent();
  await page.keyboard.type("example/" + fileName + "\n");
  await waitForLoaded;
};
for (let example of examples) {
  if (!example.endsWith(".bgn")) {
    continue;
  }
  console.log("file:", example);
  await writeCommandPalette(example);
  handlingFile = example;
  await selectLanguages();
}

await browser.close();

console.log("done");
