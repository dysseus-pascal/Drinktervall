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
// (funktioniert mit der neuen Pebble-App); ohne Token, und wenn REST
// scheitert, die lokale Schnittstelle Pebble.insertTimelinePin bzw.
// Pebble.deleteTimelinePin.

var Clay = require('@rebble/clay');
var clayConfig = require('./config');

// KEIN CATCH OHNE LOG. Ein Fehler beim Telefonspeicher oder beim Lesen soll im
// Log der Pebble-App stehen und nicht still verschwinden - sonst laesst sich
// eine verlorene Einstellung hinterher nicht erklaeren. tools/catch_check.js
// prueft, dass jeder catch ins Log meldet.
function meldeFehler(wo, e) {
  console.log('Fehler (' + wo + '): ' + e);
}

var API_URL = 'https://timeline-api.rebble.io/v1/user/pins/';
var PIN_COLOR = '#0055FF';                // Hintergrund der Pins (Pebble BlueMoon)
var STORE_KEY = 'drinktervall_pins_v2';   // id -> { sig, sentAt }, dazu legacyDeleted
var RESEND_AFTER_MS = 12 * 3600 * 1000;   // unveraenderten Pin nach 12 h erneut senden
var FORGET_AFTER_MS = 3 * 86400 * 1000;   // alte Eintraege vergessen
// Steckt in der Signatur jedes Pins: bei JEDER Aenderung am Aussehen (Symbol,
// Titel, Text, Aktionen) erhoehen. Sonst bleiben schon gesendete Pins auf ihrem
// alten Stand stehen - ihr Zustand hat sich ja nicht geaendert. Die Sprache
// steht zusaetzlich in der Signatur, die braucht also keine Erhoehung.
// 8: die Trink-Aktion traegt den Tag des Pins im Launch-Code (Audit N5).
var LOOK_VERSION = 8;
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
// Kaffeeplan, wie er an die Uhr geht: [Anzahl, je Kaffee Minute lo, hi,
// Sorte, Flags] - siehe src/c/coffee.h. Als JSON-Feld gemerkt, aus demselben
// Grund wie das Soll.
var COFFEE_KEY = 'drinktervall_coffee';
var COFFEE_MILK = 1, COFFEE_SUGAR = 2, COFFEE_DECAF = 4, COFFEE_ENERGY = 3;
// Eigene Getraenke, wie sie an die Uhr gehen: je Zeile "Name|kcal|mg|Minute",
// Minute -1 ohne Erinnerung.
var CUSTOM_KEY = 'drinktervall_custom';

// DIE UHR IST DIE EINE STELLE, AN DER DIE EINSTELLUNGEN GELTEN. Geaendert
// werden sie hier auf der Konfigseite (bis zu seiner Archivierung am
// 29.09.2026 auch in Kiesel-Helper); sie schickt an die Uhr, und die Uhr
// meldet mit jeder Standmeldung, was gilt. Boulder liest nur mit. Diese Seite
// uebernimmt das - sonst zeigte die Konfigseite einen alten Stand und schickte
// ihn beim naechsten Start wieder hin, ueber eine Aenderung von anderswo.
//
// NUR WAS NICHT ANKAM, GEHT BEIM START NOCH EINMAL. Wurde auf der Konfigseite
// gespeichert, waehrend die App auf der Uhr nicht lief, steht hier ein
// Vermerk - und nur dann schickt der 'ready'-Zweig die gespeicherten Werte.
var PENDING_KEY = 'drinktervall_pending';

function pending() {
  try { return localStorage.getItem(PENDING_KEY) === '1'; } catch (e) { meldeFehler('Vermerk lesen', e); return false; }
}
function setPending(on) {
  try { if (on) localStorage.setItem(PENDING_KEY, '1'); else localStorage.removeItem(PENDING_KEY); } catch (e) { meldeFehler('Vermerk schreiben', e); }
}

