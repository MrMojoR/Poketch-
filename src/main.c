// metakirby5

#include <pebble.h>
#include "gbitmap_color_palette_manipulator.h"

// All time changes
#define TICK_UNIT       (MINUTE_UNIT | HOUR_UNIT | DAY_UNIT)

#define HR_DAY          6
#define HR_NIGHT        18

// Colors
#ifdef PBL_COLOR
  #define PALETTE_SIZE    2
  #define COLOR_FG(day)   (day ? GColorDarkGreen : GColorScreaminGreen)
  #define COLOR_BG(day)   (day ? GColorScreaminGreen : GColorDarkGreen)
#else
  #define COLOR_FG(day)   (day ? GColorBlack : GColorWhite)
  #define COLOR_BG(day)   (day ? GColorWhite : GColorBlack)
#endif

#define FMT_TIME(mil)   (mil ? "%H:%M" : "%I:%M")
#define FMT_TIME_LEN    sizeof("00:00")

#define FMT_DATE        "%m %d"
#define FMT_DATE_LEN    sizeof("00 00")

#define FMT_SEC         "%S"
#define FMT_SEC_LEN     sizeof("00")

#define TIMEOUT_SECDATE 3000

#if defined(PBL_RECT)
  #define RECT_TIME       GRect(5, 40, 139, 70)
#elif defined(PBL_ROUND)
  #define RECT_TIME       GRect(23, 40, 139, 70)
#endif
#define RECT_SECDATE    GRect(76, 124, 68, 30)
#define RECT_PIKA       GRect(0, 122, 144, 48)
#define RECT_BANG       GRect(18, 128, 4, 16)
#define RECT_BAT        GRect(60, 160, 80, 2)
#define BOUND_BAT(pct)  GRect(0, 0, (pct / 5) * 4, 2)
#define RECT_CHG        GRect(26, 140, 48, 16)

// === Layers ===

static Window *s_main_window;
static Layer *s_root_layer;

static TextLayer *s_time_layer;
static GFont s_time_font;

static TextLayer *s_secdate_layer;
static GFont s_secdate_font;

static BitmapLayer *s_bang_layer;
static GBitmap *s_bang;

static BitmapLayer *s_bat_layer;
static GBitmap *s_bat;

static BitmapLayer *s_chg_layer;
static GBitmap *s_chg;

static BitmapLayer *s_pika_layer;
static GBitmap *s_pika;

// === Helper variables ===

static bool firstUpdate = true;
static bool showDate = true;

// === Helper methods ===

static void colorize(BitmapLayer *layer, bool day) {
  
  #ifdef PBL_COLOR
    // The colors we're replacing
    
    GColor colors_to_replace[PALETTE_SIZE],
           replace_with_colors[PALETTE_SIZE];

    if (firstUpdate) {
      colors_to_replace[0] = GColorBlack;
      colors_to_replace[1] = GColorWhite;
    } else {
      colors_to_replace[0] = COLOR_FG(!day);
      colors_to_replace[1] = COLOR_BG(!day);
    }

    replace_with_colors[0] = COLOR_FG(day);
    replace_with_colors[1] = COLOR_BG(day);

    GBitmap *bitmap = (GBitmap *) bitmap_layer_get_bitmap(layer);
    replace_gbitmap_colors(colors_to_replace, replace_with_colors, PALETTE_SIZE, bitmap, layer);
  #else
    bitmap_layer_set_compositing_mode(layer, day ? GCompOpAssign : GCompOpAssignInverted);
  #endif
}

static void update_secdate(struct tm *tick_time) {
  
  static char date_buf[FMT_DATE_LEN];
  static char sec_buf[FMT_SEC_LEN];
  
  // Update the buffer
  if (showDate)
    strftime(date_buf, FMT_DATE_LEN, FMT_DATE, tick_time);
  else
    strftime(sec_buf, FMT_SEC_LEN, FMT_SEC, tick_time);
  
  // Set the buffer
  text_layer_set_text(s_secdate_layer, showDate ? date_buf : sec_buf);
  layer_mark_dirty(text_layer_get_layer(s_secdate_layer));
}

static void show_seconds(void *data) {
  
  // Get time
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  
  // Show seconds
  showDate = false;
  update_secdate(tick_time);
}

// === Handlers ===

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  
  // === Time ===
  if (units_changed & MINUTE_UNIT) {
    static char time_buf[FMT_TIME_LEN];
    strftime(time_buf, FMT_TIME_LEN, FMT_TIME(clock_is_24h_style()), tick_time);

    text_layer_set_text(s_time_layer, time_buf);
    layer_mark_dirty(text_layer_get_layer(s_time_layer));
  }
  
  // == Secs/date ==
  update_secdate(tick_time);
  
  // === Day/night color inversion ===
  static bool day = true;
  static bool nextDay = true;
  static bool changed = true;
  
  if (units_changed & HOUR_UNIT) {
    // Did we go day->night or night->day
    nextDay = tick_time->tm_hour >= HR_DAY && tick_time->tm_hour < HR_NIGHT;
    changed = firstUpdate || (day ^ nextDay);
    day = nextDay;
    
    if (changed) {
      colorize(s_pika_layer, day);
      colorize(s_bang_layer, day);
      colorize(s_bat_layer, day);
      colorize(s_chg_layer, day);
      text_layer_set_text_color(s_time_layer, COLOR_FG(day));
      text_layer_set_text_color(s_secdate_layer, COLOR_FG(day));
      window_set_background_color(s_main_window, COLOR_BG(day));
      layer_mark_dirty(s_root_layer);
    }
  }
}

