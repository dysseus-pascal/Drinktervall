// Die Timeline-Pins der Telefonseite (src/pkjs/index.js).
//
//   node tools/pkjs_pins_test.js
//
// Was hier leicht falsch und schwer zu bemerken ist:
//
//   - VERALTETE PINS MIT TRINK-AKTION (Audit M4): die IDs haengen am
//     Slot-Index. Sank das Soll, blieben die oberen Pins mit "Getrunken" bzw.
//     "Nachholen" stehen - und zaehlten auf der Uhr ein Glas. Ebenso die
//     naechste Erinnerung von heute, wenn das Tagesziel erreicht ist.
//     Getrunkene Pins (ohne Aktion) bleiben als Verlauf.
//   - REST SCHEITERT (Audit M5): Boulder gibt einer selbst installierten App
//     ein Ersatz-Token und verweigert ihr das Netz - xhr.open wirft. Bis 1.20
//     ging dann kein einziger Pin hinaus. Jetzt REST zuerst, dann lokal; das
//     Aufraeumen alter Pins (deleteLegacy) haelt nichts auf und versucht es
//     nur einmal.
//   - DER TAG IM LAUNCH-CODE (Audit N5): die Trink-Aktion traegt JJJJMMTT*10+1,
//     damit die Uhr einen Pin von gestern erkennt.
//
// Die Telefon-App ist nachgebaut, wie Boulder sie hat (libpebble3
// startup.js, RemoteTimelineEmulator.kt): REST-Aufrufe an die Timeline-API
// und die lokale Schnittstelle landen in DERSELBEN Timeline, gefunden ueber
// die ID; bei verweigertem Netz wirft xhr.open.
// Exitcode 0 = alles wie zugesagt.
'use strict';
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const SRC = path.join(__dirname, '..', 'src', 'pkjs', 'index.js');
const CFG = path.join(__dirname, '..', 'src', 'pkjs', 'config.js');

let fails = 0;
function check(name, ok, detail) {
  console.log((ok ? '  ok     ' : '  FEHLER ') + name + (ok ? '' : '   -> ' + detail));
  if (!ok) fails++;
}

// opts: token (sonst kein Token), status (Antwort der REST-API), netz
// ('verweigert': open wirft, 'fehler': onerror), lokal (false: keine lokale
// Schnittstelle, wie die klassische App ohne sie).
function world(opts) {
  opts = opts || {};
  const store = {};
  const timeline = {};
  const rest = [], lokal = [], fehler = [];
  function XHR() {}
  XHR.prototype.open = function (m, u) {
    if (opts.netz === 'verweigert') throw new Error('Network access denied for this watchapp (XMLHttpRequest)');
    this.m = m; this.u = u;
  };
  XHR.prototype.setRequestHeader = function () {};
  XHR.prototype.send = function (body) {
    const id = this.u.split('/').pop();
    rest.push(this.m + ' ' + id);
    if (opts.netz === 'fehler') { if (this.onerror) this.onerror(); return; }
    this.status = opts.status || 200;
    if (this.status >= 200 && this.status < 300) {
      if (this.m === 'PUT') timeline[id] = JSON.parse(body);
      if (this.m === 'DELETE') delete timeline[id];
    }
    if (this.onload) this.onload.call(this);
  };
  const ev = {};
  const Pebble = {
    addEventListener: (e, fn) => { (ev[e] = ev[e] || []).push(fn); },
    sendAppMessage: () => {},
    openURL: () => {},
    getTimelineToken: (ok, nok) => (opts.token ? ok(opts.token) : nok('kein Token')),
  };
  if (opts.lokal !== false) {
    // Wie startup.js: ein Argument, kein Rueckruf.
    Pebble.insertTimelinePin = (pin) => { lokal.push('insert ' + pin.id); timeline[pin.id] = pin; };
    Pebble.deleteTimelinePin = (id) => { lokal.push('delete ' + id); delete timeline[id]; };
  }
  const sandbox = {
    console: { log: () => {} },
    Date, Math, JSON, parseInt, parseFloat, isNaN, isFinite, String, Number, Object, Array,
    RegExp, Error, setTimeout, clearTimeout,
    XMLHttpRequest: XHR,
    localStorage: {
      getItem: (k) => (k in store ? store[k] : null),
      setItem: (k, v) => { store[k] = String(v); },
      removeItem: (k) => { delete store[k]; },
    },
    Pebble: Pebble,
  };
  sandbox.module = { exports: {} };
  sandbox.require = (id) => {
    if (id === '@rebble/clay') return function () {};
    if (id === './config') return require(CFG);
    throw new Error('unbekannt: ' + id);
  };
  vm.createContext(sandbox);
  vm.runInContext(fs.readFileSync(SRC, 'utf8'), sandbox, { filename: SRC });
  return {
    store, timeline, rest, lokal, fehler,
    melde: (payload) => {
      try { (ev.appmessage || []).forEach((fn) => fn({ payload: payload })); } catch (e) { fehler.push(String(e)); }
    },
  };
}

