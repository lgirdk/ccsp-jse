var str = "To be or not to be that is the question! Whether it be nobler in the mind to suffer the slings and arrows of outrageous fortune...";

print("Unencrypted string: " + str + "\n");

// Create a random initialization vector
var ivraw  = [];
for (var i = 0; i < 16; i++) {
    // Random number between 0 and 255
    ivraw.push(Math.floor(Math.random() * 256));
}

var iv = new Uint8Array(ivraw);

// Bad secret key
var key = "01234567765432101234567887654321";

// Encrypt with AES 256 bit CBC
var enc = encrypt(str, key, 8, iv);

// Prepend the IV on the data
var ivenc = new Uint8Array(iv.length + enc.length);
ivenc.set(iv, 0);
ivenc.set(enc, iv.length);

// Write the file
writeAsFile("encrypted.bin", ivenc, true);
