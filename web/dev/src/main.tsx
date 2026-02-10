import { render } from "preact";
import { App } from "./App";

// Remove the loading animation
const ball = document.getElementById("ball");
if (ball) ball.remove();
const ballStyle = document.getElementById("ball-bound");
if (ballStyle) ballStyle.remove();

const root = document.getElementById("app");
if (root) {
  render(<App />, root);
}
