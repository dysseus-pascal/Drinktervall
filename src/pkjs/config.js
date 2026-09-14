// Konfigurationsseite fuer die Telefon-App (Clay).
//
// Einziger Knopf: wie viele Glaeser der Tagesplan vorsieht. Die Uhr bekommt
// die Zahl per AppMessage (MESSAGE_KEY_TARGET) und rechnet Plan, Erinnerungen
// und Pegel daraus aus - siehe src/c/schedule.c.
//
// Zweisprachig wie die App selbst. Welche Sprache gilt, sagt die UHR per
// MESSAGE_KEY_LANG; index.js merkt sich den Wert und reicht ihn hier herein.
// Das Telefon kann die Uhrsprache nicht von sich aus erfahren, deshalb steht
// vor dem allerersten Abgleich Englisch da.

// Tagesfenster wie in src/c/config.h. Steht hier nur, um den Abstand zwischen
// zwei Erinnerungen anzuschreiben - geaendert wird es dort.
var START_HOUR = 8, END_HOUR = 20;
var MIN = 4, MAX = 16, DEFAULT = 8;

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
    { type: 'submit', defaultValue: t.submit }
  ];
};
