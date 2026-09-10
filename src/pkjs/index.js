// AquaTakt - Telefonseite: legt fuer heute und morgen je einen Timeline-Pin
// pro Erinnerung an. Die Plan-Konfiguration kommt per AppMessage von der
// Watch (src/c/phone.c), damit sie nur an einer Stelle (config.h) lebt.
//
// Pins gehen bevorzugt ueber die lokale Schnittstelle der neuen Pebble-App
// (Pebble.insertTimelinePin, Core Devices), sonst ueber die Rebble-REST-API
// mit Timeline-Token.

var API_URL = 'https://timeline-api.rebble.io/v1/user/pins/';
// Muss zu AT_COLOR_PRIMARY in src/c/theme.h passen (handgepflegte Kopie)
var PIN_COLOR = '#0055FF';
var STORE_KEY = 'aquatakt_pins';   // bereits angelegte Pin-IDs -> Tagesschluessel

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

// Pins fuer heute und morgen, die noch nicht angelegt wurden
function pendingPins(cfg) {
  var store = loadStore();
  var pins = [];
  var now = new Date();
  var yesterday = dayKey(new Date(now.getTime() - 86400000));
  // alte Eintraege vergessen
  Object.keys(store).forEach(function (id) { if (store[id] < yesterday) delete store[id]; });
  for (var day = 0; day < 2; day++) {
    var base = new Date(now.getFullYear(), now.getMonth(), now.getDate() + day, 0, 0, 0, 0);
    var key = dayKey(base);
    for (var i = 0; i < cfg.glasses; i++) {
      var id = 'aquatakt-' + key + '-' + (i + 1);
      if (store[id]) continue;
      var when = new Date(base.getTime() + (cfg.startHour * 60 + i * cfg.intervalMin) * 60000);
      pins.push({ pin: buildPin(id, when, i, cfg), key: key });
    }
  }
  saveStore(store);
  return pins;
}

function hasLocalApi() {
  return typeof Pebble.insertTimelinePin === 'function';
}

// Einen Pin anlegen; callback(ok, info).
// Lokale API: Core Devices nimmt nur den Pin (synchron), die klassische App
// pin/success/failure. REST: PUT mit X-User-Token.
function insertPin(pin, token, callback) {
  if (hasLocalApi()) {
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
    return;
  }
  var xhr = new XMLHttpRequest();
  xhr.onload = function () { callback(this.status >= 200 && this.status < 300, this.status); };
  xhr.onerror = function () { callback(false, 'Netzwerk'); };
  xhr.open('PUT', API_URL + pin.id);
  xhr.setRequestHeader('Content-Type', 'application/json');
  xhr.setRequestHeader('X-User-Token', '' + token);
  xhr.send(JSON.stringify(pin));
}

function pushPins(cfg) {
  var queue = pendingPins(cfg);
  if (queue.length === 0) { console.log('timeline: alle Pins vorhanden'); return; }
  var run = function (token) {
    var store = loadStore();
    (function next() {
      var item = queue.shift();
      if (!item) { saveStore(store); console.log('timeline: fertig'); return; }
      insertPin(item.pin, token, function (ok, info) {
        console.log('timeline: ' + item.pin.id + ' -> ' + (ok ? 'ok' : 'fehlgeschlagen') + ' (' + info + ')');
        if (ok) store[item.pin.id] = item.key;
        next();
      });
    })();
  };
  if (hasLocalApi()) { run(null); return; }
  if (typeof Pebble.getTimelineToken !== 'function') { console.log('timeline: keine Timeline-API'); return; }
  Pebble.getTimelineToken(run, function (error) {
    console.log('timeline: kein Token (' + error + ') - Pins uebersprungen');
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
