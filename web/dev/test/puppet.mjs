export {}
import * as puppet from "puppeteer";

import { env, cwd } from "process";

import { readdir } from "fs/promises";


const wait = (ms) => new Promise((resolve) => setTimeout(resolve, ms));


const headless = env.TEST_ENV === "github";



console.log("starting puppeteer...");
console.log("headless:", headless);

const browser = await puppet.launch({ headless: headless });

const context = await browser.defaultBrowserContext();
await context.overridePermissions("http://localhost:8080", ["clipboard-write","clipboard-sanitized-write"]);
const page = await browser.newPage();
await page.setJavaScriptEnabled(true);
await page.goto("http://localhost:8080");
await page.waitForSelector("#language-select");

console.log("page loaded");


const resolver = {
    resolve: undefined,
}
const exported = name => async () => {
    console.log(name, "called");
    if(resolver.resolve !== undefined) {
        resolver.resolve();
        resolver.resolve = undefined;
    }
}
await page.exposeFunction('onPuppeteerCodeGenerated', exported("onPuppeteerCodeGenerated"));
await page.exposeFunction('onPuppeteerSampleLoaded', exported("onPuppeteerSampleLoaded"));
await page.exposeFunction('onPuppeteerCandidatesLoaded', exported("onPuppeteerCandidatesLoaded"));


const waitForEvent = async () => {
    return Promise.race([
        new Promise((resolve) => {
            resolver.resolve = resolve;
        }),
        wait(2000)
    ]);
}


let prevText = "";
let prevLang = "";
let handlingFile="(default)"
const clickSelect = async (lang) => {
    const generated = waitForEvent();
    await page.select("#language-select",lang);  
    await generated;
    await page.click("button#copy-button");
    const text = await page.evaluate(() => navigator.clipboard.readText());
    if(text === prevText) {
        console.warn("text is the same","current:", lang ,"prev:",prevLang,"file:",handlingFile);
    }    
    prevLang = lang;
    prevText = text;
}

const setJS = async () => {
    await page.select("#input-list-select","javascript");
    await page.click("div#title_bar div#inputContainer-input-list input[type='checkbox']");
}

const selectLanguages = async () => {
    await clickSelect("tokens");
    await clickSelect("json ast");
    await clickSelect("json ast (debug)");
    await clickSelect("cpp");
    await clickSelect("go");
    await clickSelect("c");
    await clickSelect("rust");
    await clickSelect("typescript");
    await setJS();
    await clickSelect("typescript");
    await setJS();
}

await selectLanguages();

let PWD = cwd();
console.log("PWD:", PWD);
if(!PWD.endsWith("/")) {
    PWD += "/";
}
const examples = await readdir(PWD + "../../example",{recursive:true}).then(x => x.map((e) => e.replace("\\","/")));
console.log("examples:", examples);

const writeCommandPalette = async (fileName) => {
    await page.click("#container1 .monaco-editor");
    await page.keyboard.press("F1");
    const waitForCandidates = waitForEvent();
    await page.keyboard.type("Load Example File\n");
    await waitForCandidates;
    const waitForLoaded = waitForEvent();
    await page.keyboard.type("example/"+fileName + "\n");
    await waitForLoaded;
}
for(let example of examples) {
    if(!example.endsWith(".bgn")) {
        continue;
    }
    console.log("file:", example)
    await writeCommandPalette(example);
    handlingFile = example;
    await selectLanguages();
}


await browser.close();

console.log("done");

