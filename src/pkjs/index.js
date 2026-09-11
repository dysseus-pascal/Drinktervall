// Drinktervall - Telefonseite: pflegt die Timeline-Pins.
//   * ein Pin fuer die naechste Erinnerung (Zukunft)
//   * je ein Pin fuer jeden heutigen Slot, der schon vorbei ist:
//     "Glas n getrunken" oder "Glas n verpasst" mit der Aktion "Nachholen"
// Die Watch schickt den Stand per AppMessage (src/c/phone.c): naechste
// Erinnerung, Tagesziel, Zaehler und die heutigen Slots mit Status. Pins
// haben die feste ID drinktervall-JJJJMMTT-n und wechseln ihren Inhalt.
//
// Uebertragung: zuerst Pebble.getTimelineToken + Rebble-REST-API
// (funktioniert mit der neuen Pebble-App); ohne Token die lokale
// Schnittstelle Pebble.insertTimelinePin.

var API_URL = 'https://timeline-api.rebble.io/v1/user/pins/';
var PIN_COLOR = '#0055FF';                // Hintergrund der Pins (Pebble BlueMoon)
var STORE_KEY = 'drinktervall_pins_v2';   // id -> { sig, sentAt }, dazu legacyDeleted
var RESEND_AFTER_MS = 12 * 3600 * 1000;   // unveraenderten Pin nach 12 h erneut senden
var FORGET_AFTER_MS = 3 * 86400 * 1000;   // alte Eintraege vergessen
// Steckt in der Signatur jedes Pins: bei JEDER Aenderung am Aussehen (Symbol,
// Titel, Text, Aktionen) erhoehen. Sonst bleiben schon gesendete Pins auf ihrem
// alten Stand stehen - ihr Zustand hat sich ja nicht geaendert.
var LOOK_VERSION = 6;
// Einzel-Pin der Versionen 1.1.0 (aquatakt-next) und 1.1.1 (drinktervall-next).
// Die Tages-Pins aquatakt-JJJJMMTT-n aus 1.0.x stehen bewusst nicht hier: sie
// liegen in der Vergangenheit und werden nicht mehr aufgeraeumt.
var LEGACY_IDS = ['aquatakt-next', 'drinktervall-next'];

var LAUNCH_CODE_DRUNK = 1;
var LAUNCH_CODE_OPEN = 2;
var SLOT_DRUNK = 2, SLOT_MISSED = 3;

// Aussehen je Zustand; %n = Glasnummer, %g = Tagesziel. Fehlt `body` bzw.
// `action`, bekommt der Pin keinen Text bzw. keine Trink-Aktion.
//
// ZUM SYMBOL: nur die Namen aus dem System-Satz funktionieren. Die Telefon-App
// von Core Devices faengt den Timeline-Aufruf selbst ab und setzt das Symbol
// ueber eine feste Tabelle, die ausschliesslich "system://images/..." kennt
// (RemoteTimelineEmulator.kt / TimelineIcon.kt). Ein unbekannter Name wird
// stillschweigend weggelassen, und die Uhr zeichnet dann ihr Standardsymbol
// fuer genericPin - die Flagge. Unsere eigenen Glaeser liegen als
// publishedMedia GLASS_DRUNK und GLASS_FULL bereit und werden von der Uhr auch
// aufgeloest (im Emulator belegt), aber die Telefon-App reicht sie nicht durch:
// github.com/coredevices/mobileapp Issue 275. Sobald das behoben ist, genuegt
// hier "app://images/GLASS_FULL" bzw. "...GLASS_DRUNK" und ein erhoehtes
// LOOK_VERSION.
// NOTIFICATION_REMINDER ist eine Hand mit Trinkglas und passt damit fuer die
// kommende Erinnerung; getrunkene tragen GENERIC_CONFIRMATION (einen Stern),
// verpasste RESULT_DELETED (einen Totenkopf, so gewuenscht).
// Einen Haken gibt es als Timeline-Symbol NICHT. Geprueft gegen die Tabelle
// der Firmware (timeline_resource_table): in der Liste wird die kleinste
// Groesse gezeichnet, und kein Eintrag mit dieser Groesse ist ein Haken.
// GENERIC_CONFIRMATION ist ein Stern mit Gesicht, THUMBS_UP und REWARD_GOOD
// haben gar keine kleine Groesse und fallen dort auf die Flagge zurueck.
var PIN_LOOK = {
  next:   { title: 'Glas Wasser %n von %g', body: 'Zeit für ein Glas Wasser.', icon: 'system://images/NOTIFICATION_REMINDER', action: 'Getrunken' },
  drunk:  { title: 'Glas %n getrunken', icon: 'system://images/GENERIC_CONFIRMATION' },
  missed: { title: 'Glas %n verpasst', body: 'Nachholen? Die App zählt das Glas.', icon: 'system://images/RESULT_DELETED', action: 'Nachholen' }
};

