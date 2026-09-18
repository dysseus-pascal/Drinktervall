// Die Konfigseite und der Weg des Solls zur Uhr.
//
//   node tools/pkjs_config_test.js
//
// Drei Dinge sind hier leicht falsch und schwer zu bemerken:
//
//   - Die Konfigseite geht auch auf, wenn die App auf der UHR nicht laeuft.
//     Dann erreicht sie kein AppMessage. Wird das Soll nicht zusaetzlich auf
//     dem Telefon gemerkt und beim naechsten Start nachgereicht, verpufft die
//     Auswahl still - der Nutzer stellt etwas ein, und nichts passiert.
//   - Ein unsinniger Wert (leere Auswahl, alte Seite, verdorbener Speicher)
//     darf nicht durchgereicht werden. Die Uhr begrenzt zwar selbst, aber ein
//     falscher Wert im Telefonspeicher kaeme bei jedem Start wieder.
//   - Die Seite ist zweisprachig, und die Sprache kennt nur die UHR. Sie kommt
//     per AppMessage und muss gemerkt werden, sonst steht die Seite auf
//     Englisch, waehrend die Uhr deutsch beschriftet ist.
//
// Exitcode 0 = alles wie zugesagt.
'use strict';
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const Module = require('module');

const SRC = process.env.DT_SRC || path.join(__dirname, '..', 'src', 'pkjs', 'index.js');
const CFG = path.join(__dirname, '..', 'src', 'pkjs', 'config.js');
const MIN = 4, MAX = 16, DEFAULT = 8;
const MIDDOT = '·';

let fails = 0;
function check(name, ok, detail) {
  console.log((ok ? '  ok     ' : '  FEHLER ') + name + (ok ? '' : '   -> ' + detail));
  if (!ok) fails++;
}

// Eine Welt: index.js frisch geladen, Telefonspeicher vorbelegbar. Clay wird
// nachgebaut - getSettings liefert wie das Original { KEY: { value } } und nur
// fuer Felder, die die Seite auch geschickt hat.
function world(store) {
  store = store || {};
  const sent = [];
  const failed = [];
  const clays = [];

  function XHR() { this.status = 200; }
  XHR.prototype.open = function () {};
  XHR.prototype.setRequestHeader = function () {};
  XHR.prototype.send = function () {};

  function FakeClay(config) {
    clays.push(config);
    this.generateUrl = () => 'about:blank';
    this.getSettings = (response) => {
      const raw = JSON.parse(response);
      const out = {};
      Object.keys(raw).forEach((k) => { out[k] = { value: raw[k] }; });
      return out;
    };
  }

  const sandbox = {
    console: { log: () => {} },
    Date, Math, JSON, parseInt, parseFloat, isNaN, isFinite,
    String, Number, Object, setTimeout, clearTimeout,
    XMLHttpRequest: XHR,
    localStorage: {
      getItem: (k) => (k in store ? store[k] : null),
      setItem: (k, v) => { store[k] = String(v); },
    },
    Pebble: {
      addEventListener: (ev, fn) => { (sandbox.__ev[ev] = sandbox.__ev[ev] || []).push(fn); },
      sendAppMessage: (d, ok, nok) => { sent.push(d); if (nok) failed.push(nok); },
      getTimelineToken: () => {},
      openURL: () => {},
    },
    __ev: {},
  };
  sandbox.module = { exports: {} };
  sandbox.require = function (id) {
    if (id === '@rebble/clay') return FakeClay;
    if (id === './config') return require(CFG);
    return Module.createRequire(SRC)(id);
  };
  vm.createContext(sandbox);
  vm.runInContext(fs.readFileSync(SRC, 'utf8'), sandbox, { filename: SRC });

  return {
    store: store, sent: sent, clays: clays,
    fire: (ev, arg) => (sandbox.__ev[ev] || []).forEach((fn) => fn(arg)),
    // Alle Fehler-Rueckrufe ausloesen: so verhaelt sich eine Uhr, auf der die
    // App gerade nicht laeuft.
    watchOffline: () => failed.splice(0).forEach((fn) => fn()),
    last: () => sent[sent.length - 1],
  };
}

