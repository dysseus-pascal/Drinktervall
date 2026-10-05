// Fenster, Tasten, Start und Glance fuer tools/app_host_test.c - siehe
// attrappe_app.h.
#include <pebble.h>

// --- Fenster ---
struct Window {
  WindowHandlers handler;
  ClickConfigProvider klicks;
  bool konfiguriert;
  bool geladen;     // wie is_loaded der Firmware: load kam, unload noch nicht
  bool sichtbar;    // wie on_screen: appear kam, disappear noch nicht
  ClickHandler tasten[NUM_BUTTONS];
};
#define STAPEL_MAX 8
static Window *s_stapel[STAPEL_MAX];
static int s_stapel_n;
static int s_gezeigt;
static Window *s_konfiguriert_gerade;

Window *window_create(void) { return calloc(1, sizeof(Window)); }
void window_destroy(Window *window) { free(window); }
void window_set_background_color(Window *window, GColor color) { (void)window; (void)color; }
void window_set_window_handlers(Window *window, WindowHandlers handlers) { window->handler = handlers; }
void window_set_click_config_provider(Window *window, ClickConfigProvider provider) {
  window->klicks = provider;
}
Layer *window_get_root_layer(const Window *window) {
  (void)window;
  return layer_create(GRect(0, 0, PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT));
}

// Wie window_set_on_screen der Firmware: load nur beim ersten Mal, appear
// und disappear nur bei einem Wechsel. Die Tasten werden danach neu verteilt.
static void prv_auf_schirm(Window *window) {
  if (window->sichtbar) return;
  window->sichtbar = true;
  window->konfiguriert = false;
  if (!window->geladen) {
    window->geladen = true;
    if (window->handler.load) window->handler.load(window);
  }
  if (window->handler.appear) window->handler.appear(window);
}

static void prv_vom_schirm(Window *window) {
  if (!window->sichtbar) return;
  window->sichtbar = false;
  if (window->geladen && window->handler.disappear) window->handler.disappear(window);
}

// Wie window_unload: danach das Fenster nicht mehr anfassen - unload darf
// es zerstoeren.
static void prv_entladen(Window *window) {
  if (!window->geladen) return;
  window->geladen = false;
  if (window->handler.unload) window->handler.unload(window);
}

void window_stack_push(Window *window, bool animated) {
  (void)animated;
  if (s_stapel_n == STAPEL_MAX) return;
  Window *vorher = s_stapel_n > 0 ? s_stapel[s_stapel_n - 1] : NULL;
  s_stapel[s_stapel_n++] = window;
  s_gezeigt++;
  // Im setup des Uebergangs: erst geht das bisherige vom Schirm, dann kommt
  // das neue (window_transition_context_appearance_call_all).
  if (vorher) prv_vom_schirm(vorher);
  prv_auf_schirm(window);
}

// SOFORT ENTLADEN, auch das oberste Fenster und auch mit Animation: die
// Firmware plant den Uebergang, sein setup laeuft noch im Aufruf und entlaedt
// dabei das Weggenommene (window_stack.c prv_remove_item -> prv_transition_to
// -> animation_schedule -> setup -> prv_unload_removed_windows). Ohne Fenster
// darunter oder fuer ein verdecktes ebenso, ueber den else-Zweig.
bool window_stack_remove(Window *window, bool animated) {
  (void)animated;
  for (int i = 0; i < s_stapel_n; i++) {
    if (s_stapel[i] != window) continue;
    const bool oben = i == s_stapel_n - 1;
    memmove(&s_stapel[i], &s_stapel[i + 1], (size_t)(s_stapel_n - i - 1) * sizeof(Window *));
    s_stapel_n--;
    prv_vom_schirm(window);
    prv_entladen(window);
    // Das darunter kommt wieder zum Vorschein: appear (Trink-Fenster: die
    // Animation beginnt neu).
    if (oben && s_stapel_n > 0) prv_auf_schirm(s_stapel[s_stapel_n - 1]);
    return true;
  }
  return false;
}

// Die Firmware legt erst die verdeckten Fenster auf die Liste der entfernten,
// dann das oberste davor - entladen wird darum das oberste zuerst, danach von
// unten nach oben.
void window_stack_pop_all(bool animated) {
  (void)animated;
  if (s_stapel_n == 0) return;
  Window *weg[STAPEL_MAX];
  const int n = s_stapel_n;
  memcpy(weg, s_stapel, (size_t)n * sizeof(Window *));
  s_stapel_n = 0;
  prv_vom_schirm(weg[n - 1]);
  prv_entladen(weg[n - 1]);
  for (int i = 0; i < n - 1; i++) prv_entladen(weg[i]);
}

bool window_stack_contains_window(Window *window) {
  for (int i = 0; i < s_stapel_n; i++) {
    if (s_stapel[i] == window) return true;
  }
  return false;
}

void window_single_click_subscribe(ButtonId button_id, ClickHandler handler) {
  if (s_konfiguriert_gerade) s_konfiguriert_gerade->tasten[button_id] = handler;
}

bool attrappe_taste(ButtonId button_id) {
  if (s_stapel_n == 0) return false;
  Window *oben = s_stapel[s_stapel_n - 1];
  if (!oben->konfiguriert && oben->klicks) {
    s_konfiguriert_gerade = oben;
    oben->klicks(NULL);
    s_konfiguriert_gerade = NULL;
  }
  oben->konfiguriert = true;
  if (!oben->tasten[button_id]) return false;
  oben->tasten[button_id](NULL, NULL);
  return true;
}
int attrappe_fenster_offen(void) { return s_stapel_n; }
int attrappe_fenster_gezeigt(void) { return s_gezeigt; }