function pad(n) { return (n < 10 ? '0' : '') + n; }
function dayKey(d) { return '' + d.getFullYear() + pad(d.getMonth() + 1) + pad(d.getDate()); }
function pinId(epoch, index) { return 'drinktervall-' + dayKey(new Date(epoch * 1000)) + '-' + (index + 1); }

function loadStore() {
  try { return JSON.parse(localStorage.getItem(STORE_KEY)) || {}; } catch (e) { return {}; }
}
function saveStore(store) {
  try { localStorage.setItem(STORE_KEY, JSON.stringify(store)); } catch (e) {}
}

function buildPin(id, epoch, state, index, goal) {
  var look = PIN_LOOK[state];
  var layout = {
    type: 'genericPin',
    title: look.title.replace('%n', index + 1).replace('%g', goal),
    subtitle: 'Drinktervall',
    tinyIcon: look.icon,
    backgroundColor: PIN_COLOR,
    foregroundColor: '#FFFFFF'
  };
  if (look.body) layout.body = look.body;
  var actions = [];
  if (look.action) actions.push({ title: look.action, type: 'openWatchApp', launchCode: LAUNCH_CODE_DRUNK });
  actions.push({ title: 'App öffnen', type: 'openWatchApp', launchCode: LAUNCH_CODE_OPEN });
  return { id: id, time: new Date(epoch * 1000).toISOString(), layout: layout, actions: actions };
}

// 5 Byte pro Slot: Zeit (little endian) + Status
function decodeSlots(bytes) {
  var slots = [];
  if (!bytes) return slots;
  for (var i = 0; i + 4 < bytes.length; i += 5) {
    var t = (bytes[i] | (bytes[i + 1] << 8) | (bytes[i + 2] << 16) | (bytes[i + 3] << 24)) >>> 0;
    slots.push({ time: t, state: bytes[i + 4] });
  }
  return slots;
}

function insertViaRest(pin, token, callback) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function () { callback(this.status >= 200 && this.status < 300, 'REST ' + this.status); };
  xhr.onerror = function () { callback(false, 'REST Netzwerkfehler'); };
  xhr.open('PUT', API_URL + pin.id);
  xhr.setRequestHeader('Content-Type', 'application/json');
  xhr.setRequestHeader('X-User-Token', '' + token);
  xhr.send(JSON.stringify(pin));
}

// Lokale API: Core Devices nimmt nur den Pin (synchron), die klassische App
// pin/success/failure.
function insertViaLocal(pin, callback) {
  try {
    if (Pebble.insertTimelinePin.length >= 3) {
      var done = false;
      var finish = function (ok) { if (!done) { done = true; callback(ok, 'lokal'); } };
      setTimeout(function () { finish(false); }, 5000);
      Pebble.insertTimelinePin(pin, function () { finish(true); }, function () { finish(false); });
    } else {
      Pebble.insertTimelinePin(pin);
      callback(true, 'lokal');
    }
  } catch (e) {
    callback(false, 'lokal: ' + e);
  }
}

