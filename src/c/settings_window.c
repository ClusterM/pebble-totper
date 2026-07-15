#include "settings_window.h"
#include "pin_window.h"
#include "storage.h"
#include "ui.h"
#include "config.h"

#define MENU_SECTION_MAIN 0
#define MENU_ROW_PIN_ACTION 0
#define MENU_ROW_STATUSBAR_TOGGLE 1
#define MENU_ROW_SYSTEM_INFO 2

typedef enum {
  PIN_MODE_NONE,
  PIN_MODE_SET_FIRST,      // Setting PIN - first entry
  PIN_MODE_SET_CONFIRM,    // Setting PIN - confirmation
  PIN_MODE_DISABLE         // Disabling PIN - verification
} PinMode;

struct SettingsWindow {
  Window *window;
  MenuLayer *menu_layer;
  PinWindow *pin_window;
  Window *info_window;
  TextLayer *info_text_layer;
  PinMode current_mode;
  Pin first_pin;  // Store first PIN entry for confirmation
};

static SettingsWindow *s_settings_window = NULL;

// Forward declarations
static void prv_info_window_load(Window *window);
static void prv_info_window_unload(Window *window);

// ============================================================================
// PIN window callbacks
// ============================================================================

static void prv_pin_setup_complete(Pin pin, void *context) {
  SettingsWindow *settings = (SettingsWindow*)context;
  
  switch (settings->current_mode) {
    case PIN_MODE_SET_FIRST:
      // First PIN entry - save and ask for confirmation
      settings->first_pin = pin;
      settings->current_mode = PIN_MODE_SET_CONFIRM;
      
      pin_window_reset(settings->pin_window);
      pin_window_set_main_text(settings->pin_window, "Confirm PIN");
      pin_window_set_sub_text(settings->pin_window, "Enter PIN again");
      break;
      
    case PIN_MODE_SET_CONFIRM:
      // Second PIN entry - verify match
      if (pin.digits[0] == settings->first_pin.digits[0] &&
          pin.digits[1] == settings->first_pin.digits[1] &&
          pin.digits[2] == settings->first_pin.digits[2]) {
        // PINs match - save it
        storage_set_pin(pin.digits[0], pin.digits[1], pin.digits[2]);
        pin_window_pop(settings->pin_window, true);
        
        vibes_short_pulse();
        APP_LOG(APP_LOG_LEVEL_INFO, "PIN set successfully");
        
        settings->current_mode = PIN_MODE_NONE;
        if (settings->menu_layer) {
          menu_layer_reload_data(settings->menu_layer);
        }
      } else {
        // PINs don't match - start over
        vibes_long_pulse();
        settings->current_mode = PIN_MODE_SET_FIRST;
        
        pin_window_reset(settings->pin_window);
        pin_window_set_main_text(settings->pin_window, "PIN Mismatch");
        pin_window_set_sub_text(settings->pin_window, "Try again");
        
        APP_LOG(APP_LOG_LEVEL_WARNING, "PIN confirmation failed");
      }
      break;
      
    case PIN_MODE_DISABLE:
      // Verify current PIN to disable
      if (storage_verify_pin(pin.digits[0], pin.digits[1], pin.digits[2])) {
        // Correct PIN - disable it
        storage_clear_pin();
        pin_window_pop(settings->pin_window, true);
        
        vibes_double_pulse();
        APP_LOG(APP_LOG_LEVEL_INFO, "PIN disabled");
        
        settings->current_mode = PIN_MODE_NONE;
        if (settings->menu_layer) {
          menu_layer_reload_data(settings->menu_layer);
        }
      } else {
        // Wrong PIN
        vibes_long_pulse();
        
        pin_window_reset(settings->pin_window);
        pin_window_set_main_text(settings->pin_window, "Wrong PIN");
        pin_window_set_sub_text(settings->pin_window, "Try again");
        
        APP_LOG(APP_LOG_LEVEL_WARNING, "PIN verification failed");
      }
      break;
      
    default:
      break;
  }
}

// ============================================================================
// Menu layer callbacks
// ============================================================================

static uint16_t prv_menu_get_num_sections_callback(MenuLayer *menu_layer, void *data) {
  return 1;
}

static uint16_t prv_menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  return 3;  // PIN action, Status Bar toggle, System Info
}

