// Drinktervall - Telefonseite: pflegt die Timeline-Pins.
//   * ein Pin fuer die naechste Erinnerung (Zukunft)
//   * je ein Pin fuer jeden heutigen Slot, der schon vorbei ist:
//     "Glas n getrunken" oder "Glas n verpasst" mit der Aktion "Nachholen"
// Die Pin-Texte gibt es auf Englisch, Deutsch, Franzoesisch, Italienisch und
// Spanisch; welche Sprache gilt, sagt die Uhr per MESSAGE_KEY_LANG
// (siehe src/c/strings_table.h).
// Die Watch schickt den Stand per AppMessage (src/c/phone.c): naechste
// Erinnerung, Tagesziel, Zaehler und die heutigen Slots mit Status. Pins
// haben die feste ID drinktervall-JJJJMMTT-n und wechseln ihren Inhalt.
//
// Uebertragung: zuerst Pebble.getTimelineToken + Rebble-REST-API
// (funktioniert mit der neuen Pebble-App); ohne Token die lokale
// Schnittstelle Pebble.insertTimelinePin.

var Clay = require('@rebble/clay');
var clayConfig = require('./config');

var API_URL = 'https://timeline-api.rebble.io/v1/user/pins/';
var PIN_COLOR = '#0055FF';                // Hintergrund der Pins (Pebble BlueMoon)
var STORE_KEY = 'drinktervall_pins_v2';   // id -> { sig, sentAt }, dazu legacyDeleted
var RESEND_AFTER_MS = 12 * 3600 * 1000;   // unveraenderten Pin nach 12 h erneut senden
var FORGET_AFTER_MS = 3 * 86400 * 1000;   // alte Eintraege vergessen
// Steckt in der Signatur jedes Pins: bei JEDER Aenderung am Aussehen (Symbol,
// Titel, Text, Aktionen) erhoehen. Sonst bleiben schon gesendete Pins auf ihrem
// alten Stand stehen - ihr Zustand hat sich ja nicht geaendert. Die Sprache
// steht zusaetzlich in der Signatur, die braucht also keine Erhoehung.
var LOOK_VERSION = 7;
// Einzel-Pin der Versionen 1.1.0 (aquatakt-next) und 1.1.1 (drinktervall-next).
// Die Tages-Pins aquatakt-JJJJMMTT-n aus 1.0.x stehen bewusst nicht hier: sie
// liegen in der Vergangenheit und werden nicht mehr aufgeraeumt.
var LEGACY_IDS = ['aquatakt-next', 'drinktervall-next'];

// Einstellungen des Telefons. Das Soll liegt hier, weil die Konfigseite auch
// dann aufgeht, wenn die App auf der Uhr gerade NICHT laeuft - dann erreicht
// sie kein AppMessage, und der Wert muss bis zum naechsten Start warten.
var LANG_KEY = 'drinktervall_lang';
var TARGET_KEY = 'drinktervall_target';
var TARGET_MIN = 4, TARGET_MAX = 16;
// Glasgroesse in ml. Geht denselben Weg wie das Soll: auf dem Telefon gemerkt,
// weil die Konfigseite auch bei geschlossener Watchapp aufgeht.
var GLASS_KEY = 'drinktervall_glass_ml';
var GLASS_MIN = 100, GLASS_MAX = 1000;
// Trink-Animation an/aus. Auch hier gemerkt, aus demselben Grund. Gespeichert
// wird '1' oder '0' und NICHT der Rueckgabewert von Clay: der ist je nach
// Schalterart ein Boolean, ein String oder eine Zahl, und localStorage macht
// aus allem ohnehin Text - 'false' waere dann wahr.
var ANIM_KEY = 'drinktervall_animation';

// DIE UHR IST DIE EINE STELLE, AN DER DIE EINSTELLUNGEN GELTEN. Geaendert
// werden sie hier auf der Konfigseite ODER in Kiesel-Helper; beide schicken an
// die Uhr, und die Uhr meldet mit jeder Standmeldung, was gilt. Diese Seite
// uebernimmt das - sonst zeigte die Konfigseite einen alten Stand und schickte
// ihn beim naechsten Start wieder hin, ueber eine Aenderung aus Kiesel-Helper.
//
// NUR WAS NICHT ANKAM, GEHT BEIM START NOCH EINMAL. Wurde auf der Konfigseite
// gespeichert, waehrend die App auf der Uhr nicht lief, steht hier ein
// Vermerk - und nur dann schickt der 'ready'-Zweig die gespeicherten Werte.
var PENDING_KEY = 'drinktervall_pending';

