
export const save =async (baseUrl :string,sourceCode :string,config :any) => {
    const data = JSON.stringify({
        sourceCode,
        config
    });
    const s= (new Blob([data], {type: 'application/json'})).stream()
    return await fetch(`${baseUrl}/save`,{
        method: 'POST',
        body: s,
        headers: {
            'Content-Type': 'application/json'
        }
    }).then(response => {
        if (!response.ok) {
            throw new Error('not saved correctly');
        }
        return;
    })
}

export const load = async (baseUrl :string) => {
    return await fetch(`${baseUrl}/load`).then(response => {
        if (!response.ok) {
            throw new Error('not loaded correctly');
        }
        return response.json() as Promise<{sourceCode: string, config: any}>;
    })
}
