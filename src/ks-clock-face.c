#include <pebble.h>

#define COLORS       true
#define ANTIALIASING true

#define HAND_MARGIN  10
#define FINAL_RADIUS 52

#define MARGIN 10
#define TEXT_HEIGHT 30

#define ANIMATION_DURATION 500
#define ANIMATION_DELAY    600

#define TEXT_MODE 1
#define CIRCLE_MODE 2

#define FACE_MODE 0
#define SHOW_GIFS 1

typedef struct {
  int hours;
  int minutes;
} Time;

static Window *s_main_window;
static Layer *s_canvas_layer;

static GFont s_time_font;
static GFont s_date_font;

static GBitmap *s_pipboy_bitmap;
static BitmapLayer *s_pipboy_layer;

static GPoint s_center;
static Time s_last_time, s_anim_time;
static int s_radius = 0;
static bool s_animating = false;

static int face_mode = TEXT_MODE;
static bool show_gifs = false;

static int s_battery_level = -2;

/******************************** Configuration *******************************/

static void in_recv_handler(DictionaryIterator *iterator, void *context)
{
  //Get Tuple
  Tuple *face_mode_t = dict_find(iterator, FACE_MODE);
  Tuple *show_gifs_t = dict_find(iterator, SHOW_GIFS);

  if (face_mode_t) {
    face_mode = face_mode_t->value->int8;
    persist_write_int(FACE_MODE, face_mode);
    if (s_canvas_layer) {
      layer_mark_dirty(s_canvas_layer);
    }
  }

  if (show_gifs_t) {
    show_gifs = show_gifs_t->value->int8;
    persist_write_bool(SHOW_GIFS, show_gifs);
  }
}

/*************************** AnimationImplementation **************************/

static void animation_started(Animation *anim, void *context) {
  s_animating = true;
}

static void animation_stopped(Animation *anim, bool stopped, void *context) {
  s_animating = false;
}

static void animate(int duration, int delay, AnimationImplementation *implementation, bool handlers) {
  Animation *anim = animation_create();
  animation_set_duration(anim, duration);
  animation_set_delay(anim, delay);
  animation_set_curve(anim, AnimationCurveEaseInOut);
  animation_set_implementation(anim, implementation);
  if(handlers) {
    animation_set_handlers(anim, (AnimationHandlers) {
      .started = animation_started,
      .stopped = animation_stopped
    }, NULL);
  }
  animation_schedule(anim);
}

/************************************ UI **************************************/


static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  // Store time
  s_last_time.hours = tick_time->tm_hour;
  s_last_time.hours -= (s_last_time.hours > 12) ? 12 : 0;
  s_last_time.minutes = tick_time->tm_min;

  // Redraw
  if(s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
  APP_LOG(APP_LOG_LEVEL_WARNING, "Tick!  %d %d", FACE_MODE, SHOW_GIFS);

}

static int hours_to_minutes(int hours_out_of_12) {
  return (int)(float)(((float)hours_out_of_12 / 12.0F) * 60.0F);
}

static void draw_background(Layer *layer, GContext *ctx) {
  // Color background?
  if(COLORS) {
    // Draw the TV stripes.
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, GRect(0, 0, 144, 168), 0, GCornerNone);

    graphics_context_set_stroke_color(ctx, GColorIslamicGreen);
    graphics_context_set_stroke_width(ctx, 1);
    for (int i = 0; i < 168; i+=4) {
      graphics_draw_rect(ctx, GRect(0, i, 144, 2));
    }
    
    // Draw the battery meter.
    GRect bounds = GRect(MARGIN, 168 - 2 * MARGIN, 72 - MARGIN, MARGIN);
    // Draw the background
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, bounds, 2, GCornersAll);
    int width = bounds.size.w - 2;

    if (s_battery_level >= 0) {
      // Find the width of the bar
      width = (int)(float)(((float)s_battery_level / 100.0F) * width);
    
      // Draw the bar
      graphics_context_set_fill_color(ctx, GColorGreen);
    } else {
      // Draw the bar
      graphics_context_set_fill_color(ctx, GColorPictonBlue);
    }
    graphics_fill_rect(ctx, GRect(bounds.origin.x + 1, bounds.origin.y + 1, width, bounds.size.h - 2), 2, GCornersAll);         
  }
}

