// Eigenes Snooze-Symbol fuer die Aktionsleiste: drei Z, von unten links nach
// oben rechts kleiner werdend. 18x18, weiss auf durchsichtig, ohne Kanten-
// glaettung - so zeichnet die Uhr die Aktionsleiste ohnehin.
//
//   node mkzzz.js <ziel.png>
'use strict';
const fs = require('fs');
const zlib = require('zlib');

const W = 18, H = 18;
const px = new Uint8Array(W * H);          // 1 = weiss, 0 = durchsichtig

function set(x, y) {
  if (x < 0 || y < 0 || x >= W || y >= H) throw new Error('ausserhalb: ' + x + ',' + y);
  px[y * W + x] = 1;
}

// Ein Z der Kantenlaenge n mit Strichstaerke t, linke obere Ecke bei (ox, oy).
function zed(ox, oy, n, t) {
  for (let r = 0; r < t; r++) {
    for (let c = 0; c < n; c++) { set(ox + c, oy + r); set(ox + c, oy + n - 1 - r); }
  }
  for (let r = t; r < n - t; r++) {
    const c0 = n - 1 - r;
    for (let k = 0; k < t; k++) set(ox + Math.max(0, Math.min(n - 1, c0 - k)), oy + r);
  }
}

zed(0, 11, 7, 2);    // gross, unten links
zed(8, 5, 6, 2);     // mittel
zed(14, 0, 4, 1);    // klein, oben rechts

let art = '';
for (let y = 0; y < H; y++) {
  for (let x = 0; x < W; x++) art += px[y * W + x] ? '#' : '.';
  art += '\n';
}
console.log(art);

// PNG schreiben: 8 Bit RGBA, Filtertyp 0 je Zeile.
const raw = Buffer.alloc(H * (1 + W * 4));
for (let y = 0; y < H; y++) {
  const off = y * (1 + W * 4) + 1;
  for (let x = 0; x < W; x++) {
    const on = px[y * W + x] ? 255 : 0;
    raw[off + x * 4] = 255; raw[off + x * 4 + 1] = 255;
    raw[off + x * 4 + 2] = 255; raw[off + x * 4 + 3] = on;
  }
}
function crc32(buf) {
  let c, t = [];
  for (let n = 0; n < 256; n++) { c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; t[n] = c >>> 0; }
  let x = 0xffffffff;
  for (let i = 0; i < buf.length; i++) x = t[(x ^ buf[i]) & 0xff] ^ (x >>> 8);
  return (x ^ 0xffffffff) >>> 0;
}
function chunk(type, data) {
  const len = Buffer.alloc(4); len.writeUInt32BE(data.length, 0);
  const body = Buffer.concat([Buffer.from(type, 'ascii'), data]);
  const crc = Buffer.alloc(4); crc.writeUInt32BE(crc32(body), 0);
  return Buffer.concat([len, body, crc]);
}
const ihdr = Buffer.alloc(13);
ihdr.writeUInt32BE(W, 0); ihdr.writeUInt32BE(H, 4);
ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
const out = Buffer.concat([
  Buffer.from('89504e470d0a1a0a', 'hex'),
  chunk('IHDR', ihdr),
  chunk('IDAT', zlib.deflateSync(raw, { level: 9 })),
  chunk('IEND', Buffer.alloc(0)),
]);
fs.writeFileSync(process.argv[2], out);
console.log(process.argv[2] + ' -> ' + out.length + ' Bytes');
