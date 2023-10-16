
import * as caller from "./src2json_caller.js";

caller.runSrc2JSON(`
format Exec:
    field :u64
`).then((result) => {
    console.log(result);
});