// Den Kaffeeplan aus den Feldern der Konfigseite bauen. Aus heisst ein
// einzelnes Null-Byte; Milch und koffeinfrei zaehlen nicht beim Energy-Drink,
// auch wenn sein Schalter noch von frueher an ist.
function coffeeBytes(dict) {
  var out = [0];
  if (!dict.COFFEE_ON || !truthy(dict.COFFEE_ON.value)) return out;
  var n = parseInt(dict.COFFEE_N && dict.COFFEE_N.value, 10);
  if (!(n >= 1 && n <= clayConfig.COFFEE_MAX)) return out;
  for (var i = 1; i <= n; i++) {
    var zeit = parseInt(dict['COFFEE_TIME' + i] && dict['COFFEE_TIME' + i].value, 10);
    var art = parseInt(dict['COFFEE_TYPE' + i] && dict['COFFEE_TYPE' + i].value, 10);
    if (!(zeit >= 0 && zeit < 1440) || !(art >= 0 && art <= 3)) continue;
    var flags = 0;
    if (art !== COFFEE_ENERGY && dict['COFFEE_MILK' + i] && truthy(dict['COFFEE_MILK' + i].value)) flags |= COFFEE_MILK;
    if (dict['COFFEE_SUGAR' + i] && truthy(dict['COFFEE_SUGAR' + i].value)) flags |= COFFEE_SUGAR;
    if (art !== COFFEE_ENERGY && dict['COFFEE_DECAF' + i] && truthy(dict['COFFEE_DECAF' + i].value)) flags |= COFFEE_DECAF;
    out.push(zeit & 0xFF, zeit >> 8, art, flags);
    out[0] += 1;
  }
  return out;
}

// Der Plan der Uhr in die Felder der Konfigseite. Ist er aus, bleiben die
// Felder stehen, wie sie waren - wer wieder einschaltet, findet seine Zeiten.
function coffeeToClay(bytes, clay) {
  var n = bytes && bytes.length ? bytes[0] : 0;
  clay.COFFEE_ON = n > 0;
  if (!n || bytes.length < 1 + n * 4) return;
  clay.COFFEE_N = String(n);
  for (var i = 0; i < n; i++) {
    var b = 1 + i * 4;
    clay['COFFEE_TIME' + (i + 1)] = String(bytes[b] | (bytes[b + 1] << 8));
    clay['COFFEE_TYPE' + (i + 1)] = String(bytes[b + 2]);
    clay['COFFEE_MILK' + (i + 1)] = (bytes[b + 3] & COFFEE_MILK) !== 0;
    clay['COFFEE_SUGAR' + (i + 1)] = (bytes[b + 3] & COFFEE_SUGAR) !== 0;
    clay['COFFEE_DECAF' + (i + 1)] = (bytes[b + 3] & COFFEE_DECAF) !== 0;
  }
}

// Fuer einen Namen hat die Uhr 15 BYTE (DT_CUSTOM_NAME - 1 in src/c/config.h),
// nicht 15 Zeichen.
var CUSTOM_NAME_BYTES = 15;

// Hoechstens `max` Byte UTF-8, gekuerzt nur an Zeichengrenzen. BIS 1.19 STAND
// HIER slice(0, 15): das zaehlt Zeichen, ein "ü" sind aber 2 Byte, ein Emoji
// 4 - die Uhr schnitt den Rest dann mitten im Zeichen ab, und kaputtes UTF-8
// stand auf der Uhr und in der Gesundheitsakte. Ein Emoji steht in JavaScript
// als ZWEI Ersatzzeichen und bleibt hier beisammen.
function utf8Kuerzen(text, max) {
  var s = String(text || ''), bytes = 0, ende = 0;
  for (var i = 0; i < s.length; i++) {
    var c = s.charCodeAt(i);
    var d = i + 1 < s.length ? s.charCodeAt(i + 1) : 0;
    var paar = c >= 0xd800 && c <= 0xdbff && d >= 0xdc00 && d <= 0xdfff;
    var n = paar ? 4 : (c < 0x80 ? 1 : (c < 0x800 ? 2 : 3));
    if (bytes + n > max) break;
    bytes += n;
    if (paar) i++;
    ende = i + 1;
  }
  return s.slice(0, ende);
}

