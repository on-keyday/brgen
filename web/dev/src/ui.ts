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

export interface CheckBoxListElement {
    name :string;
    readonly type :'checkbox' | 'number' | 'text';
    value :boolean | number | string;
}


export const makeCheckBoxList = (id :string,items :CheckBoxListElement[],value :string,onchange :(e :CheckBoxListElement)=>void,css?:CSS) => {
    const div = document.createElement("div");
    div.id = id + "-container";
    div.style.display = "flex";
    const selectContainer = document.createElement("div");
    selectContainer.id = "selectContainer-" + id;
    const select = document.createElement("select");
    select.id = id + "-select";    
    const inputContainer = document.createElement("div");
    inputContainer.id = "inputContainer-" + id;
    items.forEach((item) => {
        const option = document.createElement("option");
        option.value = item.name;
        option.innerText = item.name;
        select.appendChild(option);
    });
    const setInput = () => {
        inputContainer.innerHTML = ''; // Clear previous input
        const found = items.find((item) => item.name === select.value);
        if(found){
            const inputElement = document.createElement('input');
            inputElement.type = found.type;
            inputElement.value = found.value.toString();
            inputElement.onchange = () => {
                switch(found.type){
                    case 'checkbox':
                        found.value = inputElement.checked;
                        break;
                    case 'number':
                        found.value = Number(inputElement.value);
                        break;
                    case 'text':
                        found.value = inputElement.value;
                        break;
                }
                onchange(found);
            }
            inputContainer.appendChild(inputElement);
        }
    }
    select.value = value;
    setInput();
    select.onchange = setInput;
    selectContainer.appendChild(select);
    div.appendChild(selectContainer);
    div.appendChild(inputContainer);
    const rootDiv = document.createElement("div");
    rootDiv.id = id;
    rootDiv.appendChild(div);
    setStyle(rootDiv,css);
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


export const makeCheckBox = (id :string,text :string) => {
    const label = document.createElement("label");
    label.innerText = text;
    const checkbox = document.createElement("input");
    checkbox.type = "checkbox";
    checkbox.id = id;
    label.appendChild(checkbox);
    setStyle(label);
    return label;
}

export interface LanguageSpecific {
    enable(): void;
    disable():void;   
}
