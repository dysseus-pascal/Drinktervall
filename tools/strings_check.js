// Prueft src/c/strings_table.h.
//
//   node tools/strings_check.js [src/c/strings_table.h] [src/c]
//
// Was der Compiler schon prueft, steht hier nicht: eine Zeile mit zu wenigen
// Spalten ist ein Praeprozessorfehler ("macro STR requires 4 arguments"), und
// ein unbekannter Schluessel im Code ist ein Uebersetzungsfehler. Dieses
// Werkzeug faengt das, was der Compiler NICHT sieht:
//
//   1. leere Spalte (die englische ist der Rueckfall und darf nie leer sein)
//   2. doppelte Schluessel
//   3. Ueberschreitung des Zielpuffers in BYTES (Umlaute zaehlen doppelt)
//   4. Formatplatzhalter, die zwischen den Sprachen nicht uebereinstimmen -
//      ein fehlendes %s in einer Spalte gibt Muell aus statt eines Ortsnamens
//   5. Schluessel, die in src/c nirgends benutzt werden (tote Texte)
//
// Exitcode 0 = in Ordnung. Was nur auffaellt, aber nicht bricht, steht als
// HINWEIS da.
//
// GRENZE: geprueft wird, dass die Platzhalter ZWISCHEN den Sprachen gleich
// sind - nicht, dass sie zur Aufrufstelle in C passen. Ein Format aus einer
// Tabelle ist zur Uebersetzungszeit unbekannt, der Compiler kann es also auch
// nicht pruefen. Wer einen Platzhalter ergaenzt, muss den passenden Parameter
// bei jedem snprintf(..., S(SCHLUESSEL), ...) von Hand nachziehen.
'use strict';
const fs = require('fs'), path = require('path');

const defPath = process.argv[2] || path.join('src', 'c', 'strings_table.h');
const srcDir = process.argv[3] || path.join('src', 'c');

// STR(id, maxbytes, "en", "de") - Zeichenketten duerfen Klammern und Kommas
// enthalten, deshalb wird von Hand zerlegt statt per Regex.
function parseArgs(s) {
  const out = [];
  let cur = '', depth = 0, inStr = false, esc = false;
  for (const ch of s) {
    if (esc) { cur += ch; esc = false; continue; }
    if (ch === '\\') { cur += ch; esc = true; continue; }
    if (ch === '"') { inStr = !inStr; cur += ch; continue; }
    if (!inStr && ch === '(') { depth++; cur += ch; continue; }
    if (!inStr && ch === ')') { depth--; cur += ch; continue; }
    if (!inStr && depth === 0 && ch === ',') { out.push(cur.trim()); cur = ''; continue; }
    cur += ch;
  }
  if (cur.trim()) out.push(cur.trim());
  return out;
}

function unquote(s) {
  const m = /^"([\s\S]*)"$/.exec(s.trim());
  return m ? m[1].replace(/\\"/g, '"').replace(/\\n/g, '\n').replace(/\\\\/g, '\\') : null;
}

const LANGS = ['en', 'de'];
const text = fs.readFileSync(defPath, 'utf8');
const rows = [];
let errors = 0, notes = 0;

text.split('\n').forEach((line, i) => {
  const t = line.trim();
  if (!t.startsWith('STR(')) return;
  const inner = t.slice(4, t.lastIndexOf(')'));
  const args = parseArgs(inner);
  const lineNo = i + 1;
  if (args.length !== 2 + LANGS.length) {
    console.log('FEHLER ' + defPath + ':' + lineNo + ': ' + args.length +
                ' Spalten, erwartet ' + (2 + LANGS.length));
    errors++;
    return;
  }
  const id = args[0];
  const maxbytes = parseInt(args[1], 10);
  const cols = args.slice(2).map(unquote);
  if (cols.some((c) => c === null)) {
    console.log('FEHLER ' + defPath + ':' + lineNo + ' (' + id + '): Spalte ist keine Zeichenkette');
    errors++;
    return;
  }
  rows.push({ id, maxbytes, cols, lineNo });
});

console.log(rows.length + ' Texte in ' + defPath + ', ' + LANGS.length + ' Sprachen\n');

// 1 + 2
const seen = new Set();
for (const r of rows) {
  if (seen.has(r.id)) { console.log('FEHLER ' + r.id + ': doppelter Schluessel'); errors++; }
  seen.add(r.id);
  r.cols.forEach((c, li) => {
    if (c === '' && li === 0) {
      console.log('FEHLER ' + r.id + ': englische Spalte ist leer (sie ist der Rueckfall)');
      errors++;
    }
  });
}

// 3
for (const r of rows) {
  if (!r.maxbytes) continue;
  r.cols.forEach((c, li) => {
    const bytes = Buffer.byteLength(c, 'utf8');
    // Der Platzhalter wird zur Laufzeit ersetzt; sein Ergebnis kann nicht
    // geprueft werden, wohl aber der Rest.
    const literal = c.replace(/%[-0-9.]*[a-zA-Z]/g, '');
    const litBytes = Buffer.byteLength(literal, 'utf8');
    if (bytes > r.maxbytes) {
      console.log('FEHLER ' + r.id + ' [' + LANGS[li] + ']: ' + bytes +
                  ' Byte > Puffer ' + r.maxbytes + '  ' + JSON.stringify(c));
      errors++;
    } else if (c !== literal && litBytes > r.maxbytes / 2) {
      console.log('HINWEIS ' + r.id + ' [' + LANGS[li] + ']: fester Teil belegt ' +
                  litBytes + ' von ' + r.maxbytes + ' Byte, der Rest muss fuer ' +
                  'den eingesetzten Wert reichen');
      notes++;
    }
  });
}

// 4
for (const r of rows) {
  const sig = r.cols.map((c) => (c.match(/%[-0-9.]*[a-zA-Z]/g) || []).join(''));
  if (new Set(sig).size > 1) {
    console.log('FEHLER ' + r.id + ': Formatplatzhalter unterscheiden sich zwischen den Sprachen: ' +
                sig.map((s, i) => LANGS[i] + '="' + s + '"').join(', '));
    errors++;
  }
}

// 5
let code = '';
const tableName = path.basename(defPath);
for (const f of fs.readdirSync(srcDir)) {
  // Die Tabelle selbst ausnehmen - dort steht JEDER Schluessel, sonst waere
  // nie einer ungenutzt.
  if (f === tableName) continue;
  if (f.endsWith(".c") || f.endsWith(".h")) code += fs.readFileSync(path.join(srcDir, f), "utf8");
}
for (const r of rows) {
  const uses = (code.match(new RegExp('\\b' + r.id + '\\b', 'g')) || []).length;
  if (uses === 0) {
    console.log('HINWEIS ' + r.id + ': wird in ' + srcDir + ' nirgends benutzt');
    notes++;
  }
}

console.log('\nFehler: ' + errors + ', Hinweise: ' + notes);
process.exit(errors === 0 ? 0 : 1);
