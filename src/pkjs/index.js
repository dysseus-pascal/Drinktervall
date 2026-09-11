// Drinktervall - Telefonseite: haelt genau einen Timeline-Pin fuer die naechste
// Erinnerung. Die Watch schickt Zeitpunkt und Slot per AppMessage
// (src/c/phone.c), damit die Zeitberechnung samt Versatz nur in C lebt.
//
// Uebertragung: zuerst der bewaehrte Weg ueber Pebble.getTimelineToken und
// die Rebble-REST-API (funktioniert mit der neuen Pebble-App, verifiziert
// 2026-09-10). Nur wenn kein Token zu bekommen ist, wird die lokale
// Schnittstelle Pebble.insertTimelinePin versucht.

var API_URL = 'https://timeline-api.rebble.io/v1/user/pins/';
// Muss zu DT_COLOR_PRIMARY in src/c/theme.h passen (handgepflegte Kopie)
var PIN_COLOR = '#0055FF';
// Ein einziger Pin, der bei jeder Erinnerung auf die naechste Zeit wandert
var PIN_ID = 'drinktervall-next';
var OLD_PIN_ID = 'aquatakt-next';   // Pin-ID vor der Umbenennung; wird einmal geloescht
var STORE_KEY = 'drinktervall_next_v1';       // zuletzt gesendete Zeit + Zeitpunkt
var RESEND_AFTER_MS = 12 * 3600 * 1000;   // unveraenderten Pin nach 12 h erneut senden

var LAUNCH_CODE_DRUNK = 1;
var LAUNCH_CODE_OPEN = 2;

function loadStore() {
  try { return JSON.parse(localStorage.getItem(STORE_KEY)) || {}; } catch (e) { return {}; }
}
function saveStore(store) {
  try { localStorage.setItem(STORE_KEY, JSON.stringify(store)); } catch (e) {}
}

function buildPin(when, index, glasses) {
  return {
    id: PIN_ID,
    time: when.toISOString(),
    layout: {
      type: 'genericPin',
      title: 'Glas Wasser ' + (index + 1) + ' von ' + glasses,
      subtitle: 'Drinktervall',
      body: 'Zeit für ein Glas Wasser. Tagesziel: ' + glasses + ' Gläser.',
      tinyIcon: 'system://images/NOTIFICATION_REMINDER',
      backgroundColor: PIN_COLOR,
      foregroundColor: '#FFFFFF'
    },
    actions: [
      { title: 'Getrunken', type: 'openWatchApp', launchCode: LAUNCH_CODE_DRUNK },
      { title: 'App öffnen', type: 'openWatchApp', launchCode: LAUNCH_CODE_OPEN }
    ]
  };
}

function hasLocalApi() {
  return typeof Pebble.insertTimelinePin === 'function';
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

// Den Pin aus der Zeit vor der Umbenennung einmalig entfernen
function deleteOldPin(token) {
  if (loadStore().oldDeleted) return;
  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    console.log('timeline: alter Pin ' + OLD_PIN_ID + ' geloescht (' + this.status + ')');
    var s = loadStore(); s.oldDeleted = true; saveStore(s);
  };
  xhr.open('DELETE', API_URL + OLD_PIN_ID);
  xhr.setRequestHeader('X-User-Token', '' + token);
  xhr.send();
}

function pushNext(msg) {
  var pin = buildPin(new Date(msg.NEXT_TIME * 1000), msg.NEXT_INDEX, msg.GLASSES);
  var store = loadStore();
  if (store.time === pin.time && Date.now() - store.sentAt < RESEND_AFTER_MS) {
    console.log('timeline: Pin aktuell (' + pin.time + ')');
    return;
  }
  var done = function (ok, info) {
    console.log('timeline: ' + pin.id + ' ' + pin.time + ' -> ' + (ok ? 'ok' : 'fehlgeschlagen') + ' (' + info + ')');
    if (ok) { var s = loadStore(); s.time = pin.time; s.sentAt = Date.now(); saveStore(s); }
  };
  var useLocal = function (reason) {
    if (hasLocalApi()) {
      console.log('timeline: ' + reason + ', nutze lokale API');
      insertViaLocal(pin, done);
    } else {
      console.log('timeline: ' + reason + ', keine lokale API - Pin uebersprungen');
    }
  };
  if (typeof Pebble.getTimelineToken !== 'function') { useLocal('kein getTimelineToken'); return; }
  Pebble.getTimelineToken(function (token) {
    deleteOldPin(token);
    insertViaRest(pin, token, done);
  }, function (error) {
    useLocal('kein Token (' + error + ')');
  });
}

Pebble.addEventListener('appmessage', function (e) {
  var p = e.payload;
  if (!p.hasOwnProperty('NEXT_TIME')) return;
  pushNext(p);
});

Pebble.addEventListener('ready', function () {
  Pebble.sendAppMessage({ REQUEST: 1 },
    function () {}, function () { console.log('AppMessage: Anfrage fehlgeschlagen'); });
});
