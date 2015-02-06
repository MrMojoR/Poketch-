#include <pebble.h>
  
#define INTERVAL_UPDATE MINUTE_UNIT
 
#define HR_DAY          6
#define COLOR_FG_DAY    GColorBlack
#define COLOR_BG_DAY    GColorWhite
#define COMP_OP_DAY     GCompOpAssign
  
#define HR_NIGHT        18
#define COLOR_FG_NIGHT  GColorWhite
#define COLOR_BG_NIGHT  GColorBlack
#define COMP_OP_NIGHT   GCompOpAssignInverted
  
#define FMT_24H         "%H:%M"
#define FMT_12H         "%I:%M"
#define FMT_TIME_LEN    sizeof("00:00")
  
#define FMT_DATE        "%a %m %d"
#define FMT_DATE_LEN    sizeof("XXX 00 00")

#define PWR_HI          70
#define PWR_MED         40
#define PWR_LO          10
  
#define POS_TIME        GRect(5, 40, 139, 70)
#define POS_PIKA        GRect(0, 122, 144, 48)
  
static Window *s_main_window;
static Layer *s_root_layer;

static TextLayer *s_time_layer;
static GFont s_time_font;

static BitmapLayer *s_pika_layer;
static GBitmap *s_pika;

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  
  // Time
  static char time_buf[FMT_TIME_LEN];
  strftime(time_buf, FMT_TIME_LEN, clock_is_24h_style() ? FMT_24H : FMT_12H, tick_time);
  
  text_layer_set_text(s_time_layer, time_buf);
  layer_mark_dirty(text_layer_get_layer(s_time_layer));
}
  
static void main_window_load(Window *window) {
	
  s_root_layer = window_get_root_layer(window);
  
  // Pikachu BG
  s_pika_layer = bitmap_layer_create(POS_PIKA);
  layer_add_child(s_root_layer, bitmap_layer_get_layer(s_pika_layer));
  
  // TODO: Dynamic charging? Day/night colors?
  s_pika = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_PIKA_BG);
  bitmap_layer_set_bitmap(s_pika_layer, s_pika);
  
  // Time
  s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_POKETCH_DIGITAL_70));
  s_time_layer = text_layer_create(POS_TIME);
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_font(s_time_layer, s_time_font);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(s_root_layer, text_layer_get_layer(s_time_layer));
  
  // TODO: Day/night colors?
  text_layer_set_text_color(s_time_layer, GColorBlack);
  
  // === Initial update ===
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  
  tick_handler(tick_time, INTERVAL_UPDATE);
}

static void main_window_unload(Window *window) {
  
  // Pikachu BG
  bitmap_layer_destroy(s_pika_layer);
  gbitmap_destroy(s_pika);

  // Time
  text_layer_destroy(s_time_layer);
  fonts_unload_custom_font(s_time_font);
}

static void init() {
  
  // Register services
  tick_timer_service_subscribe(INTERVAL_UPDATE, tick_handler);
//   bluetooth_connection_service_subscribe(bt_handler);
//   battery_state_service_subscribe(batt_handler);
  
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
//   bluetooth_connection_service_unsubscribe();
//   battery_state_service_unsubscribe();
}

int main() {
  init();
  app_event_loop();
  deinit();
}
