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
  #define COLOR_FG(day)   ((day) ? GColorDarkGreen : GColorMalachite)
  #define COLOR_BG(day)   ((day) ? GColorMalachite : GColorDarkGreen)
#else
  #define COLOR_FG(day)   ((day) ? GColorBlack : GColorWhite)
  #define COLOR_BG(day)   ((day) ? GColorWhite : GColorBlack)
#endif

#define FMT_TIME(mil)   (mil ? "%H:%M" : "%I:%M")
#define FMT_TIME_LEN    sizeof("00:00")

#define FMT_DATE        "%m %d"
#define FMT_DATE_LEN    sizeof("00 00")

#define FMT_SEC         "%S"
#define FMT_SEC_LEN     sizeof("00")

#define FMT_STEPS        "%d"
#define FMT_STEPS_LEN     sizeof("00000")

#define FMT_DAY        "%s"
#define FMT_DAY_LEN     sizeof("DAY")

#define TIMEOUT_SECDATE 3000

#if defined(PBL_RECT)
  #define RECT_TIME       GRect(5, 48, 139, 78)
#elif defined(PBL_ROUND)
  #define RECT_TIME       GRect(23, 40, 139, 70)
#endif
#define RECT_SECDATE    GRect(76, 124, 68, 30)
#define RECT_STEPS    	GRect(18, 31, 72,168-25)
#define RECT_DAY   		  GRect(60, 31, 70,168-25)
#define RECT_WEATHER    GRect(0, 8, 144,168)
#define RECT_PIKA       GRect(0, 122, 144, 48)
#define RECT_BANG       GRect(18, 128, 4, 16)
#define RECT_BAT        GRect(60, 160, 80, 2)
#define BOUND_BAT(pct)  GRect(0, 0, (pct / 5) * 4, 2)
#define RECT_CHG        GRect(26, 140, 48, 16)

//Weekdays

const char* dayNames[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};


// === Layers ===

static Window *s_main_window;
static Layer *s_root_layer;

static TextLayer *s_time_layer;
static GFont s_time_font;

static TextLayer *s_secdate_layer;
static GFont s_secdate_font;

static TextLayer *s_steps_layer;
static GFont s_steps_font;

static TextLayer *s_day_layer;
static GFont s_day_font;

static TextLayer *s_weather_layer;
static GFont s_weather_font;

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
  static char day_buf[FMT_DAY_LEN];
  
  int weekday = tick_time->tm_wday;

  // Update the buffer
  if (showDate){
    strftime(date_buf, FMT_DATE_LEN, FMT_DATE, tick_time);
	snprintf(day_buf, FMT_DAY_LEN, FMT_DAY, dayNames[weekday]);
  }
  else
    strftime(sec_buf, FMT_SEC_LEN, FMT_SEC, tick_time);

  // Set the buffer
  text_layer_set_text(s_day_layer, day_buf);
  text_layer_set_text(s_secdate_layer, showDate ? date_buf : sec_buf);
  layer_mark_dirty(text_layer_get_layer(s_secdate_layer));
  layer_mark_dirty(text_layer_get_layer(s_day_layer));
}