// WAS AUF DER KONFIGSEITE STEHT, MUSS DURCH CLAY. Clay baut die Seite mit
// String.replace und setzt die gemerkten Werte in einen <script>-Block:
// "$&", "$'" oder "$$" in einem Wert werden dabei zu Teilen der Seite, und
// ein "</script>" beendet das Skript. Bis 1.20 kam so ein Getraenkename bis
// in die Seite - sie ging nicht mehr auf, und weil die Uhr den Namen jedes
// Mal zurueckmeldet, blieb das so (Audit N8). "$", "<" und ">" werden darum
// zu Leerzeichen.
function seitenSicher(text) {
  return String(text).replace(/[$<>]/g, ' ');
}

// Alle gemerkten Werte der Seite saeubern, bevor Clay sie einbaut - auch die,
// die Clay beim Schliessen selbst ungeprueft gemerkt hat.
function claySaeubern() {
  try {
    var s = JSON.parse(localStorage.getItem('clay-settings') || '{}') || {};
    var geaendert = false;
    Object.keys(s).forEach(function (k) {
      if (typeof s[k] === 'string' && seitenSicher(s[k]) !== s[k]) { s[k] = seitenSicher(s[k]); geaendert = true; }
    });
    if (geaendert) localStorage.setItem('clay-settings', JSON.stringify(s));
  } catch (e) { meldeFehler('Konfigseite saeubern', e); }
}

// Die eigenen Getraenke aus den Feldern. Ohne Namen zaehlt ein Getraenk
// nicht; "|" und Zeilenumbruch im Namen wuerden die Zeile zerlegen, "$", "<"
// und ">" die Konfigseite (seitenSicher).
function customText(dict) {
  var n = parseInt(dict.CUSTOM_N && dict.CUSTOM_N.value, 10) || 0;
  var zeilen = [];
  for (var i = 1; i <= n && i <= clayConfig.CUSTOM_MAX; i++) {
    var name = utf8Kuerzen(seitenSicher(String((dict['CUSTOM_NAME' + i] && dict['CUSTOM_NAME' + i].value) || ''))
      .replace(/[|\n\r]/g, ' ').trim(), CUSTOM_NAME_BYTES).trim();
    if (!name) continue;
    var kcal = Math.max(0, Math.min(2000, parseInt(dict['CUSTOM_KCAL' + i] && dict['CUSTOM_KCAL' + i].value, 10) || 0));
    var mg = Math.max(0, Math.min(1000, parseInt(dict['CUSTOM_MG' + i] && dict['CUSTOM_MG' + i].value, 10) || 0));
    var erinnern = dict['CUSTOM_REMIND' + i] && truthy(dict['CUSTOM_REMIND' + i].value);
    var minute = parseInt(dict['CUSTOM_TIME' + i] && dict['CUSTOM_TIME' + i].value, 10);
    if (!erinnern || !(minute >= 0 && minute < 1440)) minute = -1;
    zeilen.push(name + '|' + kcal + '|' + mg + '|' + minute);
  }
  return zeilen.join('\n');
}

// Die eigenen Getraenke der Uhr in die Felder der Konfigseite.
function customToClay(text, clay) {
  var zeilen = text ? String(text).split('\n') : [];
  clay.CUSTOM_N = String(zeilen.length);
  zeilen.forEach(function (z, i) {
    var f = z.split('|');
    clay['CUSTOM_NAME' + (i + 1)] = seitenSicher(f[0] || '');
    clay['CUSTOM_KCAL' + (i + 1)] = f[1] || '0';
    clay['CUSTOM_MG' + (i + 1)] = f[2] || '0';
    // Ohne viertes Feld (Uhr bis 1.16) oder mit -1: keine Erinnerung.
    var minute = parseInt(f[3], 10);
    clay['CUSTOM_REMIND' + (i + 1)] = minute >= 0;
    if (minute >= 0) clay['CUSTOM_TIME' + (i + 1)] = String(minute);
  });
}

function getCustom() {
  try { var v = localStorage.getItem(CUSTOM_KEY); return v === null ? null : v; } catch (e) { meldeFehler('Eigene Getraenke lesen', e); return null; }
}

function getCoffee() {
  try {
    var v = JSON.parse(localStorage.getItem(COFFEE_KEY));
    return (v && v.length) ? v : null;
  } catch (e) { meldeFehler('Kaffeeplan lesen', e); return null; }
}

