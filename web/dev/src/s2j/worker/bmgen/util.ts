export const base64ToUint8Array = (base64: string) => {
    const base64Characters =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
    //const base64URLCharacters = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_';
  
    let cleanedBase64 = base64.replace(/-/g, "+").replace(/_/g, "/").trim();
    const padding = (4 - (cleanedBase64.length % 4)) % 4;
    cleanedBase64 += "=".repeat(padding);
  
    const rawLength = cleanedBase64.length;
    const decodedLength = (rawLength * 3) / 4 - padding;
  
    const uint8Array = new Uint8Array(decodedLength);
  
    let byteIndex = 0;
    for (let i = 0; i < rawLength; i += 4) {
      const encoded1 = base64Characters.indexOf(cleanedBase64[i]);
      const encoded2 = base64Characters.indexOf(cleanedBase64[i + 1]);
      const encoded3 = base64Characters.indexOf(cleanedBase64[i + 2]);
      const encoded4 = base64Characters.indexOf(cleanedBase64[i + 3]);

      if(encoded1 < 0 || encoded2 < 0 || encoded3 < 0 || encoded4 < 0) {
        return new Error("invalid base64 string");
      }
  
      const decoded1 = (encoded1 << 2) | (encoded2 >> 4);
      const decoded2 = ((encoded2 & 15) << 4) | (encoded3 >> 2);
      const decoded3 = ((encoded3 & 3) << 6) | encoded4;
  
      uint8Array[byteIndex++] = decoded1;
      if (encoded3 !== 64) uint8Array[byteIndex++] = decoded2;
      if (encoded4 !== 64) uint8Array[byteIndex++] = decoded3;
    }
  
    return uint8Array;
  };
  