static void text_update_proc(Layer *layer, GContext *ctx) {
  GRect layer_bounds = layer_get_bounds(layer);
  GRect bounds = GRect(MARGIN, MARGIN/2, layer_bounds.size.w - 2 * MARGIN, TEXT_HEIGHT);
  char time_buffer[16];
  clock_copy_time_string(time_buffer, sizeof(time_buffer));

  graphics_context_set_text_color(ctx, GColorGreen);
  graphics_draw_text(ctx, time_buffer, s_time_font, bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);

  // Copy date into buffer from tm structure
  strftime(time_buffer, sizeof(time_buffer), "%a %d %b", tick_time);
  
  bounds = GRect(MARGIN, MARGIN * 2.5 + TEXT_HEIGHT, layer_bounds.size.w - 2 * MARGIN, TEXT_HEIGHT);
  graphics_draw_text(ctx, time_buffer, s_date_font, bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
}

static void circle_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_stroke_color(ctx, GColorGreen);
  graphics_context_set_stroke_width(ctx, 4);

  graphics_context_set_antialiased(ctx, ANTIALIASING);

  // Draw outline
  graphics_draw_circle(ctx, s_center, s_radius);

  // Don't use current time while animating
  Time mode_time = (s_animating) ? s_anim_time : s_last_time;

  // Adjust for minutes through the hour
  float minute_angle = TRIG_MAX_ANGLE * mode_time.minutes / 60;
  float hour_angle;
  if(s_animating) {
    // Hours out of 60 for smoothness
    hour_angle = TRIG_MAX_ANGLE * mode_time.hours / 60;
  } else {
    hour_angle = TRIG_MAX_ANGLE * mode_time.hours / 12;
  }
  hour_angle += (minute_angle / TRIG_MAX_ANGLE) * (TRIG_MAX_ANGLE / 12);

  // Plot hands
  GPoint minute_hand = (GPoint) {
    .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) * (int32_t)(s_radius - HAND_MARGIN) / TRIG_MAX_RATIO) + s_center.x,
    .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) * (int32_t)(s_radius - HAND_MARGIN) / TRIG_MAX_RATIO) + s_center.y,
  };
  GPoint hour_hand = (GPoint) {
    .x = (int16_t)(sin_lookup(hour_angle) * (int32_t)(s_radius - (2 * HAND_MARGIN)) / TRIG_MAX_RATIO) + s_center.x,
    .y = (int16_t)(-cos_lookup(hour_angle) * (int32_t)(s_radius - (2 * HAND_MARGIN)) / TRIG_MAX_RATIO) + s_center.y,
  };

  // Draw hands with positive length only
  if(s_radius > 2 * HAND_MARGIN) {
    graphics_draw_line(ctx, s_center, hour_hand);
  }
  if(s_radius > HAND_MARGIN) {
    graphics_draw_line(ctx, s_center, minute_hand);
  }
}

static void update_proc(Layer *layer, GContext *ctx) {
  if (face_mode == CIRCLE_MODE) {
    circle_update_proc(layer, ctx);
  } else {
    text_update_proc(layer, ctx);
  }
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);
  GRect image_bounds = GRect(0, 0, 80, 94);
  grect_align(&image_bounds, &window_bounds, GAlignBottomRight, true);


  // s_center = grect_center_point(&window_bounds);
  s_center = GPoint(FINAL_RADIUS + HAND_MARGIN, FINAL_RADIUS + HAND_MARGIN);

  s_pipboy_bitmap = gbitmap_create_with_resource(RESOURCE_ID_PIPBOY_OUTLINE);
  s_pipboy_layer = bitmap_layer_create(image_bounds);
  bitmap_layer_set_bitmap(s_pipboy_layer, s_pipboy_bitmap);
  bitmap_layer_set_background_color(s_pipboy_layer, GColorClear);
  bitmap_layer_set_compositing_mode(s_pipboy_layer, GCompOpSet);
  bitmap_layer_set_alignment(s_pipboy_layer, GAlignBottomRight);

  layer_set_update_proc(window_layer, draw_background);

  s_canvas_layer = layer_create(window_bounds);
  layer_set_update_proc(s_canvas_layer, update_proc);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_pipboy_layer));
  layer_add_child(window_layer, s_canvas_layer);
}

static void window_unload(Window *window) {
  gbitmap_destroy(s_pipboy_bitmap);
  bitmap_layer_destroy(s_pipboy_layer);

  layer_destroy(s_canvas_layer);
  s_canvas_layer = NULL;
}

/*********************************** App **************************************/

static void battery_callback(BatteryChargeState state) {
  // Record the new battery level
  int newLevel = -1;
  if (!state.is_charging) {
    newLevel = state.charge_percent;
  }
  if (s_battery_level != newLevel) {
    s_battery_level = newLevel;
    if (s_canvas_layer) {
      layer_mark_dirty(s_canvas_layer);
    }
  }
}

static int anim_percentage(AnimationProgress dist_normalized, int max) {
  return (int)(float)(((float)dist_normalized / (float)ANIMATION_NORMALIZED_MAX) * (float)max);
}

static void radius_update(Animation *anim, AnimationProgress dist_normalized) {
  s_radius = anim_percentage(dist_normalized, FINAL_RADIUS);

  layer_mark_dirty(s_canvas_layer);
}

static void hands_update(Animation *anim, AnimationProgress dist_normalized) {
  s_anim_time.hours = anim_percentage(dist_normalized, hours_to_minutes(s_last_time.hours));
  s_anim_time.minutes = anim_percentage(dist_normalized, s_last_time.minutes);

  layer_mark_dirty(s_canvas_layer);
}

static void init() {
  srand(time(NULL));

  // Register for battery level updates
  battery_state_service_subscribe(battery_callback);
  battery_callback(battery_state_service_peek());

  s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_MONOFONTO_46));
  s_date_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_MONOFONTO_24));

  app_message_register_inbox_received((AppMessageInboxReceived) in_recv_handler);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());

  face_mode = persist_read_int(FACE_MODE);
  show_gifs = persist_read_bool(SHOW_GIFS);

  time_t t = time(NULL);
  struct tm *time_now = localtime(&t);
  tick_handler(time_now, MINUTE_UNIT);

  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_main_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  // Prepare animations
  AnimationImplementation radius_impl = {
    .update = radius_update
  };
  animate(ANIMATION_DURATION, ANIMATION_DELAY, &radius_impl, false);

  AnimationImplementation hands_impl = {
    .update = hands_update
  };
  animate(2 * ANIMATION_DURATION, ANIMATION_DELAY, &hands_impl, true);
}

static void deinit() {
  fonts_unload_custom_font(s_time_font);
  fonts_unload_custom_font(s_date_font);
  battery_state_service_unsubscribe();
  window_destroy(s_main_window);
}

int main() {
  init();
  app_event_loop();
  deinit();
}
