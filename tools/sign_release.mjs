import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';

const [tag, folder, keyFile] = process.argv.slice(2);
if (!/^v\d+\.\d+\.\d+(?:-[a-zA-Z0-9.-]+)?$/.test(tag ?? '') || !folder || !keyFile) throw new Error('Expected release tag, output folder and private key file');
const names = ['MonchiLauncher.exe', 'Monchi.dll', 'MonchiFlarial.dll', `MonchiLauncher-Bugfixes-${tag.slice(1)}.exe`];
let manifest = `version ${tag}\n`;
for (const name of names) {
    const file = path.join(folder, name);
    if (!fs.existsSync(file)) continue;
    manifest += `${crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex')}  ${name}\n`;
}
if (!fs.existsSync(path.join(folder, names[0]))) throw new Error('Missing launcher');
const signature = crypto.sign('sha256', Buffer.from(manifest), {key: fs.readFileSync(keyFile), dsaEncoding: 'ieee-p1363'});
fs.writeFileSync(path.join(folder, 'checksums.txt'), manifest);
fs.writeFileSync(path.join(folder, 'checksums.sig'), signature);
console.log('Release checksum manifest signed');
