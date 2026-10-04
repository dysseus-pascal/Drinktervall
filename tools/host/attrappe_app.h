// Nur fuer tools/app_host_test.c: so viel von Fenstern, Tasten, Start und
// Glance, wie drinktervall.c, reminder_window.c, coffee_window.c und
// drink_window.c brauchen. Eingebunden von pebble.h, wenn ATTRAPPE_APP
// gesetzt ist; Layer und Animation kommen aus attrappe_grafik.h.
//
// WIE DIE FIRMWARE (pebbleos applib/ui/window_stack.c, app_glance.c):
//   - window_stack_push ruft load und appear; remove und pop_all rufen
//     disappear und unload - danach ist das Fenster weg.
//   - Eine Taste geht an das oberste Fenster. Dessen Klick-Konfiguration
//     (auch die der Aktionsleiste) laeuft, bevor die erste Taste ankommt.
//   - app_glance_add_slice kopiert den Text und weist eine Scheibe ab, die
//     schon abgelaufen ist; hoechstens acht je Glance.
// Gezeichnet wird hier nicht.
#pragma once

// --- Fenster und Tasten ---
typedef struct Window Window;
typedef void (*WindowHandler)(Window *window);
typedef struct {
  WindowHandler load;
  WindowHandler appear;
  WindowHandler disappear;
  WindowHandler unload;
} WindowHandlers;
typedef enum { BUTTON_ID_BACK = 0, BUTTON_ID_UP, BUTTON_ID_SELECT, BUTTON_ID_DOWN, NUM_BUTTONS } ButtonId;
typedef void *ClickRecognizerRef;
typedef void (*ClickHandler)(ClickRecognizerRef recognizer, void *context);
typedef void (*ClickConfigProvider)(void *context);
Window *window_create(void);
void window_destroy(Window *window);
void window_set_background_color(Window *window, GColor color);
void window_set_window_handlers(Window *window, WindowHandlers handlers);
void window_set_click_config_provider(Window *window, ClickConfigProvider provider);
Layer *window_get_root_layer(const Window *window);
void window_stack_push(Window *window, bool animated);
bool window_stack_remove(Window *window, bool animated);
void window_stack_pop_all(bool animated);
void window_single_click_subscribe(ButtonId button_id, ClickHandler handler);

typedef struct GBitmap GBitmap;
GBitmap *gbitmap_create_with_resource(uint32_t resource_id);
void gbitmap_destroy(GBitmap *bitmap);
#define RESOURCE_ID_ICON_CHECK  1
#define RESOURCE_ID_ICON_SNOOZE 2

typedef struct ActionBarLayer ActionBarLayer;
#define ACTION_BAR_WIDTH 30
ActionBarLayer *action_bar_layer_create(void);
void action_bar_layer_destroy(ActionBarLayer *bar);
void action_bar_layer_set_background_color(ActionBarLayer *bar, GColor color);
void action_bar_layer_set_icon(ActionBarLayer *bar, ButtonId button_id, const GBitmap *icon);
void action_bar_layer_set_click_config_provider(ActionBarLayer *bar, ClickConfigProvider provider);
void action_bar_layer_add_to_window(ActionBarLayer *bar, Window *window);

// --- Text (nur, damit die Zeichenfunktionen uebersetzen) ---
typedef const void *GFont;
#define FONT_KEY_GOTHIC_14                "GOTHIC_14"
#define FONT_KEY_GOTHIC_14_BOLD           "GOTHIC_14_BOLD"
#define FONT_KEY_GOTHIC_18                "GOTHIC_18"
#define FONT_KEY_GOTHIC_18_BOLD           "GOTHIC_18_BOLD"
#define FONT_KEY_GOTHIC_24_BOLD           "GOTHIC_24_BOLD"
#define FONT_KEY_LECO_20_BOLD_NUMBERS     "LECO_20_BOLD_NUMBERS"
#define FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM "LECO_26_BOLD_NUMBERS_AM_PM"
GFont fonts_get_system_font(const char *font_key);
typedef enum { GTextOverflowModeWordWrap, GTextOverflowModeTrailingEllipsis, GTextOverflowModeFill } GTextOverflowMode;
typedef enum { GTextAlignmentLeft, GTextAlignmentCenter, GTextAlignmentRight } GTextAlignment;
typedef struct GTextAttributes GTextAttributes;
void graphics_context_set_text_color(GContext *ctx, GColor color);
void graphics_draw_text(GContext *ctx, const char *text, GFont font, GRect box,
                        GTextOverflowMode overflow, GTextAlignment alignment, GTextAttributes *attributes);
