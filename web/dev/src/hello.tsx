

import * as React from 'react';
import {createRoot} from 'react-dom/client'
import { Editor } from '@monaco-editor/react';



const App = () => {
    return (
        <Editor defaultValue='(generated code)'/>
    );
}
const dummy =document.getElementById('dummy')
if (!dummy) throw new Error('dummy not found')

const root = createRoot(dummy);
const app = <App/>;
root.render(app);
console.log(app);
console.log(Editor);

