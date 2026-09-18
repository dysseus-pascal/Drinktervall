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
// Zweisprachig wie die App selbst. Welche Sprache gilt, sagt die UHR per
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
    submit: 'Speichern'
  }
];

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
    { type: 'submit', defaultValue: t.submit }
  ];
};