function slotsBytes(zeiten, zustand) {
  const b = [];
  zeiten.forEach((t, i) => b.push(t & 255, (t >> 8) & 255, (t >> 16) & 255, (t >>> 24) & 255, zustand(i)));
  return b;
}
function sek(d) { return Math.floor(d.getTime() / 1000); }
// Grundraster der Uhr (ohne Versatz) am 03.10.2026, Ortszeit des Laufs.
function plan(n, tag) {
  const iv = (12 * 60) / n;
  return Array.from({ length: n }, (_, i) => sek(new Date(2026, 9, tag || 3, 8, i * iv)));
}
// Die Uhr um 15:00: Soll `n`, `count` getrunken; vorbei ist, was vor 15:00 liegt.
function meldung(n, count) {
  const p = plan(n), jetzt = sek(new Date(2026, 9, 3, 15, 0));
  let next = p.findIndex((t) => t > jetzt);
  const nextZeit = next >= 0 && count < n ? p[next] : plan(n, 4)[0];
  if (next < 0 || count >= n) next = 0;
  return {
    GLASSES: n, COUNT: count, LANG: 1, NEXT_TIME: nextZeit, NEXT_INDEX: next,
    SLOTS: slotsBytes(p, (i) => (p[i] > jetzt ? 0 : (i < count ? 2 : 3))),
  };
}
const TRINKEN = (a) => a.type === 'openWatchApp' && a.launchCode % 10 === 1;
// Pins von heute, die eine Trink-Aktion tragen.
function mitAktion(w) {
  return Object.keys(w.timeline).filter((id) => id.indexOf('drinktervall-20261003-') === 0 &&
    (w.timeline[id].actions || []).some(TRINKEN)).sort();
}

console.log('\nSoll gesenkt (M4): Pins mit Trink-Aktion ueber dem neuen Soll verschwinden');
[['REST', { token: 'emulated-dummy-token' }], ['lokal ohne Token', {}],
 ['REST scheitert, lokal', { token: 'emulated-dummy-token', status: 500 }]].forEach(function (fall) {
  const w = world(fall[1]);
  w.melde(meldung(8, 3));
  check(fall[0] + ': bei Soll 8 tragen -4, -5 (verpasst) und -6 (naechste) die Aktion',
        mitAktion(w).join(',') === 'drinktervall-20261003-4,drinktervall-20261003-5,drinktervall-20261003-6',
        mitAktion(w).join(','));
  w.melde(meldung(4, 3));
  check(fall[0] + ': bei Soll 4 nur noch -4, die naechste Erinnerung',
        mitAktion(w).join(',') === 'drinktervall-20261003-4', mitAktion(w).join(','));
  check(fall[0] + ': getrunkene Pins bleiben stehen',
        ['1', '2', '3'].every((n) => w.timeline['drinktervall-20261003-' + n]), Object.keys(w.timeline).join(','));
  const s = JSON.parse(w.store.drinktervall_pins_v2 || '{}');
  check(fall[0] + ': geloeschte Pins sind nicht mehr vermerkt',
        !s['drinktervall-20261003-5'] && !s['drinktervall-20261003-6'], Object.keys(s).join(','));
  check(fall[0] + ': keine Ausnahme', w.fehler.length === 0, w.fehler.join(' | '));
});
{
  const w = world({ token: 'emulated-dummy-token' });
  w.melde(meldung(8, 3));
  w.melde(meldung(8, 8));
  check('Tagesziel erreicht: die naechste Erinnerung von heute (-6) verschwindet',
        mitAktion(w).join(',') === '', mitAktion(w).join(','));
  check('Tagesziel erreicht: die naechste ist die von morgen', !!w.timeline['drinktervall-20261004-1'],
        Object.keys(w.timeline).join(','));
  const vorher = w.rest.length + w.lokal.length;
  w.melde(meldung(8, 8));
  check('dieselbe Meldung noch einmal: nichts zu tun', w.rest.length + w.lokal.length === vorher,
        w.rest.slice(vorher).concat(w.lokal).join(','));
}

