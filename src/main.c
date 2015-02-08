#include <pebble.h>
  
#define TICK_UNIT       MINUTE_UNIT
 
#define HR_DAY          6
#define HR_NIGHT        18
#define COLOR_FG(day)   (day ? GColorBlack : GColorWhite)
#define COLOR_BG(day)   (day ? GColorWhite : GColorBlack)
#define COMP_OP(day)    (day ? GCompOpAssign : GCompOpAssignInverted)
  
#define FMT_24H         "%H:%M"
#define FMT_12H         "%I:%M"
#define FMT_TIME_LEN    sizeof("00:00")
  
#define FMT_DATE        "%a %m %d"
#define FMT_DATE_LEN    sizeof("XXX 00 00")
  
#define RECT_TIME       GRect(5, 40, 139, 70)
#define RECT_PIKA       GRect(0, 122, 144, 48)
#define RECT_BANG       GRect(18, 128, 4, 16)
#define RECT_BAT(pct)   GRect(60, 160, (pct / 5) * 4, 2)
#define RECT_CHG        GRect(26, 140, 48, 16)

static Window *s_main_window;
static Layer *s_root_layer;

static TextLayer *s_time_layer;
static GFont s_time_font;

static BitmapLayer *s_bang_layer;
static GBitmap *s_bang;

static BitmapLayer *s_bat_layer;
static GBitmap *s_bat;

static BitmapLayer *s_chg_layer;
static GBitmap *s_chg;

static BitmapLayer *s_pika_layer;
static GBitmap *s_pika;

static bool firstUpdate;

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  
  // TODO: slide-in date
  
  // === Time ===
  static char time_buf[FMT_TIME_LEN];
  strftime(time_buf, FMT_TIME_LEN, clock_is_24h_style() ? FMT_24H : FMT_12H, tick_time);
  
  text_layer_set_text(s_time_layer, time_buf);
  layer_mark_dirty(text_layer_get_layer(s_time_layer));
  
  // === Day/night composting ===
  static bool day = true;
  static bool nextDay = true;
  static bool changed = true;
  
  // Did we go day->night or night->day
  changed = day ^ (nextDay = (tick_time->tm_hour >= HR_DAY && tick_time->tm_hour < HR_NIGHT));
  day = nextDay;
  
  if (firstUpdate || changed) {
    bitmap_layer_set_compositing_mode(s_pika_layer, COMP_OP(day));
    bitmap_layer_set_compositing_mode(s_bang_layer, COMP_OP(day));
    bitmap_layer_set_compositing_mode(s_bat_layer, COMP_OP(day));
    bitmap_layer_set_compositing_mode(s_chg_layer, COMP_OP(day));
    text_layer_set_text_color(s_time_layer, COLOR_FG(day));
//     text_layer_set_text_color(s_date_layer, COLOR_FG(day));
    window_set_background_color(s_main_window, COLOR_BG(day));
    layer_mark_dirty(s_root_layer);
  }
  
}

static void bt_handler(bool connected) {
  
  if (!firstUpdate && !connected)
    vibes_double_pulse();
  layer_set_hidden(bitmap_layer_get_layer(s_bang_layer), connected);
}

static void bat_handler(BatteryChargeState charge) {
  
  // Set charge indicator
  layer_set_hidden(bitmap_layer_get_layer(s_chg_layer), !charge.is_plugged);
  
  // Set battery bar
  layer_set_frame(bitmap_layer_get_layer(s_bat_layer), RECT_BAT(charge.charge_percent));
}
  
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
  
  // Bluetooth
  s_bang_layer = bitmap_layer_create(RECT_BANG);
  s_bang = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BANG);
  bitmap_layer_set_bitmap(s_bang_layer, s_bang);
  layer_insert_above_sibling(bitmap_layer_get_layer(s_bang_layer),
                             bitmap_layer_get_layer(s_pika_layer));
  
  // Battery
  s_bat_layer = bitmap_layer_create(RECT_BAT(100));
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
  firstUpdate = true;
  
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  
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
  
  tick_timer_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
  battery_state_service_unsubscribe();
}

int main() {
  init();
  app_event_loop();
  deinit();
}
