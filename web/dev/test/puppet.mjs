export {}
import * as puppet from "puppeteer";

import { env } from "process";

const headless = env.TEST_ENV === "github";

const browser = await puppet.launch({ headless: headless });

const page = await browser.newPage();

await page.goto("http://localhost:8080");

