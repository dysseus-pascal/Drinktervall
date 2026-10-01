// Konfigurationsseite fuer die Telefon-App (Clay).
//
// Drei Knoepfe: wie viele Glaeser der Tagesplan vorsieht (MESSAGE_KEY_TARGET),
// wie viel in ein Glas geht (GLASS_ML) und ob die Trink-Animation gezeigt wird
// (ANIMATION). Die Uhr bekommt sie per AppMessage und rechnet Plan,
// Erinnerungen und Pegel daraus aus - siehe src/c/schedule.c.
//
// Die Ruhezeit steht hier NICHT als Schalter, obwohl die App sie beachtet: sie
// ist eine Einstellung der Uhr, und zwei Schalter fuer dieselbe Sache waeren
// einer zu viel. Auf der Seite steht nur, dass sie gilt.
//
// In fuenf Sprachen wie die App selbst (Reihenfolge wie StringLang in
// src/c/strings.h: en, de, fr, it, es). Welche Sprache gilt, sagt die UHR per
// MESSAGE_KEY_LANG; index.js merkt sich den Wert und reicht ihn hier herein.
// Das Telefon kann die Uhrsprache nicht von sich aus erfahren, deshalb steht
// vor dem allerersten Abgleich Englisch da.

// Tagesfenster wie in src/c/config.h. Steht hier nur, um den Abstand zwischen
// zwei Erinnerungen anzuschreiben - geaendert wird es dort.
var START_HOUR = 8, END_HOUR = 20;
var MIN = 4, MAX = 16, DEFAULT = 8;

// Glasgroesse. Gebraucht wird sie nur, wenn eine Companion-App das getrunkene
// Wasser in eine Gesundheitsakte eintraegt; auf der Uhr wird weiter in
// Glaesern gezaehlt. 3 dl ist ein gewoehnliches Trinkglas.
var GLASS_DEFAULT = 300;
var GLASS_SIZES = [100, 150, 200, 250, 300, 400, 500, 750, 1000];

