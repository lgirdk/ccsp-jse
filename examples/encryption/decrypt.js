var enc = readFileAsBuffer("encrypted.bin");

// Bad secret key
var key = "01234567765432101234567887654321";

// Decrypt with AES 256 bit CBC.
// The first 16 bytes are the IV
var str = decryptToString(enc.subarray(16), key, 8, enc.subarray(0, 16));

print("Unencrypted string: " + str + "\n");
