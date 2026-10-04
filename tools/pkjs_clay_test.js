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
//
// Geladen wird Clay so, wie die SDK es ins Paket buendelt
// (node_modules/@rebble/clay/dist/js/index.js), im selben Sandkasten wie
// index.js - beide teilen localStorage und Pebble.
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

function world(store) {
  store = store || {};
  const sent = [], opened = [], fehler = [];
  const ev = {};
  const schluessel = {};
  PKG.pebble.messageKeys.forEach((k, i) => { schluessel[k] = 10000 + i; });
  const sandbox = {
    console: { log: () => {}, error: () => {}, warn: () => {} },
    Date, Math, JSON, parseInt, parseFloat, isNaN, isFinite, String, Number, Object, Array,
    RegExp, Error, encodeURIComponent, decodeURIComponent, setTimeout, clearTimeout,
    XMLHttpRequest: function () {},
    localStorage: {
      getItem: (k) => (k in store ? store[k] : null),
      setItem: (k, v) => { store[k] = String(v); },
      removeItem: (k) => { delete store[k]; },
    },
    Pebble: {
      platform: 'android',
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
    store, sent, opened, fehler,
    fire: (e, arg) => {
      try { (ev[e] || []).forEach((fn) => fn(arg)); } catch (err) { fehler.push(String(err)); }
    },
  };
}

// Die Seite aus der Data-URL und ihr erstes Skript - dort stehen die
// gemerkten Werte.
function seite(w) {
  const url = w.opened[w.opened.length - 1] || '';
  const html = decodeURIComponent(url.replace(/^data:text\/html;charset=utf-8,/, ''));
  const m = html.match(/<script>([\s\S]*?)<\/script>/);
  return { html: html, skript: m ? m[1] : '' };
}
function uebersetzt(code) {
  try { new vm.Script(code); return 'ok'; } catch (e) { return String(e); }
}

console.log('\nAntwort der Seite, die kein JSON ist');
[['CANCELLED', 'Abbruch der klassischen App'], ['%E0%A4%A', 'kaputter %-Code'], ['{"TARGET":', 'abgeschnitten']]
  .forEach(function (c) {
    const w = world();
    w.fire('webviewclosed', { response: c[0] });
    check('"' + c[0] + '" (' + c[1] + '): keine Ausnahme, nichts an die Uhr',
          w.fehler.length === 0 && w.sent.length === 0, w.fehler.join(' | ') + ' ' + JSON.stringify(w.sent));
  });
{
  const w = world();
  w.fire('webviewclosed', { response: encodeURIComponent(JSON.stringify({ TARGET: { value: '10' } })) });
  check('gueltige Antwort geht weiter an die Uhr', w.fehler.length === 0 && w.sent.length === 1 && w.sent[0].TARGET === 10,
        w.fehler.join(' | ') + ' ' + JSON.stringify(w.sent));
}

console.log('\nGetraenkenamen, die die Seite brechen koennten');
['Tee $\' x', 'Mate $&', 'a</script>b', 'Cola $$', 'x $` y'].forEach(function (name) {
  // Gemerkt, wie es bis 1.20 in clay-settings kam (Clay merkt beim Schliessen
  // ungeprueft, adoptWatchSettings uebernahm den Namen der Uhr).
  const w = world({ 'clay-settings': JSON.stringify({ CUSTOM_N: '1', CUSTOM_NAME1: name }) });
  w.fire('showConfiguration');
  const s = seite(w);
  check(JSON.stringify(name) + ': das Skript der Seite uebersetzt', uebersetzt(s.skript) === 'ok', uebersetzt(s.skript));
  check(JSON.stringify(name) + ': kein Platzhalter von Clay bleibt stehen', s.html.indexOf('$$SETTINGS$$') < 0,
        'enthaelt $$SETTINGS$$');
  check(JSON.stringify(name) + ': keine Ausnahme', w.fehler.length === 0, w.fehler.join(' | '));
});
{
  const w = world();
  w.fire('appmessage', { payload: { CUSTOM: 'Tee $\' x|0|0|-1' } });
  w.fire('showConfiguration');
  const s = seite(w);
  const gemerkt = JSON.parse(w.store['clay-settings']).CUSTOM_NAME1;
  check('Name von der Uhr (Tee $\' x): die Seite uebersetzt, gemerkt ist "Tee  \' x"',
        uebersetzt(s.skript) === 'ok' && gemerkt === 'Tee  \' x', uebersetzt(s.skript) + ' / ' + gemerkt);
}
{
  const w = world();
  w.fire('webviewclosed', { response: JSON.stringify({ CUSTOM_N: { value: '1' }, CUSTOM_NAME1: { value: 'Mate <3' },
                                                       CUSTOM_KCAL1: { value: '20' } }) });
  check('gespeichert "Mate <3": an die Uhr geht "Mate  3"', w.sent.length === 1 && w.sent[0].CUSTOM === 'Mate  3|20|0|-1',
        JSON.stringify(w.sent));
  w.fire('showConfiguration');
  check('und die Seite geht danach wieder auf', uebersetzt(seite(w).skript) === 'ok', uebersetzt(seite(w).skript));
}

console.log('\nFehler: ' + fails);
process.exit(fails ? 1 : 0);