var TEXT = [
  {
    heading: 'Drinktervall',
    intro: 'The watch reminds you to drink, spread evenly over the day ' +
           '(' + START_HOUR + ':00 to ' + END_HOUR + ':00).',
    section: 'Daily plan',
    label: 'Glasses per day',
    note: 'Each reminder is shifted by up to ten minutes so it never arrives ' +
          'at exactly the same time every day. Drank more than planned? The ' +
          'bottom button on the watch raises today\u2019s goal without ' +
          'touching this setting.',
    glasses: function (n) { return n + ' glasses'; },
    glassSection: 'Glass',
    glassLabel: 'How much fits in your glass',
    glassNote: 'Only needed to pass the water you drank on to a health record. ' +
               'The watch keeps counting in glasses either way.',
    fxSection: 'On the watch',
    fxLabel: 'Drinking animation',
    fxNote: 'The full-screen glass that fills up after every logged drink. ' +
            'Switched off, the level on the main screen simply rises instead ' +
            '— nothing is counted differently.',
    quietNote: 'Quiet Time is obeyed: while it is on, a reminder still ' +
               'appears, it just does not buzz and does not light the ' +
               'screen. That follows the watch’s own setting, so there ' +
               'is nothing to switch here.',
    everyHour: 'every hour',
    everyHours: function (h) { return 'every ' + h + ' hours'; },
    everyMinutes: function (m) { return 'every ' + m + ' minutes'; },
    everyHm: function (h, m) { return 'every ' + h + ' h ' + m + ' min'; },
    coffeeSection: 'Coffee',
    coffeeOn: 'Coffee reminders',
    coffeeCount: 'How many?',
    coffeeSlot: 'Coffee',
    coffeeTime: 'When',
    coffeeType: 'What',
    coffeeTypes: ['Espresso', 'Coffee', 'Tea', 'Energy drink'],
    coffeeMilk: 'Milk',
    coffeeSugar: 'Sugar',
    customSection: 'Own drinks',
    customCount: 'How many?',
    customSlot: 'Drink',
    customName: 'Name',
    customNamePh: 'e.g. Smoothie',
    customKcal: 'kcal',
    customMg: 'Caffeine (mg)',
    customRemind: 'Reminder',
    customTime: 'When',
    submit: 'Save'
  },
  {
    heading: 'Drinktervall',
    intro: 'Die Uhr erinnert ans Trinken, gleichmässig über den Tag verteilt ' +
           '(' + START_HOUR + ' bis ' + END_HOUR + ' Uhr).',
    section: 'Tagesplan',
    label: 'Gläser pro Tag',
    note: 'Jede Erinnerung wird um bis zu zehn Minuten verschoben, damit sie ' +
          'nicht jeden Tag auf die Minute gleich kommt. Mehr getrunken als ' +
          'geplant? Die untere Taste auf der Uhr erhöht das heutige Ziel, ' +
          'ohne diese Einstellung anzufassen.',
    glasses: function (n) { return n + ' Gläser'; },
    glassSection: 'Glas',
    glassLabel: 'Wie viel in dein Glas geht',
    glassNote: 'Wird nur gebraucht, um getrunkenes Wasser an eine ' +
               'Gesundheitsakte weiterzureichen. Gezählt wird auf der Uhr so ' +
               'oder so in Gläsern.',
    fxSection: 'Auf der Uhr',
    fxLabel: 'Trink-Animation',
    fxNote: 'Das formatfüllende Glas, das sich nach jedem eingetragenen Glas ' +
            'füllt. Abgeschaltet steigt stattdessen einfach der Pegel auf dem ' +
            'Hauptscreen — gezählt wird deswegen nichts anders.',
    quietNote: 'Die Ruhezeit gilt: solange sie läuft, erscheint eine ' +
               'Erinnerung zwar, sie summt aber nicht und macht kein Licht. ' +
               'Das richtet sich nach der Einstellung der Uhr, hier ist ' +
               'dafür nichts zu schalten.',
    everyHour: 'jede Stunde',
    everyHours: function (h) { return 'alle ' + h + ' Stunden'; },
    everyMinutes: function (m) { return 'alle ' + m + ' Minuten'; },
    everyHm: function (h, m) { return 'alle ' + h + ' Std. ' + m + ' Min.'; },
    coffeeSection: 'Kaffee',
    coffeeOn: 'Kaffee-Erinnerungen',
    coffeeCount: 'Wie viele?',
    coffeeSlot: 'Kaffee',
    coffeeTime: 'Wann',
    coffeeType: 'Was',
    coffeeTypes: ['Espresso', 'Kaffee', 'Tee', 'Energy-Drink'],
    coffeeMilk: 'Milch',
    coffeeSugar: 'Zucker',
    customSection: 'Eigene Getränke',
    customCount: 'Wie viele?',
    customSlot: 'Getränk',
    customName: 'Name',
    customNamePh: 'z. B. Smoothie',
    customKcal: 'kcal',
    customMg: 'Koffein (mg)',
    customRemind: 'Erinnerung',
    customTime: 'Wann',
    submit: 'Speichern'
  },
  {
    heading: 'Drinktervall',
    intro: 'La montre vous rappelle de boire, à intervalles réguliers sur la ' +
           'journée (de ' + START_HOUR + ' h à ' + END_HOUR + ' h).',
    section: 'Plan du jour',
    label: 'Verres par jour',
    note: 'Chaque rappel est décalé de dix minutes au plus, pour ne pas ' +
          'tomber chaque jour à la même minute. Bu plus que prévu ? Le ' +
          'bouton du bas de la montre relève l’objectif du jour sans ' +
          'toucher à ce réglage.',
    glasses: function (n) { return n + ' verres'; },
    glassSection: 'Verre',
    glassLabel: 'Contenance de votre verre',
    glassNote: 'Ne sert qu’à transmettre l’eau bue à un dossier de ' +
               'santé. La montre compte de toute façon en verres.',
    fxSection: 'Sur la montre',
    fxLabel: 'Animation de boisson',
    fxNote: 'Le verre plein écran qui se remplit après chaque verre noté. ' +
            'Désactivée, le niveau monte simplement sur l’écran ' +
            'principal — rien n’est compté autrement.',
    quietNote: 'Le mode silencieux (Quiet Time) est respecté : pendant ' +
               'ce temps, un rappel apparaît quand même, mais sans vibrer ni ' +
               'allumer l’écran. C’est le réglage de la montre qui ' +
               'compte, il n’y a donc rien à régler ici.',
    everyHour: 'toutes les heures',
    everyHours: function (h) { return 'toutes les ' + h + ' heures'; },
    everyMinutes: function (m) { return 'toutes les ' + m + ' minutes'; },
    everyHm: function (h, m) { return 'toutes les ' + h + ' h ' + m + ' min'; },
    coffeeSection: 'Café',
    coffeeOn: 'Rappels café',
    coffeeCount: 'Combien\u00a0?',
    coffeeSlot: 'Café',
    coffeeTime: 'Quand',
    coffeeType: 'Quoi',
    coffeeTypes: ['Espresso', 'Café', 'Thé', 'Boisson énergisante'],
    coffeeMilk: 'Lait',
    coffeeSugar: 'Sucre',
    customSection: 'Boissons perso',
    customCount: 'Combien\u00a0?',
    customSlot: 'Boisson',
    customName: 'Nom',
    customNamePh: 'p. ex. smoothie',
    customKcal: 'kcal',
    customMg: 'Caféine (mg)',
    customRemind: 'Rappel',
    customTime: 'Quand',
    submit: 'Enregistrer'
  },
  {
    heading: 'Drinktervall',
    intro: 'L’orologio ti ricorda di bere, a intervalli regolari durante ' +
           'la giornata (dalle ' + START_HOUR + ' alle ' + END_HOUR + ').',
    section: 'Piano del giorno',
    label: 'Bicchieri al giorno',
    note: 'Ogni promemoria viene spostato fino a dieci minuti, così non ' +
          'arriva ogni giorno allo stesso minuto. Hai bevuto più del previsto? ' +
          'Il tasto in basso sull’orologio alza l’obiettivo di oggi ' +
          'senza toccare questa impostazione.',
    glasses: function (n) { return n + ' bicchieri'; },
    glassSection: 'Bicchiere',
    glassLabel: 'Quanto contiene il tuo bicchiere',
    glassNote: 'Serve solo per passare l’acqua bevuta a una cartella ' +
               'sanitaria. Sull’orologio si contano comunque i bicchieri.',
    fxSection: 'Sull’orologio',
    fxLabel: 'Animazione del bicchiere',
    fxNote: 'Il bicchiere a tutto schermo che si riempie dopo ogni bicchiere ' +
            'registrato. Se è spenta, sale semplicemente il livello nella ' +
            'schermata principale — non cambia nulla nel conteggio.',
    quietNote: 'La modalità silenziosa (Quiet Time) viene rispettata: mentre ' +
               'è attiva, il promemoria compare comunque, ma non vibra e non ' +
               'accende lo schermo. Vale l’impostazione dell’orologio, ' +
               'qui non c’è nulla da attivare.',
    everyHour: 'ogni ora',
    everyHours: function (h) { return 'ogni ' + h + ' ore'; },
    everyMinutes: function (m) { return 'ogni ' + m + ' minuti'; },
    everyHm: function (h, m) { return 'ogni ' + h + ' h ' + m + ' min'; },
    coffeeSection: 'Caffè',
    coffeeOn: 'Promemoria caffè',
    coffeeCount: 'Quanti?',
    coffeeSlot: 'Caffè',
    coffeeTime: 'Quando',
    coffeeType: 'Cosa',
    coffeeTypes: ['Espresso', 'Caffè', 'Tè', 'Energy drink'],
    coffeeMilk: 'Latte',
    coffeeSugar: 'Zucchero',
    customSection: 'Bevande tue',
    customCount: 'Quante?',
    customSlot: 'Bevanda',
    customName: 'Nome',
    customNamePh: 'es. smoothie',
    customKcal: 'kcal',
    customMg: 'Caffeina (mg)',
    customRemind: 'Promemoria',
    customTime: 'Quando',
    submit: 'Salva'
  },
  {
    heading: 'Drinktervall',
    intro: 'El reloj te recuerda beber, repartido por igual a lo largo del ' +
           'día (de ' + START_HOUR + ':00 a ' + END_HOUR + ':00).',
    section: 'Plan del día',
    label: 'Vasos al día',
    note: 'Cada aviso se desplaza hasta diez minutos para que no llegue cada ' +
          'día al mismo minuto. ¿Has bebido más de lo previsto? El botón ' +
          'inferior del reloj sube la meta de hoy sin tocar este ajuste.',
    glasses: function (n) { return n + ' vasos'; },
    glassSection: 'Vaso',
    glassLabel: 'Cuánto cabe en tu vaso',
    glassNote: 'Solo hace falta para pasar el agua bebida a un registro de ' +
               'salud. El reloj sigue contando en vasos igualmente.',
    fxSection: 'En el reloj',
    fxLabel: 'Animación al beber',
    fxNote: 'El vaso a pantalla completa que se llena tras cada vaso ' +
            'anotado. Si está desactivada, simplemente sube el nivel en la ' +
            'pantalla principal — no se cuenta nada de otra forma.',
    quietNote: 'Se respeta el modo silencio (Quiet Time): mientras está ' +
               'activo, el aviso aparece igualmente, pero no vibra ni ' +
               'enciende la pantalla. Manda el ajuste del reloj, así que ' +
               'aquí no hay nada que activar.',
    everyHour: 'cada hora',
    everyHours: function (h) { return 'cada ' + h + ' horas'; },
    everyMinutes: function (m) { return 'cada ' + m + ' minutos'; },
    everyHm: function (h, m) { return 'cada ' + h + ' h ' + m + ' min'; },
    coffeeSection: 'Café',
    coffeeOn: 'Avisos de café',
    coffeeCount: '¿Cuántos?',
    coffeeSlot: 'Café',
    coffeeTime: 'Cuándo',
    coffeeType: 'Qué',
    coffeeTypes: ['Espresso', 'Café', 'Té', 'Bebida energética'],
    coffeeMilk: 'Leche',
    coffeeSugar: 'Azúcar',
    customSection: 'Bebidas propias',
    customCount: '¿Cuántas?',
    customSlot: 'Bebida',
    customName: 'Nombre',
    customNamePh: 'p. ej. batido',
    customKcal: 'kcal',
    customMg: 'Cafeína (mg)',
    customRemind: 'Aviso',
    customTime: 'Cuándo',
    submit: 'Guardar'
  }
];

