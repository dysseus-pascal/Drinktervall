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
//   6. die laengste Erinnerungszeile aus coffee_describe ("Sorte, koffeinfrei,
//      Milch, Zucker") gegen ihren Puffer COFFEE_DESCRIBE_MAX aus coffee.h -
//      die Teile stehen einzeln in der Tabelle, die Summe sieht sonst keiner
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

// STR(id, maxbytes, "en", "de", "fr", "it", "es") - Zeichenketten duerfen Klammern und Kommas
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

const LANGS = ['en', 'de', 'fr', 'it', 'es'];
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

// 6
// coffee_describe (coffee.c) haengt an den Sortennamen jeden Zusatz mit ", "
// an. Gelesen wird aus dem C-Code selbst: die Sorten aus dem Feld names[] in
// coffee_describe, die Zusaetze aus den S(STR_...)-Aufrufen dort, die
// Puffergroesse aus #define COFFEE_DESCRIBE_MAX in coffee.h. Geprueft wird je
// Sprache die laengste Sorte mit ALLEN Zusaetzen - bewusst ohne Ruecksicht
// darauf, welche Sorte welchen Zusatz kennt (coffee_milk_possible,
// coffee_decaf_possible): so bleibt die Pruefung richtig, wenn eine Sorte
// einen Zusatz dazubekommt. Dazu muss jeder Aufrufer einen Puffer dieser
// Groesse uebergeben; ein char-Feld mit fester Zahl faellt hier auf.
{
  const SEP = ', ';
  const read = (f) => { try { return fs.readFileSync(path.join(srcDir, f), 'utf8'); } catch (e) { return ''; } };
  const header = read('coffee.h');
  const maxMatch = /#define\s+COFFEE_DESCRIBE_MAX\s+(\d+)/.exec(header);
  const body = (/void\s+coffee_describe\s*\([^)]*\)\s*\{([\s\S]*?)\n\}/.exec(read('coffee.c')) || [])[1] || '';
  const namesMatch = /names\s*\[[^\]]*\]\s*=\s*\{([^}]*)\}/.exec(body);
  const kinds = namesMatch ? namesMatch[1].split(',').map((x) => x.trim()).filter(Boolean) : [];
  const extras = (body.match(/\bS\(\s*(STR_\w+)\s*\)/g) || []).map((x) => /STR_\w+/.exec(x)[0]);
  const byId = new Map(rows.map((r) => [r.id, r]));
  const unknown = kinds.concat(extras).filter((id) => !byId.has(id));
  if (!maxMatch || !kinds.length || !extras.length || unknown.length) {
    console.log('FEHLER coffee_describe: Pruefung 6 findet ' +
                (!maxMatch ? 'COFFEE_DESCRIBE_MAX in coffee.h nicht' :
                 !kinds.length ? 'die Sorten (names[]) in coffee.c nicht' :
                 !extras.length ? 'die Zusaetze (S(STR_...)) in coffee.c nicht' :
                 'unbekannte Schluessel ' + unknown.join(', ')));
    errors++;
  } else {
    const max = parseInt(maxMatch[1], 10);
    const worst = LANGS.map((lang, li) => {
      const text = (id) => byId.get(id).cols[li] || byId.get(id).cols[0];
      const name = kinds.map(text).reduce((a, b) => (Buffer.byteLength(b) > Buffer.byteLength(a) ? b : a));
      const line = [name].concat(extras.map(text)).join(SEP);
      return { lang, line, bytes: Buffer.byteLength(line, 'utf8') + 1 };
    });
    console.log('coffee_describe, Puffer ' + max + ' Byte, laengste Zeile mit Null: ' +
                worst.map((w) => w.lang + ' ' + w.bytes).join(', '));
    for (const w of worst) {
      if (w.bytes > max) {
        console.log('FEHLER coffee_describe [' + w.lang + ']: ' + w.bytes + ' Byte > Puffer ' +
                    max + '  ' + JSON.stringify(w.line));
        errors++;
      }
    }
    // Die Aufrufer: coffee_describe(..., buf, sizeof(buf)) mit char buf[N].
    const need = Math.max.apply(null, worst.map((w) => w.bytes));
    for (const f of fs.readdirSync(srcDir)) {
      if (!f.endsWith('.c')) continue;
      const src = read(f);
      const calls = /coffee_describe\s*\([^;]*?,\s*(\w+)\s*,\s*sizeof\s*\(\s*(\w+)\s*\)\s*\)/g;
      let m;
      while ((m = calls.exec(src))) {
        const decl = new RegExp('char\\s+' + m[1] + '\\s*\\[\\s*(\\w+)\\s*\\]').exec(src);
        if (m[1] !== m[2] || !decl) {
          console.log('HINWEIS ' + f + ': Puffer von coffee_describe nicht erkannt (' + m[0] + ')');
          notes++;
          continue;
        }
        // Mit der Konstante ist die Groesse oben schon geprueft.
        if (decl[1] === 'COFFEE_DESCRIBE_MAX') continue;
        if (!(parseInt(decl[1], 10) >= need)) {
          console.log('FEHLER ' + f + ': char ' + m[1] + '[' + decl[1] + '] fuer coffee_describe, ' +
                      'gebraucht ' + need + ' Byte - COFFEE_DESCRIBE_MAX nehmen');
          errors++;
        }
      }
    }
  }
}

console.log('\nFehler: ' + errors + ', Hinweise: ' + notes);
process.exit(errors === 0 ? 0 : 1);