function pending() {
  try { return localStorage.getItem(PENDING_KEY) === '1'; } catch (e) { return false; }
}
function setPending(on) {
  try { if (on) localStorage.setItem(PENDING_KEY, '1'); else localStorage.removeItem(PENDING_KEY); } catch (e) {}
}

// Was die Konfigseite beim naechsten Oeffnen zeigt: Clay liest es aus
// 'clay-settings'. Hier wird hineingeschrieben, was die Uhr gemeldet hat.
function mergeClaySettings(values) {
  try {
    var s = JSON.parse(localStorage.getItem('clay-settings') || '{}') || {};
    for (var k in values) { if (values.hasOwnProperty(k)) s[k] = values[k]; }
    localStorage.setItem('clay-settings', JSON.stringify(s));
  } catch (e) {}
}

// Den Stand der Uhr uebernehmen - ausser eine eigene Aenderung ist noch
// unterwegs; die ginge sonst unter.
function adoptWatchSettings(p) {
  if (pending()) return;
  var clay = {};
  if (p.TARGET !== undefined) {
    var n = parseInt(p.TARGET, 10);
    if (isFinite(n) && n >= TARGET_MIN && n <= TARGET_MAX) {
      try { localStorage.setItem(TARGET_KEY, String(n)); } catch (e) {}
      clay.TARGET = String(n);
    }
  }
  if (p.GLASS_ML !== undefined && p.DRANK_AT === undefined) {
    var ml = parseInt(p.GLASS_ML, 10);
    if (isFinite(ml) && ml >= GLASS_MIN && ml <= GLASS_MAX) {
      try { localStorage.setItem(GLASS_KEY, String(ml)); } catch (e) {}
      clay.GLASS_ML = String(ml);
    }
  }
  if (p.ANIMATION !== undefined) {
    var on = parseInt(p.ANIMATION, 10) ? 1 : 0;
    try { localStorage.setItem(ANIM_KEY, String(on)); } catch (e) {}
    clay.ANIMATION = on === 1;
  }
  mergeClaySettings(clay);
}

var LAUNCH_CODE_DRUNK = 1;
var LAUNCH_CODE_OPEN = 2;
var SLOT_DRUNK = 2, SLOT_MISSED = 3;

// Symbole je Zustand. Sprachunabhaengig - die Texte stehen darunter.
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
var PIN_ICON = {
  next:   'system://images/NOTIFICATION_REMINDER',
  drunk:  'system://images/GENERIC_CONFIRMATION',
  missed: 'system://images/RESULT_DELETED'
};

// Die Texte je Sprache. Welche gilt, sagt die Uhr per MESSAGE_KEY_LANG - das
// Telefon kann die Uhrsprache nicht von sich aus erfahren. Index 0 ist
// Englisch und zugleich der Rueckfall, genau wie in src/c/strings_table.h;
// danach 1 Deutsch, 2 Franzoesisch, 3 Italienisch, 4 Spanisch (StringLang in
// src/c/strings.h).
// %n = Glasnummer, %g = Tagesziel. Fehlt `body` bzw. `action`, bekommt der Pin
// keinen Text bzw. keine Trink-Aktion.
var PIN_TEXT = [
  {
    next:   { title: 'Water glass %n of %g', body: 'Time for a glass of water.', action: 'Done' },
    drunk:  { title: 'Glass %n done' },
    missed: { title: 'Glass %n missed', body: 'Catch up? The app counts the glass.', action: 'Catch up' },
    open:   'Open app'
  },
  {
    next:   { title: 'Glas Wasser %n von %g', body: 'Zeit für ein Glas Wasser.', action: 'Getrunken' },
    drunk:  { title: 'Glas %n getrunken' },
    missed: { title: 'Glas %n verpasst', body: 'Nachholen? Die App zählt das Glas.', action: 'Nachholen' },
    open:   'App öffnen'
  },
  {
    next:   { title: 'Verre d\'eau %n sur %g', body: 'C\'est l\'heure d\'un verre d\'eau.', action: 'Bu' },
    drunk:  { title: 'Verre %n bu' },
    missed: { title: 'Verre %n manqué', body: 'Rattraper ? L\'app compte le verre.', action: 'Rattraper' },
    open:   'Ouvrir l\'app'
  },
  {
    next:   { title: 'Bicchiere %n di %g', body: 'È ora di un bicchiere d\'acqua.', action: 'Bevuto' },
    drunk:  { title: 'Bicchiere %n bevuto' },
    missed: { title: 'Bicchiere %n saltato', body: 'Recuperare? L\'app conta il bicchiere.', action: 'Recupera' },
    open:   'Apri app'
  },
  {
    next:   { title: 'Vaso de agua %n de %g', body: 'Hora de un vaso de agua.', action: 'Bebido' },
    drunk:  { title: 'Vaso %n bebido' },
    missed: { title: 'Vaso %n perdido', body: '¿Recuperarlo? La app cuenta el vaso.', action: 'Recuperar' },
    open:   'Abrir app'
  }
];