// Kaffeezeiten: so viele Plaetze wie DT_COFFEE_MAX in src/c/config.h. Die
// Sorten in derselben Reihenfolge wie CoffeeKind in src/c/coffee.h - die
// Nummer geht an die Uhr und von dort an die Companion-App.
var COFFEE_MAX = 4;
var COFFEE_DEFAULT_TIMES = [420, 600, 840, 960];   // 07:00, 10:00, 14:00, 16:00
var COFFEE_ENERGY = '3';                            // Milch zu allem ausser dem Energy-Drink

// Viertelstundenraster von 5 bis 23 Uhr: Kaffee hat feste Gewohnheiten, aber
// nicht nur zur vollen und halben Stunde.
function coffeeTimes() {
  var out = [];
  for (var m = 5 * 60; m < 23 * 60; m += 15) {
    var h = Math.floor(m / 60), mm = m % 60;
    out.push({ label: (h < 10 ? '0' : '') + h + ':' + (mm < 10 ? '0' : '') + mm, value: String(m) });
  }
  return out;
}

function coffeeSlot(t, n) {
  return {
    type: 'section',
    // Die Kennung an der Ueberschrift, nicht an der Section: Clay verwirft
    // die id einer Section (siehe custom unten).
    items: [
      { type: 'heading', id: 'cofhead' + n, defaultValue: t.coffeeSlot + ' ' + n },
      {
        type: 'select', messageKey: 'COFFEE_TIME' + n, label: t.coffeeTime,
        defaultValue: String(COFFEE_DEFAULT_TIMES[n - 1]), options: coffeeTimes()
      },
      {
        type: 'select', messageKey: 'COFFEE_TYPE' + n, label: t.coffeeType,
        defaultValue: '1',
        options: t.coffeeTypes.map(function (name, i) { return { label: name, value: String(i) }; })
      },
      { type: 'toggle', messageKey: 'COFFEE_MILK' + n, label: t.coffeeMilk, defaultValue: false },
      { type: 'toggle', messageKey: 'COFFEE_SUGAR' + n, label: t.coffeeSugar, defaultValue: false }
    ]
  };
}

