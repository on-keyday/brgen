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