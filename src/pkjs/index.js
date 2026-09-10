// AquaTakt - Telefonseite: legt fuer heute und morgen je einen Timeline-Pin
// pro Erinnerung an. Die Plan-Konfiguration kommt per AppMessage von der
// Watch (src/c/phone.c), damit sie nur an einer Stelle (config.h) lebt.
//
// Uebertragung: zuerst der bewaehrte Weg ueber Pebble.getTimelineToken und
// die Rebble-REST-API (funktioniert mit der neuen Pebble-App, verifiziert
// 2026-09-10). Nur wenn kein Token zu bekommen ist, wird die lokale
// Schnittstelle Pebble.insertTimelinePin versucht.

var API_URL = 'https://timeline-api.rebble.io/v1/user/pins/';
// Muss zu AT_COLOR_PRIMARY in src/c/theme.h passen (handgepflegte Kopie)
var PIN_COLOR = '#0055FF';
// Angelegte Pin-IDs -> Zeitpunkt des Sendens. Der Schluessel traegt eine
// Version: aendern, wenn alte Eintraege verworfen werden sollen.
var STORE_KEY = 'aquatakt_pins_v2';
var RESEND_AFTER_MS = 12 * 3600 * 1000;   // Pins nach 12 h erneut senden
var FORGET_AFTER_MS = 3 * 86400 * 1000;   // alte Eintraege vergessen

var LAUNCH_CODE_DRUNK = 1;
var LAUNCH_CODE_OPEN = 2;

function pad(n) { return (n < 10 ? '0' : '') + n; }
function dayKey(d) { return '' + d.getFullYear() + pad(d.getMonth() + 1) + pad(d.getDate()); }

function loadStore() {
  try { return JSON.parse(localStorage.getItem(STORE_KEY)) || {}; } catch (e) { return {}; }
}
function saveStore(store) {
  try { localStorage.setItem(STORE_KEY, JSON.stringify(store)); } catch (e) {}
}

function buildPin(id, when, index, cfg) {
  return {
    id: id,
    time: when.toISOString(),
    layout: {
      type: 'genericPin',
      title: 'Glas Wasser ' + (index + 1) + ' von ' + cfg.glasses,
      subtitle: 'AquaTakt',
      body: 'Zeit für ein Glas Wasser. Tagesziel: ' + cfg.glasses + ' Gläser zwischen ' +
            cfg.startHour + ' und ' + (cfg.startHour + cfg.intervalMin * cfg.glasses / 60) + ' Uhr.',
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

// Pins fuer heute und morgen, die noch nie oder vor mehr als 12 h gesendet wurden
function pendingPins(cfg) {
  var store = loadStore();
  var pins = [];
  var now = new Date();
  Object.keys(store).forEach(function (id) {
    if (now.getTime() - store[id] > FORGET_AFTER_MS) delete store[id];
  });
  for (var day = 0; day < 2; day++) {
    var base = new Date(now.getFullYear(), now.getMonth(), now.getDate() + day, 0, 0, 0, 0);
    var key = dayKey(base);
    for (var i = 0; i < cfg.glasses; i++) {
      var id = 'aquatakt-' + key + '-' + (i + 1);
      if (store[id] && now.getTime() - store[id] < RESEND_AFTER_MS) continue;
      var when = new Date(base.getTime() + (cfg.startHour * 60 + i * cfg.intervalMin) * 60000);
      pins.push(buildPin(id, when, i, cfg));
    }
  }
  saveStore(store);
  return pins;
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

function sendAll(queue, insert) {
  var store = loadStore();
  (function next() {
    var pin = queue.shift();
    if (!pin) { saveStore(store); console.log('timeline: fertig'); return; }
    insert(pin, function (ok, info) {
      console.log('timeline: ' + pin.id + ' -> ' + (ok ? 'ok' : 'fehlgeschlagen') + ' (' + info + ')');
      if (ok) store[pin.id] = Date.now();
      next();
    });
  })();
}

function pushPins(cfg) {
  var queue = pendingPins(cfg);
  console.log('timeline: ' + queue.length + ' Pins zu senden');
  if (queue.length === 0) return;
  var useLocal = function (reason) {
    if (hasLocalApi()) {
      console.log('timeline: ' + reason + ', nutze lokale API');
      sendAll(queue, insertViaLocal);
    } else {
      console.log('timeline: ' + reason + ', keine lokale API - Pins uebersprungen');
    }
  };
  if (typeof Pebble.getTimelineToken !== 'function') { useLocal('kein getTimelineToken'); return; }
  Pebble.getTimelineToken(function (token) {
    console.log('timeline: Token erhalten, sende per REST');
    sendAll(queue, function (pin, cb) { insertViaRest(pin, token, cb); });
  }, function (error) {
    useLocal('kein Token (' + error + ')');
  });
}

Pebble.addEventListener('appmessage', function (e) {
  var p = e.payload;
  if (!p.hasOwnProperty('START_HOUR')) return;
  pushPins({
    startHour: p.START_HOUR,
    intervalMin: p.INTERVAL_MIN,
    glasses: p.GLASSES
  });
});

Pebble.addEventListener('ready', function () {
  Pebble.sendAppMessage({ REQUEST: 1 },
    function () {}, function () { console.log('AppMessage: Anfrage fehlgeschlagen'); });
});