// Was die Konfigseite beim naechsten Oeffnen zeigt: Clay liest es aus
// 'clay-settings'. Hier wird hineingeschrieben, was die Uhr gemeldet hat.
function mergeClaySettings(values) {
  try {
    var s = JSON.parse(localStorage.getItem('clay-settings') || '{}') || {};
    for (var k in values) { if (values.hasOwnProperty(k)) s[k] = values[k]; }
    localStorage.setItem('clay-settings', JSON.stringify(s));
  } catch (e) { meldeFehler('Konfigseite nachfuehren', e); }
}

// Den Stand der Uhr uebernehmen - ausser eine eigene Aenderung ist noch
// unterwegs; die ginge sonst unter.
function adoptWatchSettings(p) {
  if (pending()) return;
  var clay = {};
  if (p.TARGET !== undefined) {
    var n = parseInt(p.TARGET, 10);
    if (isFinite(n) && n >= TARGET_MIN && n <= TARGET_MAX) {
      try { localStorage.setItem(TARGET_KEY, String(n)); } catch (e) { meldeFehler('Soll speichern', e); }
      clay.TARGET = String(n);
    }
  }
  if (p.GLASS_ML !== undefined && p.DRANK_AT === undefined) {
    var ml = parseInt(p.GLASS_ML, 10);
    if (isFinite(ml) && ml >= GLASS_MIN && ml <= GLASS_MAX) {
      try { localStorage.setItem(GLASS_KEY, String(ml)); } catch (e) { meldeFehler('Glasgroesse speichern', e); }
      clay.GLASS_ML = String(ml);
    }
  }
  if (p.ANIMATION !== undefined) {
    var on = parseInt(p.ANIMATION, 10) ? 1 : 0;
    try { localStorage.setItem(ANIM_KEY, String(on)); } catch (e) { meldeFehler('Animation speichern', e); }
    clay.ANIMATION = on === 1;
  }
  if (p.COFFEE !== undefined && p.COFFEE.length) {
    var bytes = Array.prototype.slice.call(p.COFFEE);
    try { localStorage.setItem(COFFEE_KEY, JSON.stringify(bytes)); } catch (e) { meldeFehler('Kaffeeplan speichern', e); }
    coffeeToClay(bytes, clay);
  }
  if (p.CUSTOM !== undefined) {
    try { localStorage.setItem(CUSTOM_KEY, String(p.CUSTOM)); } catch (e) { meldeFehler('Eigene Getraenke speichern', e); }
    customToClay(p.CUSTOM, clay);
  }
  mergeClaySettings(clay);
}

var LAUNCH_CODE_DRUNK = 1;
var LAUNCH_CODE_OPEN = 2;

// DER TAG DES PINS IM LAUNCH-CODE: JJJJMMTT * 10 + 1. Bis 1.20 trug die
// Trink-Aktion nur die 1, und "Nachholen" an einem Pin von gestern zaehlte
// auf der Uhr ein Glas fuer heute (Audit N5). Die Uhr vergleicht den Tag mit
// ihrem eigenen (src/c/drinktervall.c). Passt in einen uint32.
function drunkCode(epoch) {
  return parseInt(dayKey(new Date(epoch * 1000)), 10) * 10 + LAUNCH_CODE_DRUNK;
}

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
  if (!s_clay) s_clay = new Clay(clayConfig(getLang()), clayConfig.custom, { autoHandleEvents: false });
  return s_clay;
}

