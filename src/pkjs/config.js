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
    submit: 'Guardar'
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
