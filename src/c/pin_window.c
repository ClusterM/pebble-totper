#include "pin_window.h"

#define PIN_WINDOW_SIZE GSize(130, 40)

static char* prv_selection_handle_get_text(int index, void *context) {
  PinWindow *pin_window = (PinWindow*)context;
  snprintf(
    pin_window->field_buffs[index], 
    sizeof(pin_window->field_buffs[0]), "%d",
    (int)pin_window->pin.digits[index]
  );
  return pin_window->field_buffs[index];
}

static void prv_selection_handle_complete(void *context) {
  PinWindow *pin_window = (PinWindow*)context;
  if (pin_window->callbacks.pin_complete) {
    pin_window->callbacks.pin_complete(pin_window->pin, pin_window->callback_context);
  }
}

static void prv_selection_handle_inc(int index, uint8_t clicks, void *context) {
  PinWindow *pin_window = (PinWindow*)context;
  pin_window->pin.digits[index]++;
  if (pin_window->pin.digits[index] > PIN_WINDOW_MAX_VALUE) {
    pin_window->pin.digits[index] = 0;
  }
}

static void prv_selection_handle_dec(int index, uint8_t clicks, void *context) {
  PinWindow *pin_window = (PinWindow*)context;
  pin_window->pin.digits[index]--;
  if (pin_window->pin.digits[index] < 0) {
    pin_window->pin.digits[index] = PIN_WINDOW_MAX_VALUE;
  }
}

PinWindow* pin_window_create(PinWindowCallbacks callbacks, void *context) {
  PinWindow *pin_window = (PinWindow*)malloc(sizeof(PinWindow));
  if (!pin_window) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to allocate PinWindow");
    return NULL;
  }
  
  memset(pin_window, 0, sizeof(PinWindow));
  pin_window->callbacks = callbacks;
  pin_window->callback_context = context;
  pin_window->highlight_color = GColorCobaltBlue;
  
  pin_window->window = window_create();
  if (!pin_window->window) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create window");
    free(pin_window);
    return NULL;
  }
  
  pin_window->field_selection = 0;
  for (int i = 0; i < PIN_WINDOW_NUM_CELLS; i++) {
    pin_window->pin.digits[i] = 0;
  }
  
  // Get window parameters
  Layer *window_layer = window_get_root_layer(pin_window->window);
  GRect bounds = layer_get_bounds(window_layer);
  
  // Main TextLayer
  const GEdgeInsets main_text_insets = {.top = 30};
  pin_window->main_text = text_layer_create(grect_inset(bounds, main_text_insets));
  if (!pin_window->main_text) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create main text layer");
    window_destroy(pin_window->window);
    free(pin_window);
    return NULL;
  }
  text_layer_set_text(pin_window->main_text, "PIN Required");
  text_layer_set_font(pin_window->main_text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(pin_window->main_text, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(pin_window->main_text));
  
  // Sub TextLayer
  const GEdgeInsets sub_text_insets = {.top = (bounds.size.h + PIN_WINDOW_SIZE.h) / 2 + 10, .left = 0, .bottom = 0, .right = 0};
  pin_window->sub_text = text_layer_create(grect_inset(bounds, sub_text_insets));
  if (!pin_window->sub_text) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create sub text layer");
    text_layer_destroy(pin_window->main_text);
    window_destroy(pin_window->window);
    free(pin_window);
    return NULL;
  }
  text_layer_set_text(pin_window->sub_text, "Enter your PIN");
  text_layer_set_text_alignment(pin_window->sub_text, GTextAlignmentCenter);
  text_layer_set_font(pin_window->sub_text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(pin_window->sub_text));
  
  // Create selection layer
  const GEdgeInsets selection_insets = GEdgeInsets(
    (bounds.size.h - PIN_WINDOW_SIZE.h) / 2, 
    (bounds.size.w - PIN_WINDOW_SIZE.w) / 2,
    (bounds.size.h - PIN_WINDOW_SIZE.h) / 2,
    (bounds.size.w - PIN_WINDOW_SIZE.w) / 2
  );
  pin_window->selection = selection_layer_create(grect_inset(bounds, selection_insets), PIN_WINDOW_NUM_CELLS);
  if (!pin_window->selection) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create selection layer");
    text_layer_destroy(pin_window->sub_text);
    text_layer_destroy(pin_window->main_text);
    window_destroy(pin_window->window);
    free(pin_window);
    return NULL;
  }
  
  for (int i = 0; i < PIN_WINDOW_NUM_CELLS; i++) {
    selection_layer_set_cell_width(pin_window->selection, i, 40);
  }
  selection_layer_set_cell_padding(pin_window->selection, 4);
  selection_layer_set_active_bg_color(pin_window->selection, pin_window->highlight_color);
  selection_layer_set_inactive_bg_color(pin_window->selection, GColorDarkGray);
  selection_layer_set_click_config_onto_window(pin_window->selection, pin_window->window);
  selection_layer_set_callbacks(pin_window->selection, pin_window, (SelectionLayerCallbacks) {
    .get_cell_text = prv_selection_handle_get_text,
    .complete = prv_selection_handle_complete,
    .increment = prv_selection_handle_inc,
    .decrement = prv_selection_handle_dec,
  });
  layer_add_child(window_layer, selection_layer_get_layer(pin_window->selection));

  // Create status bar
  pin_window->status = status_bar_layer_create();
  if (!pin_window->status) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create status bar");
    selection_layer_destroy(pin_window->selection);
    text_layer_destroy(pin_window->sub_text);
    text_layer_destroy(pin_window->main_text);
    window_destroy(pin_window->window);
    free(pin_window);
    return NULL;
  }
  status_bar_layer_set_colors(pin_window->status, GColorClear, GColorBlack);
  layer_add_child(window_layer, status_bar_layer_get_layer(pin_window->status));
  
  return pin_window;
}