function loadStore() {
  try { return JSON.parse(localStorage.getItem(STORE_KEY)) || {}; } catch (e) { meldeFehler('Pins lesen', e); return {}; }
}
function saveStore(store) {
  try { localStorage.setItem(STORE_KEY, JSON.stringify(store)); } catch (e) { meldeFehler('Pins speichern', e); }
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
  if (look.action) actions.push({ title: look.action, type: 'openWatchApp', launchCode: drunkCode(epoch) });
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

// Ein Aufruf der REST-API, der IMMER genau einmal zurueckmeldet. Bis 1.20
// stand hier kein try: verweigert die Telefon-App der Watchapp das Netz
// (Boulder: Standard), wirft schon xhr.open - und kein Pin ging hinaus, auch
// keiner ueber die lokale Schnittstelle (Audit M5).
function rest(methode, id, token, body, callback) {
  var fertig = false;
  var melde = function (ok, info) { if (!fertig) { fertig = true; callback(ok, info); } };
  try {
    var xhr = new XMLHttpRequest();
    xhr.onload = function () { melde(this.status >= 200 && this.status < 300, 'REST ' + this.status); };
    xhr.onerror = function () { melde(false, 'REST Netzwerkfehler'); };
    xhr.open(methode, API_URL + id);
    if (body !== null) xhr.setRequestHeader('Content-Type', 'application/json');
    xhr.setRequestHeader('X-User-Token', '' + token);
    xhr.send(body);
  } catch (e) {
    meldeFehler('REST ' + methode, e);
    melde(false, 'REST: ' + e);
  }
}

function insertViaRest(pin, token, callback) {
  rest('PUT', pin.id, token, JSON.stringify(pin), callback);
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
    meldeFehler('Pin lokal', e);
    callback(false, 'lokal: ' + e);
  }
}

// Lokal loeschen: wie insertViaLocal - Core Devices synchron mit der ID,
// die klassische App mit Rueckrufen.
function deleteViaLocal(id, callback) {
  if (typeof Pebble.deleteTimelinePin !== 'function') { callback(false, 'keine lokale API'); return; }
  try {
    if (Pebble.deleteTimelinePin.length >= 3) {
      var done = false;
      var finish = function (ok) { if (!done) { done = true; callback(ok, 'lokal'); } };
      setTimeout(function () { finish(false); }, 5000);
      Pebble.deleteTimelinePin(id, function () { finish(true); }, function () { finish(false); });
    } else {
      Pebble.deleteTimelinePin(id);
      callback(true, 'lokal');
    }
  } catch (e) {
    meldeFehler('Pin lokal loeschen', e);
    callback(false, 'lokal: ' + e);
  }
}

// REST ZUERST, DANN LOKAL. Bis 1.20 galt der REST-Weg als gesetzt, sobald
// ein Token kam - ob er gelang, wurde nicht ausgewertet. Boulder gibt einer
// selbst installierten App ein Ersatz-Token und verweigert ihr das Netz:
// dann ging kein einziger Pin hinaus (Audit M5). Doppelt ankommen kann ein
// Pin so hoechstens mit derselben ID - er wird dann nur ersetzt.
function mitRueckfall(token) {
  return {
    insert: function (pin, cb) {
      insertViaRest(pin, token, function (ok, info) {
        if (ok || typeof Pebble.insertTimelinePin !== 'function') { cb(ok, info); return; }
        console.log('timeline: ' + pin.id + ' per REST fehlgeschlagen (' + info + '), lokal');
        insertViaLocal(pin, cb);
      });
    },
    remove: function (id, cb) {
      rest('DELETE', id, token, null, function (ok, info) {
        if (ok || typeof Pebble.deleteTimelinePin !== 'function') { cb(ok, info); return; }
        console.log('timeline: ' + id + ' per REST nicht geloescht (' + info + '), lokal');
        deleteViaLocal(id, cb);
      });
    }
  };
}

// Pins frueherer Versionen einmalig entfernen. EIN VERSUCH JE PIN, gleich wie
// er ausgeht: bis 1.20 zaehlte nur ein Erfolg herunter - nach einem
// Netzfehler blieb der Vermerk aus, und jede Meldung versuchte es von vorn.
// Die Pins liegen ohnehin seit 1.1 in der Vergangenheit.
function deleteLegacy(wege) {
  var store = loadStore();
  if (store.legacyDeleted) return;
  var left = LEGACY_IDS.length;
  LEGACY_IDS.forEach(function (id) {
    wege.remove(id, function (ok, info) {
      console.log('timeline: alter Pin ' + id + (ok ? ' geloescht' : ' nicht geloescht') + ' (' + info + ')');
      left -= 1;
      if (left === 0) { var s = loadStore(); s.legacyDeleted = true; saveStore(s); }
    });
  });
}