function pad(n) { return (n < 10 ? '0' : '') + n; }
function dayKey(d) { return '' + d.getFullYear() + pad(d.getMonth() + 1) + pad(d.getDate()); }
function pinId(epoch, index) { return 'drinktervall-' + dayKey(new Date(epoch * 1000)) + '-' + (index + 1); }

function getLang() {
  var v = parseInt(localStorage.getItem(LANG_KEY), 10);
  return (v >= 1 && v < PIN_TEXT.length) ? v : 0;
}

// Gespeichertes Soll, oder null wenn noch nie eines gewaehlt wurde. null heisst
// "nichts zu sagen": die Uhr bleibt dann bei ihrer eigenen Voreinstellung,
// statt von hier eine erfundene Zahl aufgedraengt zu bekommen.
function getTarget() {
  var v = parseInt(localStorage.getItem(TARGET_KEY), 10);
  if (!isFinite(v) || v < TARGET_MIN || v > TARGET_MAX) return null;
  return v;
}

function getGlassMl() {
  var v = parseInt(localStorage.getItem(GLASS_KEY), 10);
  if (!isFinite(v) || v < GLASS_MIN || v > GLASS_MAX) return null;
  return v;
}

// Trink-Animation, oder null wenn nie etwas gewaehlt wurde. null heisst auch
// hier "nichts zu sagen": die Uhr bleibt dann bei ihrer Voreinstellung (an).
// Nur die gespeicherte '1' oder '0' gilt - alles andere ist kein Wert von uns.
function getAnimation() {
  var v = localStorage.getItem(ANIM_KEY);
  if (v === '1') return 1;
  if (v === '0') return 0;
  return null;
}

// Was Clay fuer einen Schalter zurueckgibt, ist nicht festgelegt: true, 'true'
// oder 1 sind alle schon vorgekommen. Deshalb hier auf alle drei pruefen statt
// auf eine Form zu wetten.
function truthy(v) {
  return v === true || v === 1 || v === '1' || v === 'true';
}

// Clay erst bauen, wenn die Seite gebraucht wird: dann steht die Sprache der
// Uhr schon fest. Eine einmal gebaute Instanz bleibt, damit showConfiguration
// und webviewclosed dieselbe benutzen.
var s_clay = null;
function getClay() {
  if (!s_clay) s_clay = new Clay(clayConfig(getLang()), null, { autoHandleEvents: false });
  return s_clay;
}

function loadStore() {
  try { return JSON.parse(localStorage.getItem(STORE_KEY)) || {}; } catch (e) { return {}; }
}
function saveStore(store) {
  try { localStorage.setItem(STORE_KEY, JSON.stringify(store)); } catch (e) {}
}

