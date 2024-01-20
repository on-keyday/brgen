

export type CSS = Partial<CSSStyleDeclaration>;

export const setStyle = (e :HTMLElement,css ?:CSS) => {
    e.style.position = "absolute";
    e.style.display = "block";
    if(css){
        Object.assign(e.style,css);
    }
}

export const makeListBox = (id :string,items :string[],value :string,onchange :()=>void,css?:CSS) => {
    const select = document.createElement("select");
    select.id = id;
    items.forEach((item) => {
        const option = document.createElement("option");
        option.value = item;
        option.innerText = item;
        select.appendChild(option);
    });
    select.value = value;
    setStyle(select,css);
    select.onchange = onchange;
    return select;
}

export interface InputListElement {
    name? :string;
    readonly type :'checkbox' | 'number' | 'text';
    value :boolean | number | string;
}


export const makeInputList = (id :string,items :Array<InputListElement>|Map<string,InputListElement>,value :string,onchange :(e :InputListElement)=>void,css?:CSS) => {
    const div = document.createElement("div");
    div.id = id + "-container";
    div.style.display = "flex";
    const selectContainer = document.createElement("div");
    selectContainer.id = "selectContainer-" + id;
    const select = document.createElement("select");
    select.id = id + "-select";    
    const inputContainer = document.createElement("div");
    inputContainer.id = "inputContainer-" + id;
    items.forEach((item,key) => {
        if(typeof key === 'string'){ 
            item.name = key;
        }
        if(item.name === undefined){
            throw new Error("InputListElement must have name property");
        }
        const option = document.createElement("option");
        option.value = item.name;
        option.innerText = item.name;
        select.appendChild(option);
    });
    const setCheckBoxStyle = (checkbox :HTMLInputElement) => {
        if(!checkbox.checked) {
            checkbox.style.backgroundColor = "red";
            // check mark design in css
            checkbox.style.transform = 'rotate(-45deg)';
            checkbox.style.left = `${select.clientWidth/2}px}`;
            checkbox.style.width = `${select.clientWidth/3}px`;
            checkbox.style.height = `${select.clientHeight/3}px`;
        }
        else{
            checkbox.style.backgroundColor = "green";
            // clear check mark design in css
            checkbox.style.borderBottom = "";
            checkbox.style.borderLeft = "";
            checkbox.style.transform = '';
            checkbox.style.top = '';
            checkbox.style.left = '';
            checkbox.style.left = `${select.clientWidth/2}px}`;
            checkbox.style.width = `${select.clientWidth/3}px`;
            checkbox.style.height = `${select.clientHeight/3}px`;
        }
        inputContainer.innerHTML = ''; // Clear previous input
        const textElement = document.createElement('label');
        textElement.innerText = checkbox.checked ? 'ON' : 'OFF';
        textElement.appendChild(checkbox);
        inputContainer.appendChild(textElement);
    }
    const setInput = () => {
        inputContainer.innerHTML = ''; // Clear previous input
        const found = (() => {
            if(typeof items === 'object' && items instanceof Map) {
                return items.get(select.value);
            }
            else {
                return items.find((item) => item.name === select.value);
            }
        })();
        if(found){
            const inputElement = document.createElement('input');
            inputElement.type = found.type;
            inputElement.value = found.value.toString();
            inputElement.style.width = `${select.clientWidth}px`
            inputElement.style.height = `${select.clientHeight}px`
            inputElement.style.border = "solid 1px black";
                     
            inputElement.onchange = () => {  
                switch(found.type){
                    case 'checkbox':
                        found.value = inputElement.checked;
                        setCheckBoxStyle(inputElement);
                        break;
                    case 'number':
                        found.value = Number(inputElement.value);
                        break;
                    case 'text':
                        found.value = inputElement.value;
                        break;
                }
                onchange(found);
                switch(found.type){
                    case 'checkbox':
                        inputElement.checked = found.value as boolean;
                        setCheckBoxStyle(inputElement);
                        break;
                    case 'number':
                        inputElement.value = found.value.toString();
                        break;
                    case 'text':
                        inputElement.value = found.value.toString();
                        break;
                }
            }
            if(found.type === 'checkbox'){
                inputElement.checked = found.value as boolean;
                setCheckBoxStyle(inputElement);
            } 
            else{ 
                inputContainer.appendChild(inputElement);
            }
        }
    }
    select.value = value;
    const rootDiv = document.createElement("div");
    rootDiv.id = id;
    rootDiv.appendChild(div);
    setStyle(rootDiv,css);
    select.style.border = rootDiv.style.border;
    rootDiv.style.border = "";
    select.onchange = setInput;
    selectContainer.appendChild(select);
    div.appendChild(selectContainer);
    div.appendChild(inputContainer);
    setInput();
    // inspect when rootDiv is inserted to Document
    const observer = new MutationObserver((mutations) => {
        mutations.forEach((mutation) => {
            if(mutation.type === 'childList'){
                mutation.addedNodes.forEach((node) => {
                    if(node === rootDiv){
                        setInput();
                    }
                });
            }
        });
    });
    observer.observe(document.body,{childList:true,subtree:true});
    globalThis.addEventListener('resize',setInput);
    return rootDiv;
}

export const makeButton = (id :string,text :string,onclick:()=>void,css?:CSS) => {
    const button = document.createElement("button");
    button.id = id;
    button.innerText = text;
    button.onclick = onclick;
    setStyle(button,css);
    return button;
}

export const makeLink = (id :string,text :string,href :string,onclick:(e:MouseEvent)=>void,css?:CSS) => {
    const link = document.createElement("a");
    link.id = id;
    link.innerText = text;
    link.href = href;
    link.onclick = onclick;
    setStyle(link,css);
    return link;
}



