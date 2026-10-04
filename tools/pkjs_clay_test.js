// Die Konfigseite mit dem ECHTEN Clay (Audit N8).
//
//   node tools/pkjs_clay_test.js       (nach npm install)
//
// Was hier leicht falsch und schwer zu bemerken ist:
//
//   - Clay WIRFT, wenn die Antwort der Seite kein JSON ist ("CANCELLED" der
//     klassischen App) oder einen kaputten %-Code traegt. Bis 1.20 flog das
//     ungefangen aus webviewclosed.
//   - Clay baut die Seite mit String.replace und setzt die gemerkten Werte in
//     einen <script>-Block. Ein Getraenkename mit "$'", "$&" oder
//     "</script>" machte die Seite kaputt - und weil die Uhr den Namen mit
//     jeder Meldung zurueckschickt, blieb sie es.
//   - Die Werte des Nutzers kommen dabei UNVERAENDERT durch: kein Zeichen
//     wird ersetzt, weder auf der Seite noch auf dem Weg zur Uhr noch im
//     Telefonspeicher.
//
// Geladen wird Clay so, wie die SDK es ins Paket buendelt
// (node_modules/@rebble/clay/dist/js/index.js), im selben Sandkasten wie
// index.js - beide teilen localStorage und Pebble. Aus der erzeugten Seite
// werden die Einstellungen so gelesen, wie die Seite sie liest: als
// JavaScript-Ausdruck zwischen "window.claySettings=" und dem naechsten
// Eintrag.
// Exitcode 0 = alles wie zugesagt.
'use strict';
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const SRC = path.join(__dirname, '..', 'src', 'pkjs', 'index.js');
const CFG = path.join(__dirname, '..', 'src', 'pkjs', 'config.js');
const CLAY = path.join(__dirname, '..', 'node_modules', '@rebble', 'clay', 'dist', 'js', 'index.js');
const PKG = require(path.join(__dirname, '..', 'package.json'));

let fails = 0;
function check(name, ok, detail) {
  console.log((ok ? '  ok     ' : '  FEHLER ') + name + (ok ? '' : '   -> ' + detail));
  if (!ok) fails++;
}

if (!fs.existsSync(CLAY)) {
  console.log('  FEHLER Clay fehlt (' + CLAY + ') - erst npm install');
  process.exit(1);
}

function world(store, plattform) {
  store = store || {};
  const sent = [], opened = [], fehler = [], logs = [];
  const ev = {};
  const schluessel = {};
  PKG.pebble.messageKeys.forEach((k, i) => { schluessel[k] = 10000 + i; });
  const sandbox = {
    console: { log: (m) => logs.push(String(m)), error: () => {}, warn: () => {} },
    Date, Math, JSON, parseInt, parseFloat, isNaN, isFinite, String, Number, Object, Array,
    RegExp, Error, encodeURIComponent, decodeURIComponent, setTimeout, clearTimeout,
    XMLHttpRequest: function () {},
    localStorage: {
      getItem: (k) => (k in store ? store[k] : null),
      setItem: (k, v) => { store[k] = String(v); },
      removeItem: (k) => { delete store[k]; },
    },
    Pebble: {
      platform: plattform || 'android',
      addEventListener: (e, fn) => { (ev[e] = ev[e] || []).push(fn); },
      sendAppMessage: (d) => sent.push(d),
      openURL: (u) => opened.push(u),
      getTimelineToken: () => {},
    },
  };
  sandbox.window = sandbox;
  vm.createContext(sandbox);
  // Erst Clay (UMD: legt sich in module.exports), dann index.js.
  sandbox.module = { exports: {} };
  sandbox.exports = sandbox.module.exports;
  sandbox.require = (id) => {
    if (id === 'message_keys') return schluessel;
    throw new Error('unbekannt: ' + id);
  };
  vm.runInContext(fs.readFileSync(CLAY, 'utf8'), sandbox, { filename: CLAY });
  const Clay = sandbox.module.exports;
  sandbox.module = { exports: {} };
  sandbox.exports = sandbox.module.exports;
  sandbox.require = (id) => {
    if (id === '@rebble/clay') return Clay;
    if (id === './config') return require(CFG);
    throw new Error('unbekannt: ' + id);
  };
  vm.runInContext(fs.readFileSync(SRC, 'utf8'), sandbox, { filename: SRC });
  return {
    store, sent, opened, fehler, logs,
    fire: (e, arg) => {
      try { (ev[e] || []).forEach((fn) => fn(arg)); } catch (err) { fehler.push(String(err)); }
    },
  };
}