// Eigene Getraenke: so viele wie DT_CUSTOM_MAX in src/c/config.h. Namen
// kuerzer als DT_CUSTOM_NAME, die Uhr schneidet sonst ab.
var CUSTOM_MAX = 3;

function customSlot(t, n) {
  return {
    type: 'section',
    items: [
      { type: 'heading', id: 'cushead' + n, defaultValue: t.customSlot + ' ' + n },
      { type: 'input', messageKey: 'CUSTOM_NAME' + n, label: t.customName,
        attributes: { placeholder: t.customNamePh, limit: 15 } },
      { type: 'input', messageKey: 'CUSTOM_KCAL' + n, label: t.customKcal,
        attributes: { type: 'number', min: 0, max: 2000, placeholder: '0' } },
      { type: 'input', messageKey: 'CUSTOM_MG' + n, label: t.customMg,
        attributes: { type: 'number', min: 0, max: 1000, placeholder: '0' } },
      { type: 'toggle', messageKey: 'CUSTOM_REMIND' + n, label: t.customRemind, defaultValue: false },
      {
        type: 'select', messageKey: 'CUSTOM_TIME' + n, label: t.customTime,
        defaultValue: '960', options: coffeeTimes()
      }
    ]
  };
}

// "8 Gläser · alle 90 Minuten" - der Abstand steht dabei, weil die blosse Zahl
// nichts darueber sagt, wie oft es klopft.
function options(t) {
  var out = [];
  for (var n = MIN; n <= MAX; n++) {
    var min = Math.floor((END_HOUR - START_HOUR) * 60 / n);
    var how;
    if (min % 60 === 0) how = (min === 60) ? t.everyHour : t.everyHours(min / 60);
    else if (min > 120) how = t.everyHm(Math.floor(min / 60), min % 60);
    else how = t.everyMinutes(min);
    out.push({ label: t.glasses(n) + ' \u00b7 ' + how, value: String(n) });
  }
  return out;
}