function save(w, value) {
  w.fire('webviewclosed', { response: JSON.stringify({ TARGET: value }) });
}

function saveBoth(w, target, glass) {
  w.fire('webviewclosed', { response: JSON.stringify({ TARGET: target, GLASS_ML: glass }) });
}

function saveGlass(w, glass) {
  w.fire('webviewclosed', { response: JSON.stringify({ GLASS_ML: glass }) });
}

function saveAnim(w, on) {
  w.fire('webviewclosed', { response: JSON.stringify({ ANIMATION: on }) });
}

console.log('\nDie Seite selbst');
{
  const cfg = require(CFG);
  [0, 1].forEach(function (lang) {
    const items = cfg(lang)[2].items;
    const sel = items.filter((i) => i.messageKey === 'TARGET')[0];
    check('Sprache ' + lang + ': Auswahlfeld vorhanden', !!sel, JSON.stringify(items));
    if (!sel) return;
    const values = sel.options.map((o) => parseInt(o.value, 10));
    check('Sprache ' + lang + ': Werte ' + MIN + ' bis ' + MAX + ', luekenlos',
          values.length === MAX - MIN + 1 && values.every((v, i) => v === MIN + i),
          values.join(','));
    check('Sprache ' + lang + ': Voreinstellung ' + DEFAULT,
          sel.defaultValue === String(DEFAULT), sel.defaultValue);
    check('Sprache ' + lang + ': jede Auswahl nennt den Abstand',
          sel.options.every((o) => /\d/.test(o.label) && o.label.indexOf(MIDDOT) > 0),
          sel.options.map((o) => o.label).join(' | '));
  });
  const de = JSON.stringify(cfg(1)), en = JSON.stringify(cfg(0));
  check('Deutsch und Englisch unterscheiden sich', de !== en, 'identisch');
  check('Unbekannte Sprache faellt auf Englisch zurueck',
        JSON.stringify(cfg(7)) === en, 'weder Englisch noch Rueckfall');
}

console.log('\nAuswahl speichern');
{
  const w = world();
  save(w, '12');
  check('Soll geht sofort an die Uhr',
        !!w.last() && w.last().TARGET === 12, JSON.stringify(w.last()));
  check('Soll bleibt auf dem Telefon',
        w.store.drinktervall_target === '12', w.store.drinktervall_target);
}
{
  // Der eigentliche Fall: die Uhr ist nicht erreichbar. Beim naechsten Start
  // muss das Soll trotzdem ankommen.
  const w = world();
  save(w, '6');
  w.watchOffline();
  const w2 = world(w.store);
  w2.fire('ready');
  check('Uhr war aus: Soll faehrt beim naechsten Start mit',
        !!w2.last() && w2.last().TARGET === 6 && w2.last().REQUEST === 1,
        JSON.stringify(w2.last()));
}
{
  const w = world();
  w.fire('ready');
  check('Ohne je gewaehltes Soll schickt das Telefon keines',
        !!w.last() && w.last().REQUEST === 1 && w.last().TARGET === undefined,
        JSON.stringify(w.last()));
}

console.log('\nUnsinn abweisen');
[['0', 'Null'], ['3', 'unter dem Minimum'], ['99', 'ueber dem Maximum'],
 ['abc', 'keine Zahl'], ['', 'leer']].forEach(function (c) {
  const w = world();
  save(w, c[0]);
  check('"' + c[0] + '" (' + c[1] + ') wird verworfen',
        w.sent.length === 0 && w.store.drinktervall_target === undefined,
        JSON.stringify(w.last()) + ' / ' + w.store.drinktervall_target);
});
{
  const w = world({ drinktervall_target: '99' });
  w.fire('ready');
  check('Verdorbener Telefonspeicher wird beim Start ignoriert',
        !!w.last() && w.last().TARGET === undefined, JSON.stringify(w.last()));
}
{
  const w = world();
  w.fire('webviewclosed', { response: JSON.stringify({}) });
  check('Antwort ohne Auswahl aendert nichts',
        w.sent.length === 0 && w.store.drinktervall_target === undefined,
        JSON.stringify(w.store));
  w.fire('webviewclosed', {});
  check('Abgebrochene Seite aendert nichts', w.sent.length === 0, JSON.stringify(w.sent));
}

