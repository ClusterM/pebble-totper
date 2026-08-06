#include <pebble.h>
#include "ui.h"
#include "storage.h"
#include "comms.h"
#include "config.h"
#include "pin_window.h"

// === Global variables =======================================================

static PinWindow *s_pin_window = NULL;
static bool s_pin_verified = false;
static int s_pin_attempts = 0;

// === PIN window callbacks ===================================================

static void prv_pin_complete_handler(Pin pin, void *context) {
  if (storage_verify_pin(pin.digits[0], pin.digits[1], pin.digits[2])) {
    s_pin_verified = true;
    ui_init();
    pin_window_pop(s_pin_window, true);
    comms_init();
    
    APP_LOG(APP_LOG_LEVEL_INFO, "PIN verified successfully");
  } else {
    s_pin_attempts++;
    
    // Reset PIN input
    pin_window_reset(s_pin_window);
    pin_window_set_main_text(s_pin_window, "Wrong PIN!");
    pin_window_set_sub_text(s_pin_window, "Try again");
    
    // Vibrate to indicate error
    vibes_double_pulse();
    
    APP_LOG(APP_LOG_LEVEL_WARNING, "PIN verification failed (attempt %d)", s_pin_attempts);
    
    // After 3 failed attempts, wipe data and close the app
    if (s_pin_attempts >= 3) {
      pin_window_set_main_text(s_pin_window, "Too many attempts!");
      pin_window_set_sub_text(s_pin_window, "Wiping data...");
      
      // Wipe all TOTP account data
      storage_wipe_all_accounts();
      
      // Schedule app exit after a short delay
      app_timer_register(2000, (AppTimerCallback)window_stack_pop_all, NULL);
    }
  }
}

// === App lifecycle ==========================================================

static void prv_init(void) {
#ifdef DEBUG
  APP_LOG(APP_LOG_LEVEL_WARNING, "========================================");
  APP_LOG(APP_LOG_LEVEL_WARNING, "DEBUG MODE IS ENABLED!");
  APP_LOG(APP_LOG_LEVEL_WARNING, "Using fake TOTP accounts for testing");
  APP_LOG(APP_LOG_LEVEL_WARNING, "========================================");
#endif
  
  // Load account count
  storage_load_accounts();
  ui_set_total_count(s_total_account_count);
 
  // Check if PIN is enabled
  if (storage_has_pin()) {
    s_pin_verified = false;
    s_pin_attempts = 0;
    
    // Create and show PIN window
    s_pin_window = pin_window_create((PinWindowCallbacks){
      .pin_complete = prv_pin_complete_handler
    }, NULL);
    
    if (s_pin_window) {
      pin_window_push(s_pin_window, true);
    }
  } else {
    s_pin_verified = true;  // No PIN required
    ui_init();
    comms_init();
  }
}

static void prv_deinit(void) {
  if (s_pin_window) {
    pin_window_destroy(s_pin_window);
    s_pin_window = NULL;
  }
  
  comms_deinit();
  ui_deinit();
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
  return 0;
}