module.exports = function (lang) {
  var t = TEXT[lang] || TEXT[0];
  return [
    { type: 'heading', defaultValue: t.heading },
    { type: 'text', defaultValue: t.intro },
    {
      type: 'section',
      items: [
        { type: 'heading', defaultValue: t.section },
        {
          type: 'select',
          messageKey: 'TARGET',
          label: t.label,
          defaultValue: String(DEFAULT),
          options: options(t)
        },
        { type: 'text', defaultValue: t.note }
      ]
    },
    {
      type: 'section',
      items: [
        { type: 'heading', defaultValue: t.glassSection },
        {
          type: 'select',
          messageKey: 'GLASS_ML',
          label: t.glassLabel,
          defaultValue: String(GLASS_DEFAULT),
          options: GLASS_SIZES.map(function (ml) {
            // Deziliter lesen sich bei runden Werten besser als Milliliter -
            // 3 dl statt 300 ml. Krumme bleiben in ml, ein voller Liter wird
            // einer.
            var label;
            if (ml === 1000) label = '1 l';
            else if (ml % 100 === 0) label = (ml / 100) + ' dl';
            else label = ml + ' ml';
            return { label: label, value: String(ml) };
          })
        },
        { type: 'text', defaultValue: t.glassNote }
      ]
    },
    {
      type: 'section',
      items: [
        { type: 'heading', defaultValue: t.fxSection },
        {
          type: 'toggle',
          messageKey: 'ANIMATION',
          label: t.fxLabel,
          defaultValue: true
        },
        { type: 'text', defaultValue: t.fxNote },
        { type: 'text', defaultValue: t.quietNote }
      ]
    },
    {
      type: 'section',
      items: [
        { type: 'heading', defaultValue: t.coffeeSection },
        { type: 'toggle', messageKey: 'COFFEE_ON', label: t.coffeeOn, defaultValue: false },
        {
          type: 'select', messageKey: 'COFFEE_N', label: t.coffeeCount, defaultValue: '2',
          options: [1, 2, 3, 4].map(function (n) { return { label: String(n), value: String(n) }; })
        }
      ]
    }
  ].concat([1, 2, 3, 4].map(function (n) { return coffeeSlot(t, n); }))
   .concat([{
     type: 'section',
     items: [
       { type: 'heading', defaultValue: t.customSection },
       {
         type: 'select', messageKey: 'CUSTOM_N', label: t.customCount, defaultValue: '0',
         options: [0, 1, 2, 3].map(function (n) { return { label: String(n), value: String(n) }; })
       }
     ]
   }])
   .concat([1, 2, 3].map(function (n) { return customSlot(t, n); }))
   .concat([{ type: 'submit', defaultValue: t.submit }]);
};

