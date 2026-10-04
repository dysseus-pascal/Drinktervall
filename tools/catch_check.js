// Kein catch ohne Log - in der Telefonseite.
//
//   node tools/catch_check.js [datei ...]      (ohne Angabe: src/pkjs/*.js)
//
// Ein leerer catch verschluckt einen Fehler beim Telefonspeicher oder beim
// Lesen, und hinterher laesst sich nicht mehr erklaeren, warum eine
// Einstellung oder ein Pin fehlte (Audit, Regel 10). Gleich gebaut wie in
// SupCycle. Jeder catch-Block muss darum ins Log
// melden: ueber meldeFehler(...) oder console.log(...).
//
// Exitcode 0 = jeder catch meldet.
'use strict';
const fs = require('fs');
const path = require('path');

const MELDET = /\bmeldeFehler\s*\(|\bconsole\.(log|warn|error)\s*\(/;

// Den Rumpf ab der oeffnenden Klammer bis zur passenden schliessenden -
// Zeichenketten und Kommentare zaehlen nicht mit.
function rumpf(text, auf) {
  let tiefe = 0;
  for (let i = auf; i < text.length; i++) {
    const c = text[i];
    if (c === '\'' || c === '"' || c === '`') {
      for (i++; i < text.length && text[i] !== c; i++) if (text[i] === '\\') i++;
      continue;
    }
    if (c === '/' && text[i + 1] === '/') { while (i < text.length && text[i] !== '\n') i++; continue; }
    if (c === '/' && text[i + 1] === '*') { i = text.indexOf('*/', i + 2) + 1; continue; }
    if (c === '{') tiefe++;
    if (c === '}' && --tiefe === 0) return text.slice(auf + 1, i);
  }
  return null;
}

function pruefe(datei) {
  const text = fs.readFileSync(datei, 'utf8');
  const muster = /\bcatch\s*\(\s*\w*\s*\)\s*\{/g;
  let fehler = 0, anzahl = 0, m;
  while ((m = muster.exec(text)) !== null) {
    anzahl++;
    const zeile = text.slice(0, m.index).split('\n').length;
    const r = rumpf(text, m.index + m[0].length - 1);
    const ok = r !== null && MELDET.test(r);
    if (!ok) {
      fehler++;
      console.log('  FEHLER ' + path.basename(datei) + ':' + zeile + ' catch ohne Log: ' +
                  JSON.stringify((r || '').trim().slice(0, 60)));
    }
  }
  console.log((fehler ? '  FEHLER ' : '  ok     ') + path.basename(datei) + ': ' + anzahl +
              ' catch, ' + (anzahl - fehler) + ' melden ins Log');
  return fehler;
}

const dateien = process.argv.length > 2 ? process.argv.slice(2)
  : fs.readdirSync(path.join(__dirname, '..', 'src', 'pkjs'))
      .filter((f) => f.endsWith('.js')).map((f) => path.join(__dirname, '..', 'src', 'pkjs', f));
const fehler = dateien.reduce((n, d) => n + pruefe(d), 0);
console.log('Fehler: ' + fehler);
process.exit(fehler ? 1 : 0);