void pin_window_destroy(PinWindow *pin_window) {
  if (!pin_window) return;
  
  if (pin_window->status) {
    status_bar_layer_destroy(pin_window->status);
  }
  
  if (pin_window->selection) {
    selection_layer_destroy(pin_window->selection);
  }
  
  if (pin_window->sub_text) {
    text_layer_destroy(pin_window->sub_text);
  }
  
  if (pin_window->main_text) {
    text_layer_destroy(pin_window->main_text);
  }
  
  if (pin_window->window) {
    window_destroy(pin_window->window);
  }
  
  free(pin_window);
}

void pin_window_push(PinWindow *pin_window, bool animated) {
  if (pin_window && pin_window->window) {
    window_stack_push(pin_window->window, animated);
  }
}

void pin_window_pop(PinWindow *pin_window, bool animated) {
  if (pin_window && pin_window->window) {
    window_stack_remove(pin_window->window, animated);
  }
}

bool pin_window_get_topmost_window(PinWindow *pin_window) {
  if (!pin_window || !pin_window->window) return false;
  return window_stack_get_top_window() == pin_window->window;
}

void pin_window_set_main_text(PinWindow *pin_window, const char *text) {
  if (pin_window && pin_window->main_text && text) {
    text_layer_set_text(pin_window->main_text, text);
  }
}

void pin_window_set_sub_text(PinWindow *pin_window, const char *text) {
  if (pin_window && pin_window->sub_text && text) {
    text_layer_set_text(pin_window->sub_text, text);
  }
}

void pin_window_reset(PinWindow *pin_window) {
  if (!pin_window) return;
  
  for (int i = 0; i < PIN_WINDOW_NUM_CELLS; i++) {
    pin_window->pin.digits[i] = 0;
  }
  
  if (pin_window->selection) {
    layer_mark_dirty(selection_layer_get_layer(pin_window->selection));
  }
}