module.exports.COFFEE_MAX = COFFEE_MAX;
module.exports.CUSTOM_MAX = CUSTOM_MAX;

/**
 * Laeuft IN DER KONFIGSEITE, nicht hier: Clay reicht diese Funktion in die
 * Webansicht weiter. Sie darf deshalb nichts von aussen benutzen.
 *
 * Erst der Schalter, dann die Anzahl, dann so viele Kaffees - und Milch nur
 * beim Kaffee: beim Espresso gibt es keine, beim Macchiato gehoert sie dazu.
 */
module.exports.custom = function () {
  var clayConfig = this;
  var MAX = 4;

  // Clay legt Sections ohne id an; der Kasten wird ueber seine Ueberschrift
  // gefunden und als Ganzes verborgen - sonst bliebe ein leerer grauer Rahmen.
  function box(i) {
    var head = clayConfig.getItemById('cofhead' + i);
    if (!head || !head.$element || !head.$element[0]) return null;
    var el = head.$element[0];
    return el.closest ? el.closest('.section') : null;
  }

  function on(v) { return v === true || v === 1 || v === '1' || v === 'true'; }

  // Eigene Getraenke: so viele Kaesten wie gewaehlt.
  function cusbox(i) {
    var head = clayConfig.getItemById('cushead' + i);
    if (!head || !head.$element || !head.$element[0]) return null;
    var el = head.$element[0];
    return el.closest ? el.closest('.section') : null;
  }

  function apply() {
    var cn = clayConfig.getItemByMessageKey('CUSTOM_N');
    var eigene = cn ? parseInt(cn.get(), 10) || 0 : 0;
    for (var c = 1; c <= 3; c++) {
      var cb = cusbox(c);
      if (cb) { if (c <= eigene) cb.classList.remove('hide'); else cb.classList.add('hide'); }
      // Die Zeit nur, wenn die Erinnerung an ist.
      var erinnern = clayConfig.getItemByMessageKey('CUSTOM_REMIND' + c);
      var zeit = clayConfig.getItemByMessageKey('CUSTOM_TIME' + c);
      if (zeit) { if (erinnern && on(erinnern.get())) zeit.show(); else zeit.hide(); }
    }
    var schalter = clayConfig.getItemByMessageKey('COFFEE_ON');
    var aktiv = schalter ? on(schalter.get()) : false;
    var anzahl = clayConfig.getItemByMessageKey('COFFEE_N');
    if (anzahl) { if (aktiv) anzahl.show(); else anzahl.hide(); }
    var n = anzahl ? parseInt(anzahl.get(), 10) : MAX;
    if (!n || n < 1 || n > MAX) n = MAX;
    for (var i = 1; i <= MAX; i++) {
      var zeigen = aktiv && i <= n;
      var b = box(i);
      if (b) {
        if (zeigen) b.classList.remove('hide'); else b.classList.add('hide');
      }
      var art = clayConfig.getItemByMessageKey('COFFEE_TYPE' + i);
      var milch = clayConfig.getItemByMessageKey('COFFEE_MILK' + i);
      // Milch zu allem ausser dem Energy-Drink (Sorte 3).
      if (milch) { if (art && art.get() !== '3') milch.show(); else milch.hide(); }
    }
  }

  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function () {
    apply();
    var keys = ['CUSTOM_N', 'CUSTOM_REMIND1', 'CUSTOM_REMIND2', 'CUSTOM_REMIND3', 'COFFEE_ON', 'COFFEE_N', 'COFFEE_TYPE1', 'COFFEE_TYPE2', 'COFFEE_TYPE3', 'COFFEE_TYPE4'];
    for (var k = 0; k < keys.length; k++) {
      var it = clayConfig.getItemByMessageKey(keys[k]);
      if (it) it.on('change', apply);
    }
  });
};