function buildPin(id, epoch, state, index, goal, lang) {
  var texts = PIN_TEXT[lang] || PIN_TEXT[0];
  var look = texts[state];
  var layout = {
    type: 'genericPin',
    title: look.title.replace('%n', index + 1).replace('%g', goal),
    subtitle: 'Drinktervall',
    tinyIcon: PIN_ICON[state],
    backgroundColor: PIN_COLOR,
    foregroundColor: '#FFFFFF'
  };
  if (look.body) layout.body = look.body;
  var actions = [];
  if (look.action) actions.push({ title: look.action, type: 'openWatchApp', launchCode: LAUNCH_CODE_DRUNK });
  actions.push({ title: texts.open, type: 'openWatchApp', launchCode: LAUNCH_CODE_OPEN });
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
  // Fehlt LANG, laeuft eine aeltere Uhrseite: dann Englisch.
  var lang = msg.LANG || 0;
  // Fuer die Konfigseite merken - die oeffnet spaeter und ohne die Uhr zu fragen.
  try { localStorage.setItem(LANG_KEY, String(lang)); } catch (e) {}
  var store = loadStore();
  Object.keys(store).forEach(function (id) {
    if (store[id] && store[id].sentAt && now - store[id].sentAt > FORGET_AFTER_MS) delete store[id];
  });
  saveStore(store);

  // Ein Pin je Slot; nur senden, was sich geaendert hat oder zu lange liegt.
  var queue = [], wanted = 0;
  function want(epoch, state, index) {
    wanted += 1;
    // Die Sprache gehoert in die Signatur: ein Pin, der schon draussen ist,
    // hat nach einem Sprachwechsel unveraenderten Zustand und wuerde sonst in
    // der alten Sprache stehen bleiben.
    var id = pinId(epoch, index);
    var sig = state + ':' + epoch + ':' + goal + ':v' + LOOK_VERSION + ':l' + lang;
    var had = store[id];
    if (had && had.sig === sig && now - had.sentAt < RESEND_AFTER_MS) return;
    queue.push({ pin: buildPin(id, epoch, state, index, goal, lang), sig: sig });
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
  adoptWatchSettings(p);
  if (!p.hasOwnProperty('NEXT_TIME')) return;
  pushState(p);
});

Pebble.addEventListener('showConfiguration', function () {
  Pebble.openURL(getClay().generateUrl());
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) return;
  // false = Clay soll nichts von sich aus schicken; wir pruefen die Werte erst
  // und schicken sie dann selbst - alle in EINER Nachricht.
  var dict = getClay().getSettings(e.response, false);
  var msg = {};

  if (dict.GLASS_ML !== undefined) {
    var ml = parseInt(dict.GLASS_ML.value, 10);
    if (isFinite(ml) && ml >= GLASS_MIN && ml <= GLASS_MAX) {
      try { localStorage.setItem(GLASS_KEY, String(ml)); } catch (err) {}
      msg.GLASS_ML = ml;
    } else {
      console.log('Konfig: ungueltige Glasgroesse ' + dict.GLASS_ML.value);
    }
  }
  if (dict.ANIMATION !== undefined) {
    var on = truthy(dict.ANIMATION.value) ? 1 : 0;
    try { localStorage.setItem(ANIM_KEY, String(on)); } catch (err) {}
    msg.ANIMATION = on;
  }
  if (dict.TARGET !== undefined) {
    var n = parseInt(dict.TARGET.value, 10);
    if (isFinite(n) && n >= TARGET_MIN && n <= TARGET_MAX) {
      try { localStorage.setItem(TARGET_KEY, String(n)); } catch (err) {}
      msg.TARGET = n;
    } else {
      console.log('Konfig: ungueltiges Soll ' + dict.TARGET.value + ' - verworfen');
    }
  }
  if (!Object.keys(msg).length) return;

  // Bis die Uhr bestaetigt, gilt die Aenderung als unterwegs: kein Stand der
  // Uhr ueberschreibt sie, und beim naechsten Start geht sie noch einmal.
  setPending(true);
  Pebble.sendAppMessage(msg,
    function () { setPending(false); console.log('Konfig: an die Uhr'); },
    function () { console.log('Konfig: Uhr nicht erreichbar, gilt ab dem naechsten Start'); });
});

Pebble.addEventListener('ready', function () {
  // Nur eine Aenderung, die nie ankam, faehrt bei der Anfrage mit. Sonst
  // gilt, was die Uhr hat - womoeglich aus Kiesel-Helper -, und die Antwort
  // auf die Anfrage bringt es hierher.
  var msg = { REQUEST: 1 };
  var mitWerten = pending();
  if (mitWerten) {
    var target = getTarget();
    if (target !== null) msg.TARGET = target;
    var glass = getGlassMl();
    if (glass !== null) msg.GLASS_ML = glass;
    var anim = getAnimation();
    if (anim !== null) msg.ANIMATION = anim;
  }
  Pebble.sendAppMessage(msg,
    function () { if (mitWerten) setPending(false); },
    function () { console.log('AppMessage: Anfrage fehlgeschlagen'); });
});