GSize graphics_text_layout_get_content_size(const char *text, GFont font, GRect box,
                                            GTextOverflowMode overflow, GTextAlignment alignment);
typedef enum { GCornerNone = 0 } GCornerMask;
void graphics_fill_rect(GContext *ctx, GRect rect, uint16_t corner_radius, GCornerMask corner_mask);
void clock_copy_time_string(char *buffer, uint8_t size);

// --- Vibration, Licht, Ruhezeit: die Tests zaehlen mit ---
void vibes_short_pulse(void);
void vibes_double_pulse(void);
void light_enable_interaction(void);
bool quiet_time_is_active(void);

// --- Start, Wecker, Ereignisschleife ---
typedef enum {
  APP_LAUNCH_SYSTEM = 0, APP_LAUNCH_USER, APP_LAUNCH_PHONE, APP_LAUNCH_WAKEUP, APP_LAUNCH_WORKER,
  APP_LAUNCH_QUICK_LAUNCH, APP_LAUNCH_TIMELINE_ACTION, APP_LAUNCH_SMARTSTRAP,
} AppLaunchReason;
AppLaunchReason launch_reason(void);
uint32_t launch_get_args(void);
bool wakeup_get_launch_event(WakeupId *wakeup_id, int32_t *cookie);
typedef void (*WakeupHandler)(WakeupId wakeup_id, int32_t cookie);
void wakeup_service_subscribe(WakeupHandler handler);
void app_event_loop(void);

// --- Glance ---
typedef uint32_t PublishedId;
typedef struct AppGlanceReloadSession AppGlanceReloadSession;
typedef struct {
  struct {
    PublishedId icon;
    const char *subtitle_template_string;
  } layout;
  time_t expiration_time;
} AppGlanceSlice;
#define APP_GLANCE_SLICE_DEFAULT_ICON  ((PublishedId)0)
#define APP_GLANCE_SLICE_NO_EXPIRATION ((time_t)0)
typedef enum {
  APP_GLANCE_RESULT_SUCCESS = 0,
  APP_GLANCE_RESULT_SLICE_CAPACITY_EXCEEDED = 1 << 3,
  APP_GLANCE_RESULT_EXPIRES_IN_THE_PAST = 1 << 4,
} AppGlanceResult;
typedef void (*AppGlanceReloadCallback)(AppGlanceReloadSession *session, size_t limit, void *context);
void app_glance_reload(AppGlanceReloadCallback callback, void *context);
AppGlanceResult app_glance_add_slice(AppGlanceReloadSession *session, AppGlanceSlice slice);

// --- Was der Test damit tut ---
// Wie die App startet; die Ereignisschleife spielt `attrappe_ereignisse` ab.
extern AppLaunchReason attrappe_start_grund;
extern uint32_t attrappe_start_args;
extern int32_t attrappe_start_cookie;
extern void (*attrappe_ereignisse)(void);
void attrappe_wecker_ausloesen(int32_t cookie);     //< ein Wecker, waehrend die App laeuft
bool attrappe_taste(ButtonId button_id);            //< false: kein Fenster oder kein Handler
int attrappe_fenster_offen(void);                   //< Fenster auf dem Stapel
int attrappe_fenster_gezeigt(void);                 //< wie oft ein Fenster kam
int attrappe_vibes_doppelt(void);
int attrappe_vibes_kurz(void);
extern size_t attrappe_glance_grenze;               //< so viele Scheiben erlaubt die Uhr
int attrappe_glance_anzahl(void);
time_t attrappe_glance_ablauf(int nummer);
const char *attrappe_glance_text(int nummer);