// Die Seite aus der URL (Telefon: data:-URL, Emulator: Adresse mit #).
function seite(w) {
  const url = w.opened[w.opened.length - 1] || '';
  const i = url.indexOf(url.indexOf('data:') === 0 ? ',' : '#');
  return decodeURIComponent(url.slice(i + 1));
}
// Was die Seite als Einstellungen liest.
function einstellungen(html) {
  const a = html.indexOf('window.claySettings=');
  const e = html.indexOf(',window.customFn=', a);
  if (a < 0 || e < 0) return null;
  try { return vm.runInNewContext('(' + html.slice(a + 'window.claySettings='.length, e) + ')'); } catch (err) { return null; }
}
// Das erste Skript der Seite - dort stehen die gemerkten Werte. Der Browser
// beendet ein Skript bei "</script" in jeder Schreibweise.
function uebersetzt(html) {
  const m = html.match(/<script>([\s\S]*?)<\/script/i);
  try { new vm.Script(m ? m[1] : ''); return 'ok'; } catch (e) { return String(e); }
}
const zaehle = (text, was) => text.toLowerCase().split(was).length - 1;
// Fuer die Ausgabe: Zeilentrenner sichtbar.
const zeige = (name) => JSON.stringify(name).replace(/\u2028/g, '\\u2028');
// So schickt die Seite beim Speichern zurueck: je Feld { value }, als %-Code.
function antwortDerSeite(s) {
  const r = {};
  Object.keys(s).forEach((k) => { r[k] = { value: s[k] }; });
  return encodeURIComponent(JSON.stringify(r));
}

// Grundlinie: ein gewoehnlicher Name, von der Uhr gemeldet.
function seiteFuer(name, plattform) {
  const w = world({}, plattform);
  w.fire('appmessage', { payload: { CUSTOM: name + '|20|0|-1' } });
  const vorher = w.store['clay-settings'];
  w.fire('showConfiguration');
  return { w, html: seite(w), vorher };
}
const grund = seiteFuer('Mate').html;
const SKRIPTE = zaehle(grund, '</script');

console.log('\nAntwort der Seite, die kein JSON ist');
[['CANCELLED', 'Abbruch der klassischen App'], ['%E0%A4%A', 'kaputter %-Code'], ['{"TARGET":', 'abgeschnitten']]
  .forEach(function (c) {
    const w = world({ 'clay-settings': JSON.stringify({ TARGET: '8' }) });
    const vorher = JSON.stringify(w.store);
    w.fire('webviewclosed', { response: c[0] });
    check('"' + c[0] + '" (' + c[1] + '): keine Ausnahme, nichts an die Uhr, nichts geaendert',
          w.fehler.length === 0 && w.sent.length === 0 && JSON.stringify(w.store) === vorher,
          w.fehler.join(' | ') + ' ' + JSON.stringify(w.sent) + ' ' + JSON.stringify(w.store));
    check('"' + c[0] + '": es steht im Log', w.logs.some((l) => l.indexOf('Fehler (Konfig lesen)') >= 0),
          w.logs.join(' | '));
  });
{
  const w = world();
  w.fire('webviewclosed', { response: encodeURIComponent(JSON.stringify({ TARGET: { value: '10' } })) });
  check('gueltige Antwort geht weiter an die Uhr', w.fehler.length === 0 && w.sent.length === 1 && w.sent[0].TARGET === 10,
        w.fehler.join(' | ') + ' ' + JSON.stringify(w.sent));
}