static int16_t prv_menu_get_header_height_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void prv_menu_draw_header_callback(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data) {
  menu_cell_basic_header_draw(ctx, cell_layer, PBL_IF_RECT_ELSE("Settings", "   Settings"));
}

static void prv_menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  bool has_pin = storage_has_pin();
  bool statusbar_enabled = storage_is_statusbar_enabled();
  
  switch (cell_index->row) {
    case MENU_ROW_PIN_ACTION:
      if (has_pin) {
        menu_cell_basic_draw(ctx, cell_layer, "Disable PIN", "Enter PIN to remove", NULL);
      } else {
        menu_cell_basic_draw(ctx, cell_layer, "Set PIN", "Enter PIN twice", NULL);
      }
      break;
      
    case MENU_ROW_STATUSBAR_TOGGLE:
      menu_cell_basic_draw(ctx, cell_layer, "Status Bar", 
                          statusbar_enabled ? "Enabled" : "Disabled", NULL);
      break;
      
    case MENU_ROW_SYSTEM_INFO:
      menu_cell_basic_draw(ctx, cell_layer, "System Info", "Version & Memory", NULL);
      break;
  }
}

static void prv_menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  SettingsWindow *settings = (SettingsWindow*)data;
  bool has_pin = storage_has_pin();
  
  switch (cell_index->row) {
    case MENU_ROW_PIN_ACTION:
      // Create PIN window if it doesn't exist
      if (!settings->pin_window) {
        settings->pin_window = pin_window_create((PinWindowCallbacks){
          .pin_complete = prv_pin_setup_complete
        }, settings);
      }
      
      if (settings->pin_window) {
        pin_window_reset(settings->pin_window);
        
        if (has_pin) {
          // Disable PIN mode
          settings->current_mode = PIN_MODE_DISABLE;
          pin_window_set_main_text(settings->pin_window, "Disable PIN");
          pin_window_set_sub_text(settings->pin_window, "Enter current PIN");
        } else {
          // Set PIN mode - first entry
          settings->current_mode = PIN_MODE_SET_FIRST;
          pin_window_set_main_text(settings->pin_window, "Set PIN");
          pin_window_set_sub_text(settings->pin_window, "Enter new PIN");
        }
        
        pin_window_push(settings->pin_window, true);
      }
      break;
      
    case MENU_ROW_STATUSBAR_TOGGLE:
      // Toggle status bar setting
      {
        bool current = storage_is_statusbar_enabled();
        storage_set_statusbar_enabled(!current);
        
        // Reload menu to show new status
        menu_layer_reload_data(menu_layer);
        
        // Reload main window to apply changes
        ui_reload_window();
        
        vibes_short_pulse();
      }
      break;
      
    case MENU_ROW_SYSTEM_INFO:
      // Create and show system info window
      if (!settings->info_window) {
        settings->info_window = window_create();
        if (settings->info_window) {
          window_set_user_data(settings->info_window, settings);
          window_set_window_handlers(settings->info_window, (WindowHandlers){
            .load = prv_info_window_load,
            .unload = prv_info_window_unload,
          });
        }
      }
      
      if (settings->info_window) {
        window_stack_push(settings->info_window, true);
      }
      break;
  }
}

// ============================================================================
// System Information window
// ============================================================================