console.log('');
console.log('Glasgroesse');
{
  const cfg = require(CFG);
  [0, 1].forEach(function (lang) {
    const sel = cfg(lang)[3].items[1];
    check('Sprache ' + lang + ': Auswahlfeld GLASS_ML',
          !!sel && sel.messageKey === 'GLASS_ML', JSON.stringify(sel && sel.messageKey));
    if (!sel) return;
    check('Sprache ' + lang + ': Voreinstellung 300 ml',
          sel.defaultValue === '300', sel.defaultValue);
    const vals = sel.options.map((o) => parseInt(o.value, 10));
    check('Sprache ' + lang + ': alle Werte im erlaubten Bereich',
          vals.every((v) => v >= 100 && v <= 1000), vals.join(','));
    check('Sprache ' + lang + ': 3 dl ist dabei und heisst so',
          sel.options.some((o) => o.value === '300' && o.label === '3 dl'),
          sel.options.map((o) => o.label).join(' | '));
  });
}
{
  const w = world();
  saveGlass(w, '500');
  check('Glasgroesse geht an die Uhr',
        !!w.last() && w.last().GLASS_ML === 500, JSON.stringify(w.last()));
  check('Glasgroesse bleibt auf dem Telefon',
        w.store.drinktervall_glass_ml === '500', w.store.drinktervall_glass_ml);
}
{
  // Der Fall, der ohne Sorgfalt verlorenginge: die Soll-Pruefung steigt frueh
  // aus, wenn TARGET in der Antwort fehlt. Die Glasgroesse desselben
  // Speichervorgangs darf davon nicht mitgerissen werden.
  const w = world();
  saveGlass(w, '250');
  check('Glasgroesse ohne TARGET in derselben Antwort ueberlebt',
        w.store.drinktervall_glass_ml === '250', w.store.drinktervall_glass_ml);
}
{
  const w = world();
  saveBoth(w, '10', '400');
  check('beides zusammen gespeichert',
        w.store.drinktervall_target === '10' && w.store.drinktervall_glass_ml === '400',
        w.store.drinktervall_target + ' / ' + w.store.drinktervall_glass_ml);
}
[['50', 'zu klein'], ['5000', 'zu gross'], ['abc', 'keine Zahl'], ['', 'leer']].forEach(function (c) {
  const w = world();
  saveGlass(w, c[0]);
  check('Glasgroesse "' + c[0] + '" (' + c[1] + ') wird verworfen',
        w.sent.length === 0 && w.store.drinktervall_glass_ml === undefined,
        JSON.stringify(w.last()) + ' / ' + w.store.drinktervall_glass_ml);
});
{
  const w = world({ drinktervall_glass_ml: '400', drinktervall_target: '12' });
  w.fire('ready');
  check('beides faehrt beim Start mit',
        !!w.last() && w.last().GLASS_ML === 400 && w.last().TARGET === 12,
        JSON.stringify(w.last()));
}
{
  const w = world();
  w.fire('ready');
  check('ohne gewaehlte Glasgroesse schickt das Telefon keine',
        !!w.last() && w.last().GLASS_ML === undefined, JSON.stringify(w.last()));
}
{
  const w = world({ drinktervall_glass_ml: '9999' });
  w.fire('ready');
  check('verdorbene Glasgroesse wird beim Start ignoriert',
        !!w.last() && w.last().GLASS_ML === undefined, JSON.stringify(w.last()));
}

