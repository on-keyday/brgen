

import * as React from 'react';
import {createRoot} from 'react-dom/client'


const App = () => {
    return (
        <div>
            <h1>Hello World!</h1>
        </div>
    );
}
const dummy =document.getElementById('dummy')
if (!dummy) throw new Error('dummy not found')

const root = createRoot(dummy);
root.render(<App/>);