console.log('\nREST scheitert (M5): die Pins gehen lokal hinaus');
[['REST 500', { token: 'emulated-dummy-token', status: 500 }],
 ['Netz verweigert (open wirft)', { token: 'emulated-dummy-token', netz: 'verweigert' }],
 ['Netzwerkfehler (onerror)', { token: 'emulated-dummy-token', netz: 'fehler' }]].forEach(function (fall) {
  const w = world(fall[1]);
  w.melde(meldung(8, 3));
  const ids = Object.keys(w.timeline).filter((id) => id.indexOf('drinktervall-') === 0).sort();
  check(fall[0] + ': alle sechs Pins in der Timeline', ids.length === 6, ids.join(','));
  check(fall[0] + ': ueber die lokale Schnittstelle', w.lokal.filter((x) => x.indexOf('insert ') === 0).length === 6,
        w.lokal.join(','));
  check(fall[0] + ': keine Ausnahme', w.fehler.length === 0, w.fehler.join(' | '));
  check(fall[0] + ': alte Pins nur einmal versucht, dann vermerkt',
        JSON.parse(w.store.drinktervall_pins_v2 || '{}').legacyDeleted === true, w.store.drinktervall_pins_v2);
  const legacy = () => w.rest.concat(w.lokal).filter((x) => /aquatakt-next|drinktervall-next/.test(x)).length;
  const n = legacy();
  w.melde(meldung(8, 4));
  check(fall[0] + ': die naechste Meldung versucht sie nicht wieder', legacy() === n, String(legacy()) + ' statt ' + n);
});
{
  const w = world({ token: 'emulated-dummy-token' });
  w.melde(meldung(8, 3));
  check('REST klappt: nichts geht lokal', w.lokal.length === 0 && Object.keys(w.timeline).length === 6,
        w.lokal.join(','));
}
{
  const w = world({ token: 'emulated-dummy-token', status: 500, lokal: false });
  w.melde(meldung(8, 3));
  check('REST scheitert, keine lokale Schnittstelle: keine Ausnahme, nichts vermerkt',
        w.fehler.length === 0 && Object.keys(JSON.parse(w.store.drinktervall_pins_v2 || '{}'))
          .filter((k) => k !== 'legacyDeleted').length === 0, w.fehler.join(' | ') + ' ' + w.store.drinktervall_pins_v2);
}
{
  // Kein Weg klappt: die alten Pins werden trotzdem nur einmal versucht.
  const w = world({ token: 'emulated-dummy-token', netz: 'fehler', lokal: false });
  w.melde(meldung(8, 3));
  w.melde(meldung(8, 4));
  const legacy = w.rest.filter((x) => /aquatakt-next|drinktervall-next/.test(x));
  check('Netzwerkfehler ohne lokale Schnittstelle: alte Pins einmal versucht, nicht bei jeder Meldung',
        legacy.length === 2 && JSON.parse(w.store.drinktervall_pins_v2 || '{}').legacyDeleted === true,
        legacy.join(','));
}

console.log('\nDer Tag im Launch-Code (N5)');
{
  const w = world({ token: 'emulated-dummy-token' });
  const m = meldung(8, 3);
  m.NEXT_TIME = plan(8, 4)[0];                         // am Abend: die naechste ist morgen frueh
  m.NEXT_INDEX = 0;
  w.melde(m);
  const aktionen = [];
  Object.keys(w.timeline).forEach((id) => (w.timeline[id].actions || []).filter(TRINKEN)
    .forEach((a) => aktionen.push({ id: id, code: a.launchCode, zeit: new Date(w.timeline[id].time) })));
  const tag = (d) => d.getFullYear() * 10000 + (d.getMonth() + 1) * 100 + d.getDate();
  check('jede Trink-Aktion traegt den Tag ihres Pins (JJJJMMTT1)',
        aktionen.length === 3 && aktionen.every((a) => a.code === tag(a.zeit) * 10 + 1),
        JSON.stringify(aktionen.map((a) => a.id + '=' + a.code)));
  check('verpasst von heute: 202610031', aktionen.some((a) => a.id === 'drinktervall-20261003-4' && a.code === 202610031),
        JSON.stringify(aktionen));
  check('naechste von morgen: 202610041', aktionen.some((a) => a.id === 'drinktervall-20261004-1' && a.code === 202610041),
        JSON.stringify(aktionen));
  check('passt in einen uint32', aktionen.every((a) => a.code > 0 && a.code < 4294967296), JSON.stringify(aktionen));
  const offen = Object.keys(w.timeline).map((id) => w.timeline[id].actions.filter((a) => a.launchCode === 2).length);
  check('"App oeffnen" bleibt 2', offen.every((n) => n === 1), JSON.stringify(offen));
}

console.log('\nFehler: ' + fails);
process.exit(fails ? 1 : 0);
