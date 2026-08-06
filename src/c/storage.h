#pragma once

#include "totp.h"

#define PERSIST_KEY_COUNT 0
#define PERSIST_KEY_PIN_HASH 2
#define PERSIST_KEY_STATUSBAR_ENABLED 3
#define PERSIST_KEY_ACCOUNTS_START 8

// Get account count
size_t storage_get_count(void);

// Set account count
void storage_set_count(size_t count);

// Load account by ID
bool storage_load_account(size_t id, TotpAccount *account);

// Save account by ID
bool storage_save_account(size_t id, const TotpAccount *account);

// Delete account by ID
void storage_delete_account(size_t id);

// Load account count from storage
void storage_load_accounts(void);

// PIN management
bool storage_has_pin(void);
uint32_t storage_get_pin_hash(void);
void storage_set_pin(int pin_digit1, int pin_digit2, int pin_digit3);
bool storage_verify_pin(int pin_digit1, int pin_digit2, int pin_digit3);
void storage_clear_pin(void);

// Wipe all accounts from storage
void storage_wipe_all_accounts(void);

// Status bar management
bool storage_is_statusbar_enabled(void);
void storage_set_statusbar_enabled(bool enabled);