console.log('\nGetraenkenamen von der Uhr durch die Seite und zurueck');
['a$&b', "Tee $' x", 'a$`b', 'Cola $$', '$$META$$', '$$SETTINGS$$', 'x</script>', '</SCRIPT>y',
 'Mate <3', 'Öl & $5', '<!--x', 'a\u2028b'].forEach((name) => {
  const { w, html, vorher } = seiteFuer(name);
  const s = einstellungen(html);
  const N = zeige(name);
  check(N + ': die Seite zeigt den Namen unveraendert', s && s.CUSTOM_NAME1 === name,
        s ? JSON.stringify(s.CUSTOM_NAME1) : 'Einstellungen nicht lesbar');
  check(N + ': die Seite ist ganz (Skript uebersetzt, Skripte, Laenge)',
        uebersetzt(html) === 'ok' && zaehle(html, '</script') === SKRIPTE && Math.abs(html.length - grund.length) < 200,
        uebersetzt(html) + ', ' + zaehle(html, '</script') + ' Skripte, ' + html.length + ' statt ' + grund.length + ' Zeichen');
  check(N + ': der Telefonspeicher bleibt, wie er war (keine Marke)',
        w.store['clay-settings'] === vorher && JSON.parse(vorher).CUSTOM_NAME1 === name, w.store['clay-settings']);
  check(N + ': keine Ausnahme', w.fehler.length === 0, w.fehler.join(' | '));
  // U+2028/U+2029 duerfen in JSON roh stehen, beenden aber in aelteren
  // WebViews einen JavaScript-String.
  const ausdruck = html.slice(html.indexOf('window.claySettings='), html.indexOf(',window.customFn='));
  check(N + ': kein roher Zeilentrenner im Skript', !/[\u2028\u2029]/.test(ausdruck), 'U+2028/U+2029 roh');
  w.fire('webviewclosed', { response: antwortDerSeite(s || {}) });
  const an = w.sent.length ? w.sent[w.sent.length - 1].CUSTOM : undefined;
  check(N + ': an die Uhr geht derselbe Name', an === name + '|20|0|-1', JSON.stringify(an));
  w.fire('showConfiguration');
  const s2 = einstellungen(seite(w));
  check(N + ': nach dem Speichern geht die Seite wieder auf, Name gleich',
        s2 && s2.CUSTOM_NAME1 === name && uebersetzt(seite(w)) === 'ok', s2 ? JSON.stringify(s2.CUSTOM_NAME1) : 'nicht lesbar');
});

console.log('\nAndere Freitextfelder, wie Clay sie beim Schliessen ungeprueft merkt');
{
  // kcal und mg sind Textfelder; Clay merkt, was darin stand, und setzt es
  // beim naechsten Oeffnen wieder ein.
  const gemerkt = { CUSTOM_N: '1', CUSTOM_NAME1: 'Saft', CUSTOM_KCAL1: "20$'", CUSTOM_MG1: '</script>5' };
  const w = world({ 'clay-settings': JSON.stringify(gemerkt) });
  w.fire('showConfiguration');
  const html = seite(w);
  const s = einstellungen(html);
  check('kcal "20$\'" und mg "</script>5": die Seite zeigt beide unveraendert',
        s && s.CUSTOM_KCAL1 === gemerkt.CUSTOM_KCAL1 && s.CUSTOM_MG1 === gemerkt.CUSTOM_MG1,
        s ? JSON.stringify([s.CUSTOM_KCAL1, s.CUSTOM_MG1]) : 'nicht lesbar');
  check('und die Seite ist ganz', uebersetzt(html) === 'ok' && zaehle(html, '</script') === SKRIPTE,
        uebersetzt(html) + ', ' + zaehle(html, '</script') + ' Skripte');
  check('der Telefonspeicher bleibt, wie er war', w.store['clay-settings'] === JSON.stringify(gemerkt), w.store['clay-settings']);
}
{
  // Noch nie gespeichert: nach dem Bauen bleibt auch kein Eintrag zurueck.
  const w = world({});
  w.fire('showConfiguration');
  const s = einstellungen(seite(w));
  check('ohne gemerkte Werte: leere Einstellungen, kein Eintrag im Speicher',
        s && Object.keys(s).length === 0 && !('clay-settings' in w.store), JSON.stringify(s) + ' ' + JSON.stringify(w.store));
}
{
  // Im Emulator haengt Clay die Seite anders an - die Marke findet sich auch dort.
  const { html } = seiteFuer('x</script>', 'pypkjs');
  const s = einstellungen(html);
  check('Emulator: auch dort unveraendert', s && s.CUSTOM_NAME1 === 'x</script>' && zaehle(html, '</script') === SKRIPTE,
        s ? JSON.stringify(s.CUSTOM_NAME1) : 'nicht lesbar');
}

console.log('\nFehler: ' + fails);
process.exit(fails ? 1 : 0);