// Der Reihe nach: Pins senden ({ pin, sig }) und veraltete loeschen ({ weg }).
function sendAll(queue, wege) {
  (function next() {
    var item = queue.shift();
    if (!item) { console.log('timeline: fertig'); return; }
    if (item.weg) {
      wege.remove(item.weg, function (ok, info) {
        console.log('timeline: ' + item.weg + ' loeschen -> ' + (ok ? 'ok' : 'fehlgeschlagen') + ' (' + info + ')');
        // Was nicht weg ist, bleibt vermerkt und wird beim naechsten Mal
        // wieder versucht.
        if (ok) { var s = loadStore(); delete s[item.weg]; saveStore(s); }
        next();
      });
      return;
    }
    wege.insert(item.pin, function (ok, info) {
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
  try { localStorage.setItem(LANG_KEY, String(lang)); } catch (e) { meldeFehler('Sprache speichern', e); }
  var store = loadStore();
  Object.keys(store).forEach(function (id) {
    if (store[id] && store[id].sentAt && now - store[id].sentAt > FORGET_AFTER_MS) delete store[id];
  });
  saveStore(store);

  // Ein Pin je Slot; nur senden, was sich geaendert hat oder zu lange liegt.
  var queue = [], wanted = 0, gewollt = {};
  function want(epoch, state, index) {
    wanted += 1;
    // Die Sprache gehoert in die Signatur: ein Pin, der schon draussen ist,
    // hat nach einem Sprachwechsel unveraenderten Zustand und wuerde sonst in
    // der alten Sprache stehen bleiben.
    var id = pinId(epoch, index);
    gewollt[id] = true;
    var sig = state + ':' + epoch + ':' + goal + ':v' + LOOK_VERSION + ':l' + lang;
    var had = store[id];
    if (had && had.sig === sig && now - had.sentAt < RESEND_AFTER_MS) return;
    queue.push({ pin: buildPin(id, epoch, state, index, goal, lang), sig: sig });
  }
  want(msg.NEXT_TIME, 'next', msg.NEXT_INDEX);
  var slots = decodeSlots(msg.SLOTS);
  slots.forEach(function (s, i) {
    if (s.state === SLOT_DRUNK) want(s.time, 'drunk', i);
    else if (s.state === SLOT_MISSED) want(s.time, 'missed', i);
  });
  var neu = queue.length;

  // VERALTETE PINS MIT TRINK-AKTION WEG (Audit M4). Die IDs haengen am
  // Slot-Index: sinkt das Soll, kommt fuer die oberen Slots keine Meldung
  // mehr, und ihr "Getrunken"/"Nachholen" zaehlte auf der Uhr weiter ein
  // Glas. Ebenso die naechste Erinnerung von heute, die keine mehr ist
  // (Tagesziel erreicht). Nur Pins von heute, nur mit Aktion: getrunkene
  // bleiben als Verlauf stehen.
  var heute = 'drinktervall-' + dayKey(slots.length ? new Date(slots[0].time * 1000) : new Date(now)) + '-';
  Object.keys(store).forEach(function (id) {
    if (id.indexOf(heute) !== 0 || gewollt[id] || !store[id] || typeof store[id].sig !== 'string') return;
    if (/^(next|missed):/.test(store[id].sig)) queue.push({ weg: id });
  });
  console.log('timeline: ' + neu + ' von ' + wanted + ' Pins zu senden, ' + (queue.length - neu) + ' zu loeschen');

  var lokal = { insert: insertViaLocal, remove: deleteViaLocal };
  var useLocal = function (reason) {
    if (queue.length === 0) return;
    if (typeof Pebble.insertTimelinePin === 'function') {
      console.log('timeline: ' + reason + ', nutze lokale API');
      sendAll(queue, lokal);
    } else {
      console.log('timeline: ' + reason + ', keine lokale API - Pins uebersprungen');
    }
  };
  if (typeof Pebble.getTimelineToken !== 'function') { useLocal('kein getTimelineToken'); return; }
  Pebble.getTimelineToken(function (token) {
    var wege = mitRueckfall(token);
    deleteLegacy(wege);
    if (queue.length) sendAll(queue, wege);
  }, function (error) {
    useLocal('kein Token (' + error + ')');
  });
}

Pebble.addEventListener('appmessage', function (e) {
  var p = e.payload;
  // DIE UHR FRAGT NACH DER ZEIT, wenn sie hinter ihrem gemerkten Tag steht:
  // ob sie jetzt falsch geht (Neustart) oder vorher falsch ging, weiss nur das
  // Telefon (src/c/schedule.c). Die Antwort traegt nur die Zeit; entscheiden
  // tut die Uhr.
  if (p.UHRZEIT !== undefined) {
    var jetzt = Math.floor(Date.now() / 1000);
    Pebble.sendAppMessage({ UHRZEIT: jetzt },
      function () { console.log('Zeit an die Uhr: ' + jetzt + ' (Uhr: ' + p.UHRZEIT + ')'); },
      function () { console.log('Zeit nicht zugestellt'); });
  }
  adoptWatchSettings(p);
  if (!p.hasOwnProperty('NEXT_TIME')) return;
  pushState(p);
});

Pebble.addEventListener('showConfiguration', function () {
  claySaeubern();
  Pebble.openURL(getClay().generateUrl());
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) return;
  // false = Clay soll nichts von sich aus schicken; wir pruefen die Werte erst
  // und schicken sie dann selbst - alle in EINER Nachricht.
  // Clay WIRFT bei einer Antwort, die kein JSON ist ("CANCELLED" der
  // klassischen App, ein kaputter %-Code) - bis 1.20 ungefangen (Audit N8).
  var dict;
  try {
    dict = getClay().getSettings(e.response, false);
  } catch (err) {
    meldeFehler('Konfig lesen', err);
    return;
  }
  var msg = {};

  if (dict.GLASS_ML !== undefined) {
    var ml = parseInt(dict.GLASS_ML.value, 10);
    if (isFinite(ml) && ml >= GLASS_MIN && ml <= GLASS_MAX) {
      try { localStorage.setItem(GLASS_KEY, String(ml)); } catch (err) { meldeFehler('Glasgroesse speichern', err); }
      msg.GLASS_ML = ml;
    } else {
      console.log('Konfig: ungueltige Glasgroesse ' + dict.GLASS_ML.value);
    }
  }
  if (dict.ANIMATION !== undefined) {
    var on = truthy(dict.ANIMATION.value) ? 1 : 0;
    try { localStorage.setItem(ANIM_KEY, String(on)); } catch (err) { meldeFehler('Animation speichern', err); }
    msg.ANIMATION = on;
  }
  if (dict.TARGET !== undefined) {
    var n = parseInt(dict.TARGET.value, 10);
    if (isFinite(n) && n >= TARGET_MIN && n <= TARGET_MAX) {
      try { localStorage.setItem(TARGET_KEY, String(n)); } catch (err) { meldeFehler('Soll speichern', err); }
      msg.TARGET = n;
    } else {
      console.log('Konfig: ungueltiges Soll ' + dict.TARGET.value + ' - verworfen');
    }
  }
  if (dict.CUSTOM_N !== undefined) {
    msg.CUSTOM = customText(dict);
    try { localStorage.setItem(CUSTOM_KEY, msg.CUSTOM); } catch (err) { meldeFehler('Eigene Getraenke speichern', err); }
  }
  if (dict.COFFEE_ON !== undefined) {
    msg.COFFEE = coffeeBytes(dict);
    try { localStorage.setItem(COFFEE_KEY, JSON.stringify(msg.COFFEE)); } catch (err) { meldeFehler('Kaffeeplan speichern', err); }
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
  // gilt, was die Uhr hat, und die Antwort auf die Anfrage bringt es hierher.
  var msg = { REQUEST: 1 };
  var mitWerten = pending();
  if (mitWerten) {
    var target = getTarget();
    if (target !== null) msg.TARGET = target;
    var glass = getGlassMl();
    if (glass !== null) msg.GLASS_ML = glass;
    var anim = getAnimation();
    if (anim !== null) msg.ANIMATION = anim;
    var coffee = getCoffee();
    if (coffee !== null) msg.COFFEE = coffee;
    var custom = getCustom();
    if (custom !== null) msg.CUSTOM = custom;
  }
  Pebble.sendAppMessage(msg,
    function () { if (mitWerten) setPending(false); },
    function () { console.log('AppMessage: Anfrage fehlgeschlagen'); });
});