// Pins frueherer Versionen einmalig entfernen
function deleteLegacy(token) {
  var store = loadStore();
  if (store.legacyDeleted) return;
  var left = LEGACY_IDS.length;
  LEGACY_IDS.forEach(function (id) {
    var xhr = new XMLHttpRequest();
    xhr.onload = function () {
      console.log('timeline: alter Pin ' + id + ' geloescht (' + this.status + ')');
      left -= 1;
      if (left === 0) { var s = loadStore(); s.legacyDeleted = true; saveStore(s); }
    };
    xhr.open('DELETE', API_URL + id);
    xhr.setRequestHeader('X-User-Token', '' + token);
    xhr.send();
  });
}

function sendAll(queue, insert) {
  (function next() {
    var item = queue.shift();
    if (!item) { console.log('timeline: fertig'); return; }
    insert(item.pin, function (ok, info) {
      console.log('timeline: ' + item.pin.id + ' [' + item.sig + '] -> ' + (ok ? 'ok' : 'fehlgeschlagen') + ' (' + info + ')');
      // Pro Pin neu laden und sofort sichern: deleteLegacy schreibt nebenlaeufig
      // in denselben Store, und pkjs wird mit der App beendet - ein
      // Sammelspeichern am Ende ginge dabei verloren.
      if (ok) { var s = loadStore(); s[item.pin.id] = { sig: item.sig, sentAt: Date.now() }; saveStore(s); }
      next();
    });
  })();
}

function pushState(msg) {
  var goal = msg.GLASSES, now = Date.now();
  var store = loadStore();
  Object.keys(store).forEach(function (id) {
    if (store[id] && store[id].sentAt && now - store[id].sentAt > FORGET_AFTER_MS) delete store[id];
  });
  saveStore(store);

  // Ein Pin je Slot; nur senden, was sich geaendert hat oder zu lange liegt.
  var queue = [], wanted = 0;
  function want(epoch, state, index) {
    wanted += 1;
    var id = pinId(epoch, index), sig = state + ':' + epoch + ':' + goal + ':v' + LOOK_VERSION;
    var had = store[id];
    if (had && had.sig === sig && now - had.sentAt < RESEND_AFTER_MS) return;
    queue.push({ pin: buildPin(id, epoch, state, index, goal), sig: sig });
  }
  want(msg.NEXT_TIME, 'next', msg.NEXT_INDEX);
  decodeSlots(msg.SLOTS).forEach(function (s, i) {
    if (s.state === SLOT_DRUNK) want(s.time, 'drunk', i);
    else if (s.state === SLOT_MISSED) want(s.time, 'missed', i);
  });
  console.log('timeline: ' + queue.length + ' von ' + wanted + ' Pins zu senden');

  var useLocal = function (reason) {
    if (queue.length === 0) return;
    if (typeof Pebble.insertTimelinePin === 'function') {
      console.log('timeline: ' + reason + ', nutze lokale API');
      sendAll(queue, insertViaLocal);
    } else {
      console.log('timeline: ' + reason + ', keine lokale API - Pins uebersprungen');
    }
  };
  if (typeof Pebble.getTimelineToken !== 'function') { useLocal('kein getTimelineToken'); return; }
  Pebble.getTimelineToken(function (token) {
    deleteLegacy(token);
    if (queue.length) sendAll(queue, function (pin, cb) { insertViaRest(pin, token, cb); });
  }, function (error) {
    useLocal('kein Token (' + error + ')');
  });
}

Pebble.addEventListener('appmessage', function (e) {
  var p = e.payload;
  if (!p.hasOwnProperty('NEXT_TIME')) return;
  pushState(p);
});

Pebble.addEventListener('ready', function () {
  Pebble.sendAppMessage({ REQUEST: 1 },
    function () {}, function () { console.log('AppMessage: Anfrage fehlgeschlagen'); });
});