static void update_steps() {
	static char steps_buffer[FMT_STEPS_LEN];
	unsigned short int steps_day = health_service_sum_today(HealthMetricStepCount);
  snprintf(steps_buffer,FMT_STEPS_LEN,FMT_STEPS,steps_day);
	text_layer_set_text(s_steps_layer, steps_buffer);
	layer_mark_dirty(text_layer_get_layer(s_steps_layer));
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
  
  update_steps();
  
  layer_mark_dirty(text_layer_get_layer(s_steps_layer));
  
  //Update weather
  // Get weather update every 30 minutes
  if(tick_time->tm_min % 30 == 0) {
  // Begin dictionary
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);

    // Add a key-value pair
    dict_write_uint8(iter, 0, 0);
  
    // Send the message!
    app_message_outbox_send();
}


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
	    text_layer_set_text_color(s_steps_layer, COLOR_FG(day));
      text_layer_set_text_color(s_day_layer, COLOR_FG(day));
      text_layer_set_text_color(s_weather_layer, COLOR_FG(day));
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
	
  update_steps();
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
  
  // Steps
  s_steps_layer = text_layer_create(RECT_STEPS);
  s_steps_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SQUARE_22));
  text_layer_set_font(s_steps_layer, s_steps_font);
  text_layer_set_background_color(s_steps_layer, GColorClear);
  text_layer_set_text_alignment(s_steps_layer, GTextAlignmentLeft);
  layer_add_child(s_root_layer, text_layer_get_layer(s_steps_layer));
  
  // Day
  s_day_layer = text_layer_create(RECT_DAY);
  s_day_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SQUARE_22));
  text_layer_set_font(s_day_layer, s_day_font);
  text_layer_set_background_color(s_day_layer, GColorClear);
  text_layer_set_text_alignment(s_day_layer, GTextAlignmentRight);
  layer_add_child(s_root_layer, text_layer_get_layer(s_day_layer));
  
  // Weather
  s_weather_layer = text_layer_create(RECT_WEATHER);
  s_weather_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SQUARE_16));
  text_layer_set_font(s_weather_layer, s_weather_font);
  text_layer_set_background_color(s_weather_layer, GColorClear);
  text_layer_set_text_alignment(s_weather_layer, GTextAlignmentCenter);
  text_layer_set_text(s_weather_layer, "loading");
  layer_add_child(s_root_layer, text_layer_get_layer(s_weather_layer));

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
  
  // Steps
  text_layer_destroy(s_steps_layer);
  fonts_unload_custom_font(s_steps_font);
  
  // Day
  text_layer_destroy(s_day_layer);
  fonts_unload_custom_font(s_day_font);
  
  // Weather
  text_layer_destroy(s_weather_layer);
  fonts_unload_custom_font(s_weather_font);

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

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  
  // Store incoming information
static char temperature_buffer[8];
static char conditions_buffer[32];
static char weather_layer_buffer[40];
  
   // Read tuples for data
Tuple *temp_tuple = dict_find(iterator, MESSAGE_KEY_TEMPERATURE);
Tuple *conditions_tuple = dict_find(iterator, MESSAGE_KEY_CONDITIONS);
  

// If all data is available, use it
if(temp_tuple && conditions_tuple) {
  snprintf(temperature_buffer, sizeof(temperature_buffer), "%dC",(int)temp_tuple->value->int32);
  snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", conditions_tuple->value->cstring);
}
  
  // Assemble full string and display
snprintf(weather_layer_buffer, sizeof(weather_layer_buffer), "%s %s", temperature_buffer, conditions_buffer);
  //snprintf(weather_layer_buffer, sizeof(weather_layer_buffer), "%s", "15c cloudy");
  if (strlen(weather_layer_buffer) > 13){
    layer_set_frame(text_layer_get_layer(s_weather_layer), GRect(0, 0, 144,168));
    layer_set_frame(text_layer_get_layer(s_steps_layer), 	GRect(18, 34, 72,168-25));
    layer_set_frame(text_layer_get_layer(s_day_layer), 	GRect(60, 34, 70,168-25));
    layer_set_frame(text_layer_get_layer(s_time_layer), 	GRect(5, 51, 139, 81));
  }else{
    layer_set_frame(text_layer_get_layer(s_weather_layer), GRect(0, 8, 144,168));
    layer_set_frame(text_layer_get_layer(s_steps_layer), 	GRect(18, 31, 72,168-25));
    layer_set_frame(text_layer_get_layer(s_day_layer), 	GRect(60, 31, 70,168-25));
    layer_set_frame(text_layer_get_layer(s_time_layer), 	GRect(5, 48, 139, 78));
  }
text_layer_set_text(s_weather_layer, weather_layer_buffer);
  layer_mark_dirty(text_layer_get_layer(s_weather_layer));

}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}




static void init() {

  // Register services
  tick_timer_service_subscribe(TICK_UNIT, tick_handler);
  //accel_tap_service_subscribe(acc_handler);
  bluetooth_connection_service_subscribe(bt_handler);
  battery_state_service_subscribe(bat_handler);

  s_main_window = window_create();

  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  //Init weather
  app_message_register_inbox_received(inbox_received_callback);
  
  // Open AppMessage
const int inbox_size = 128;
const int outbox_size = 128;
app_message_open(inbox_size, outbox_size);
  
app_message_register_inbox_dropped(inbox_dropped_callback);
app_message_register_outbox_failed(outbox_failed_callback);
app_message_register_outbox_sent(outbox_sent_callback);
  
  window_stack_push(s_main_window, true);
}

static void deinit() {

  window_destroy(s_main_window);

  // Unregister services
  tick_timer_service_unsubscribe();
  //accel_tap_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
  battery_state_service_unsubscribe();
}

int main() {
  init();
  app_event_loop();
  deinit();
}
