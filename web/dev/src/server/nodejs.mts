
import "./worker_pollyfill.js"
import { serve } from '@hono/node-server'
import app from './core.js'

console.log("Server started at 8081")
serve({
    "fetch": app.fetch,
    "port": 8081
})