console.log('\nTrink-Animation');
{
  const cfg = require(CFG);
  [0, 1].forEach(function (lang) {
    const items = cfg(lang)[4].items;
    const tog = items.filter((i) => i.messageKey === 'ANIMATION')[0];
    check('Sprache ' + lang + ': Schalter ANIMATION vorhanden', !!tog, JSON.stringify(items));
    if (!tog) return;
    // VOREINGESTELLT AN. Stuende hier false, schaltete die Seite die Animation
    // bei jedem ab, der sie nie angefasst hat - und das saehe aus wie ein
    // Fehler, nicht wie eine Einstellung.
    check('Sprache ' + lang + ': voreingestellt an', tog.defaultValue === true,
          JSON.stringify(tog.defaultValue));
    check('Sprache ' + lang + ': Ruhezeit wird erklaert, aber nicht geschaltet',
          items.some((i) => i.type === 'text' && /Ruhezeit|Quiet Time/.test(i.defaultValue)) &&
          !items.some((i) => i.messageKey === 'QUIET'),
          JSON.stringify(items.map((i) => i.messageKey || i.type)));
  });
}
{
  const w = world();
  saveAnim(w, false);
  check('aus geht an die Uhr', !!w.last() && w.last().ANIMATION === 0, JSON.stringify(w.last()));
  check('aus bleibt als "0" auf dem Telefon',
        w.store.drinktervall_animation === '0', w.store.drinktervall_animation);
}
{
  const w = world({ drinktervall_animation: '0' });
  saveAnim(w, true);
  check('an geht an die Uhr', !!w.last() && w.last().ANIMATION === 1, JSON.stringify(w.last()));
  check('an bleibt als "1" auf dem Telefon',
        w.store.drinktervall_animation === '1', w.store.drinktervall_animation);
}
{
  // Was Clay fuer einen Schalter zurueckgibt, ist nicht festgelegt: true,
  // 'true' und 1 sind alle schon vorgekommen. Wuerde hier nur auf `true`
  // geprueft, kaeme ein 'true' als AUS an - man schaltet ein und bekommt das
  // Gegenteil. localStorage macht aus allem Text, 'false' waere dann wahr.
  [true, 'true', 1, '1'].forEach(function (v) {
    const w = world();
    saveAnim(w, v);
    check(JSON.stringify(v) + ' gilt als an',
          w.store.drinktervall_animation === '1', w.store.drinktervall_animation);
  });
  [false, 'false', 0, '0'].forEach(function (v) {
    const w = world();
    saveAnim(w, v);
    check(JSON.stringify(v) + ' gilt als aus',
          w.store.drinktervall_animation === '0', w.store.drinktervall_animation);
  });
}
{
  // Derselbe Fall wie bei der Glasgroesse: die Soll-Pruefung steigt frueh aus,
  // wenn TARGET fehlt. Der Schalter desselben Speichervorgangs darf davon
  // nicht mitgerissen werden.
  const w = world();
  saveAnim(w, false);
  check('Schalter ohne TARGET in derselben Antwort ueberlebt',
        w.store.drinktervall_animation === '0', w.store.drinktervall_animation);
}
{
  const w = world({ drinktervall_animation: '0' });
  w.fire('ready');
  check('Schalter faehrt beim naechsten Start mit',
        !!w.last() && w.last().ANIMATION === 0, JSON.stringify(w.last()));
}
{
  const w = world();
  w.fire('ready');
  check('ohne je gewaehlten Schalter schickt das Telefon keinen',
        !!w.last() && w.last().ANIMATION === undefined, JSON.stringify(w.last()));
}
{
  const w = world({ drinktervall_animation: 'vielleicht' });
  w.fire('ready');
  check('verdorbener Schalter wird beim Start ignoriert',
        !!w.last() && w.last().ANIMATION === undefined, JSON.stringify(w.last()));
}

console.log('\nSprache der Seite');
{
  const w = world();
  w.fire('appmessage', { payload: { NEXT_TIME: 1789000000, NEXT_INDEX: 0, GLASSES: 8, COUNT: 0, LANG: 1 } });
  check('Sprache der Uhr wird gemerkt', w.store.drinktervall_lang === '1', w.store.drinktervall_lang);

  const cfg = require(CFG);
  const w2 = world(w.store);
  w2.fire('showConfiguration');
  check('Konfigseite kommt auf Deutsch',
        JSON.stringify(w2.clays[0]) === JSON.stringify(cfg(1)), 'nicht die deutsche Fassung');

  const w3 = world();
  w3.fire('showConfiguration');
  check('Ohne bekannte Sprache auf Englisch',
        JSON.stringify(w3.clays[0]) === JSON.stringify(cfg(0)), 'nicht die englische Fassung');
}

console.log('\nFehler: ' + fails);
process.exit(fails ? 1 : 0);