static void prv_info_window_load(Window *window) {
  SettingsWindow *settings = window_get_user_data(window);
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  const GFont info_font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  
  // Create text layer for system info
  settings->info_text_layer = text_layer_create(GRect(5, 10, bounds.size.w - 10, bounds.size.h - 20));
  if (!settings->info_text_layer) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create info text layer");
    window_destroy(settings->info_window);
    settings->info_window = NULL;
    return;
  }
  text_layer_set_font(settings->info_text_layer, info_font);
  text_layer_set_text_alignment(settings->info_text_layer, PBL_IF_RECT_ELSE(GTextAlignmentLeft, GTextAlignmentCenter));
  text_layer_set_overflow_mode(settings->info_text_layer, GTextOverflowModeWordWrap);
  
  // Format system information
  static char info_buffer[256];
  size_t heap_used = heap_bytes_used();
  size_t heap_free = heap_bytes_free();
  
  snprintf(info_buffer, sizeof(info_buffer),
    "Version: %s\n\n"
    "Account count: %zu\n\n"
    "Memory used: %zu B\n"
    "Memory free: %zu B\n",
    VERSION,
    storage_get_count(),
    heap_used,
    heap_free
  );
  
  text_layer_set_text(settings->info_text_layer, info_buffer);
  GSize content_size = graphics_text_layout_get_content_size(
    info_buffer,
    info_font,
    GRect(0, 0, bounds.size.w - 10, bounds.size.h),
    GTextOverflowModeWordWrap,
    PBL_IF_RECT_ELSE(GTextAlignmentLeft, GTextAlignmentCenter)
  );
  int16_t content_height = content_size.h;
  int16_t top_margin = (bounds.size.h - content_height) / 2;
  if (top_margin < 0) {
    top_margin = 0;
  }
  layer_set_frame(text_layer_get_layer(settings->info_text_layer), GRect(5, top_margin, bounds.size.w - 10, content_height));
  layer_add_child(window_layer, text_layer_get_layer(settings->info_text_layer));
}

static void prv_info_window_unload(Window *window) {
  SettingsWindow *settings = window_get_user_data(window);
  
  if (settings->info_text_layer) {
    text_layer_destroy(settings->info_text_layer);
    settings->info_text_layer = NULL;
  }
}

// ============================================================================
// Window lifecycle
// ============================================================================

static void prv_window_load(Window *window) {
  SettingsWindow *settings = window_get_user_data(window);
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  // Create menu layer
  settings->menu_layer = menu_layer_create(bounds);
  if (!settings->menu_layer) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create menu layer");
    return;
  }
  menu_layer_set_callbacks(settings->menu_layer, settings, (MenuLayerCallbacks){
    .get_num_sections = prv_menu_get_num_sections_callback,
    .get_num_rows = prv_menu_get_num_rows_callback,
    .get_header_height = prv_menu_get_header_height_callback,
    .draw_header = prv_menu_draw_header_callback,
    .draw_row = prv_menu_draw_row_callback,
    .select_click = prv_menu_select_callback,
  });
  
  menu_layer_set_click_config_onto_window(settings->menu_layer, window);
  layer_add_child(window_layer, menu_layer_get_layer(settings->menu_layer));
}

static void prv_window_unload(Window *window) {
  SettingsWindow *settings = window_get_user_data(window);
  
  // Only destroy menu_layer here, as it's created in prv_window_load
  if (settings->menu_layer) {
    menu_layer_destroy(settings->menu_layer);
    settings->menu_layer = NULL;
  }
  
  // Note: pin_window and info_window are NOT destroyed here
  // They are created on-demand and will be cleaned up in settings_window_destroy
}

// ============================================================================
// Public API
// ============================================================================

SettingsWindow* settings_window_create(void) {
  if (s_settings_window) {
    return s_settings_window;
  }
  
  SettingsWindow *settings = malloc(sizeof(SettingsWindow));
  if (!settings) return NULL;
  
  memset(settings, 0, sizeof(SettingsWindow));
  
  settings->window = window_create();
  if (!settings->window) {
    free(settings);
    return NULL;
  }
  
  window_set_user_data(settings->window, settings);
  window_set_window_handlers(settings->window, (WindowHandlers){
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  
  s_settings_window = settings;
  return settings;
}

void settings_window_destroy(SettingsWindow *settings_window) {
  if (!settings_window) return;
  
  // Clean up pin window if it exists
  if (settings_window->pin_window) {
    pin_window_destroy(settings_window->pin_window);
    settings_window->pin_window = NULL;
  }
  
  // Clean up info window if it exists
  if (settings_window->info_window) {
    window_destroy(settings_window->info_window);
    settings_window->info_window = NULL;
  }
  
  // Clean up main window
  if (settings_window->window) {
    window_destroy(settings_window->window);
  }
  
  if (s_settings_window == settings_window) {
    s_settings_window = NULL;
  }
  
  free(settings_window);
}

void settings_window_push(SettingsWindow *settings_window, bool animated) {
  if (settings_window && settings_window->window) {
    window_stack_push(settings_window->window, animated);
  }
}

void settings_window_pop(SettingsWindow *settings_window, bool animated) {
  if (settings_window && settings_window->window) {
    window_stack_remove(settings_window->window, animated);
  }
}

