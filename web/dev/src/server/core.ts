
import { updateGenerated } from "../s2j/generator.js";
import { Hono } from 'hono'
import { RequestLanguage } from "../s2j/msg.js";

const app = new Hono()

interface GenerateRequest {
    source_code :string;
    lang :string;
    options :{ [key: string]: string|boolean }
}

function isGenerateRequest(x :any): x is GenerateRequest {
    return typeof x?.source_code === "string" &&
         typeof x?.lang === "string" &&
        typeof x?.options === "object";
}

app.post('/generate', async(c) => {
  const body = await c.req.json()
  if(!isGenerateRequest(body)){
    return c.json({"error": "invalid json"},400)
  }
  let result :string | null = null
  try {
    await updateGenerated({
        getValue: () => {
            return body.source_code
        },
        setDefault: ()=> {},
        setGenerated: (s, lang) => {
            result = s;
        },
        getLanguageConfig: (lang,key) => {
            return body.options[key]
        },
        mappingCode: () => {}
    },body.lang as RequestLanguage)
  }catch(e :any) {
    return c.json({"error": e.toString()},500)
  }
  return c.json({"result": result})
})

export default app