// --- Bilder und Aktionsleiste ---
struct GBitmap { uint32_t id; };
GBitmap *gbitmap_create_with_resource(uint32_t resource_id) {
  GBitmap *b = calloc(1, sizeof(GBitmap));
  if (b) b->id = resource_id;
  return b;
}
void gbitmap_destroy(GBitmap *bitmap) { free(bitmap); }

struct ActionBarLayer { ClickConfigProvider klicks; };
ActionBarLayer *action_bar_layer_create(void) { return calloc(1, sizeof(ActionBarLayer)); }
void action_bar_layer_destroy(ActionBarLayer *bar) { free(bar); }
void action_bar_layer_set_background_color(ActionBarLayer *bar, GColor color) { (void)bar; (void)color; }
void action_bar_layer_set_icon(ActionBarLayer *bar, ButtonId button_id, const GBitmap *icon) {
  (void)bar; (void)button_id; (void)icon;
}
void action_bar_layer_set_click_config_provider(ActionBarLayer *bar, ClickConfigProvider provider) {
  bar->klicks = provider;
}
// Die Aktionsleiste uebernimmt die Tasten ihres Fensters.
void action_bar_layer_add_to_window(ActionBarLayer *bar, Window *window) {
  window->klicks = bar->klicks;
  window->konfiguriert = false;
}

// --- Text ---
GFont fonts_get_system_font(const char *font_key) { return font_key; }
void graphics_context_set_text_color(GContext *ctx, GColor color) { (void)ctx; (void)color; }
void graphics_draw_text(GContext *ctx, const char *text, GFont font, GRect box,
                        GTextOverflowMode overflow, GTextAlignment alignment, GTextAttributes *attributes) {
  (void)ctx; (void)text; (void)font; (void)box; (void)overflow; (void)alignment; (void)attributes;
}
GSize graphics_text_layout_get_content_size(const char *text, GFont font, GRect box,
                                            GTextOverflowMode overflow, GTextAlignment alignment) {
  (void)text; (void)font; (void)overflow; (void)alignment;
  return box.size;
}
void graphics_fill_rect(GContext *ctx, GRect rect, uint16_t corner_radius, GCornerMask corner_mask) {
  (void)ctx; (void)rect; (void)corner_radius; (void)corner_mask;
}
void clock_copy_time_string(char *buffer, uint8_t size) {
  struct tm x;
  localtime_r(&stub_jetzt, &x);
  strftime(buffer, size, "%H:%M", &x);
}

// --- Vibration, Licht, Ruhezeit ---
static int s_vibes_kurz, s_vibes_doppelt;
void vibes_short_pulse(void) { s_vibes_kurz++; }
void vibes_double_pulse(void) { s_vibes_doppelt++; }
void light_enable_interaction(void) {}
bool quiet_time_is_active(void) { return false; }
int attrappe_vibes_doppelt(void) { return s_vibes_doppelt; }
int attrappe_vibes_kurz(void) { return s_vibes_kurz; }

// --- Start, Wecker, Ereignisschleife ---
AppLaunchReason attrappe_start_grund = APP_LAUNCH_USER;
uint32_t attrappe_start_args;
int32_t attrappe_start_cookie;
void (*attrappe_ereignisse)(void);
static WakeupHandler s_wecker_handler;

AppLaunchReason launch_reason(void) { return attrappe_start_grund; }
uint32_t launch_get_args(void) { return attrappe_start_args; }
bool wakeup_get_launch_event(WakeupId *wakeup_id, int32_t *cookie) {
  if (attrappe_start_grund != APP_LAUNCH_WAKEUP) return false;
  *wakeup_id = 1;
  *cookie = attrappe_start_cookie;
  return true;
}
void wakeup_service_subscribe(WakeupHandler handler) { s_wecker_handler = handler; }
void attrappe_wecker_ausloesen(int32_t cookie) {
  if (s_wecker_handler) s_wecker_handler(2, cookie);
}
void app_event_loop(void) {
  if (attrappe_ereignisse) attrappe_ereignisse();
}

// --- Glance ---
#define GLANCE_MAX 8
size_t attrappe_glance_grenze = GLANCE_MAX;
struct AppGlanceReloadSession { int unbenutzt; };
static struct { time_t ablauf; char text[160]; } s_scheiben[GLANCE_MAX];
static int s_scheiben_n;

void app_glance_reload(AppGlanceReloadCallback callback, void *context) {
  s_scheiben_n = 0;
  struct AppGlanceReloadSession sitzung = { 0 };
  if (callback) callback(&sitzung, attrappe_glance_grenze, context);
}
AppGlanceResult app_glance_add_slice(AppGlanceReloadSession *session, AppGlanceSlice slice) {
  (void)session;
  if (s_scheiben_n >= (int)attrappe_glance_grenze) return APP_GLANCE_RESULT_SLICE_CAPACITY_EXCEEDED;
  if (slice.expiration_time != APP_GLANCE_SLICE_NO_EXPIRATION && slice.expiration_time <= stub_jetzt) {
    return APP_GLANCE_RESULT_EXPIRES_IN_THE_PAST;
  }
  s_scheiben[s_scheiben_n].ablauf = slice.expiration_time;
  snprintf(s_scheiben[s_scheiben_n].text, sizeof(s_scheiben[0].text), "%s",
           slice.layout.subtitle_template_string ? slice.layout.subtitle_template_string : "");
  s_scheiben_n++;
  return APP_GLANCE_RESULT_SUCCESS;
}
int attrappe_glance_anzahl(void) { return s_scheiben_n; }
time_t attrappe_glance_ablauf(int nummer) { return nummer < s_scheiben_n ? s_scheiben[nummer].ablauf : -1; }
const char *attrappe_glance_text(int nummer) { return nummer < s_scheiben_n ? s_scheiben[nummer].text : ""; }