static void acc_handler(AccelAxisType axis, int32_t direction) {
  
  static AppTimer *timer_secdate = NULL;
  
  // Get time
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  
  // Show date
  showDate = true;
  update_secdate(tick_time);
  
  // === Reschedule seconds showing ===
  // If timer exists, try rescheduling
  if (!(timer_secdate && app_timer_reschedule(timer_secdate, TIMEOUT_SECDATE)))
    // If it doesn't exist or already elapsed, schedule a new timer
    timer_secdate = app_timer_register(TIMEOUT_SECDATE, show_seconds, NULL);
}

static void bt_handler(bool connected) {
  
  // Vibrate
  if (!firstUpdate)
    connected ? vibes_short_pulse() : vibes_double_pulse();
  
  // Set BT indicator
  layer_set_hidden(bitmap_layer_get_layer(s_bang_layer), connected);
}

static void bat_handler(BatteryChargeState charge) {
  
  // Set charge indicator
  layer_set_hidden(bitmap_layer_get_layer(s_chg_layer), !charge.is_plugged);
  
  // Set battery bar
  layer_set_bounds(bitmap_layer_get_layer(s_bat_layer), BOUND_BAT(charge.charge_percent));
}

// === Setup ===
  
static void main_window_load(Window *window) {
	
  s_root_layer = window_get_root_layer(window);
  
  // Pikachu BG
  s_pika_layer = bitmap_layer_create(RECT_PIKA);
  s_pika = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_PIKA_BG);
  bitmap_layer_set_bitmap(s_pika_layer, s_pika);
  layer_add_child(s_root_layer, bitmap_layer_get_layer(s_pika_layer));
  
  // Time
  s_time_layer = text_layer_create(RECT_TIME);
  s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_POKETCH_DIGITAL_70));
  text_layer_set_font(s_time_layer, s_time_font);
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(s_root_layer, text_layer_get_layer(s_time_layer));
  
  // Secs/date
  s_secdate_layer = text_layer_create(RECT_SECDATE);
  s_secdate_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_POKETCH_DIGITAL_30));
  text_layer_set_font(s_secdate_layer, s_secdate_font);
  text_layer_set_background_color(s_secdate_layer, GColorClear);
  text_layer_set_text_alignment(s_secdate_layer, GTextAlignmentCenter);
  layer_add_child(s_root_layer, text_layer_get_layer(s_secdate_layer));
  
  // Bluetooth
  s_bang_layer = bitmap_layer_create(RECT_BANG);
  s_bang = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BANG);
  bitmap_layer_set_bitmap(s_bang_layer, s_bang);
  layer_insert_above_sibling(bitmap_layer_get_layer(s_bang_layer),
                             bitmap_layer_get_layer(s_pika_layer));
  
  // Battery
  s_bat_layer = bitmap_layer_create(RECT_BAT);
  s_bat = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BAT);
  bitmap_layer_set_bitmap(s_bat_layer, s_bat);
  layer_set_clips(bitmap_layer_get_layer(s_bat_layer), true); // enable clipping
  layer_insert_above_sibling(bitmap_layer_get_layer(s_bat_layer),
                             bitmap_layer_get_layer(s_pika_layer));
  
  // Charging
  s_chg_layer = bitmap_layer_create(RECT_CHG);
  s_chg = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CHG);
  bitmap_layer_set_bitmap(s_chg_layer, s_chg);
  layer_insert_above_sibling(bitmap_layer_get_layer(s_chg_layer),
                             bitmap_layer_get_layer(s_pika_layer));
  
  // === Initial update ===
  
  // Get current time
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  
  // Initial handler runs
  tick_handler(tick_time, TICK_UNIT);
  bt_handler(bluetooth_connection_service_peek());
  bat_handler(battery_state_service_peek());
  
  firstUpdate = false;
}

static void main_window_unload(Window *window) {
  
  // Pikachu BG
  bitmap_layer_destroy(s_pika_layer);
  gbitmap_destroy(s_pika);

  // Time
  text_layer_destroy(s_time_layer);
  fonts_unload_custom_font(s_time_font);
  
  // Secs/date
  text_layer_destroy(s_secdate_layer);
  fonts_unload_custom_font(s_secdate_font);

  // Bluetooth
  bitmap_layer_destroy(s_bang_layer);
  gbitmap_destroy(s_bang);
  
  // Battery
  bitmap_layer_destroy(s_bat_layer);
  gbitmap_destroy(s_bat);
  
  // Charging
  bitmap_layer_destroy(s_chg_layer);
  gbitmap_destroy(s_chg);
}

static void init() {
  
  // Register services
  tick_timer_service_subscribe(TICK_UNIT, tick_handler);
//   accel_tap_service_subscribe(acc_handler);
  bluetooth_connection_service_subscribe(bt_handler);
  battery_state_service_subscribe(bat_handler);
  
  s_main_window = window_create();

  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  window_stack_push(s_main_window, true);
}

static void deinit() {

  window_destroy(s_main_window);
  
  // Unregister services
  tick_timer_service_unsubscribe();
//   accel_tap_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
  battery_state_service_unsubscribe();
}

int main() {
  init();
  app_event_loop();
  deinit();
}
