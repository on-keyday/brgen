// polyfill

export const base64ToUint8Array = (base64: string) => {
   const base64Characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
   //const base64Characters = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_';

  let cleanedBase64 = base64.replace(/-/g, "+").replace(/_/g, "/");
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

    if (encoded1 === -1 || encoded2 === -1) {
      throw new Error("Invalid base64 string");
    }

    const decoded1 = (encoded1 << 2) | (encoded2 >> 4);
    const decoded2 = ((encoded2 & 15) << 4) | (encoded3 >> 2);
    const decoded3 = ((encoded3 & 3) << 6) | encoded4;

    uint8Array[byteIndex++] = decoded1;
    if (encoded3 !== -1) uint8Array[byteIndex++] = decoded2;
    if (encoded4 !== -1) uint8Array[byteIndex++] = decoded3;
  }

  return uint8Array;
};

export const Uint8ArrayToBase64 = (uint8Array: Uint8Array) => {
  //const base64Characters ="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const base64Characters = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_';

  let base64 = "";
  const { length } = uint8Array;

  for (let i = 0; i < length; i += 3) {
    const byte1 = uint8Array[i];
    const byte2 = uint8Array[i + 1];
    const byte3 = uint8Array[i + 2];

    const encoded1 = byte1 >> 2;
    const encoded2 = ((byte1 & 3) << 4) | (byte2 >> 4);
    const encoded3 = ((byte2 & 15) << 2) | (byte3 >> 6);
    const encoded4 = byte3 & 63;

    base64 += base64Characters[encoded1] + base64Characters[encoded2];
    base64 += byte2 !== undefined ? base64Characters[encoded3] : "";
    base64 += byte3 !== undefined ? base64Characters[encoded4] : "";
  }

  return base64;
};