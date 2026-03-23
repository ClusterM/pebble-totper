#include "storage.h"
#include "ui.h"
#include "config.h"
#include <string.h>

typedef struct {
  char label[LABEL_MAX_LEN + 1];
  char account_name[ACCOUNT_NAME_MAX_LEN + 1];
  uint8_t secret_len;
  uint8_t secret[SECRET_BYTES_MAX];
  uint16_t period;
  uint8_t digits;
  uint8_t algorithm;  // TotpAlgorithm
} __attribute__((__packed__)) PersistedAccount;

#ifdef DEBUG
// Create fake account for debug mode and screenshots
static void prv_create_fake_account(size_t id, TotpAccount *account) {
  memset(account, 0, sizeof(*account));
  
  // Base32 test secret: "JBSWY3DPEHPK3PXP" = "Hello!" in bytes
  const uint8_t test_secret[] = {0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x21};
  const uint8_t test_secret2[] = {0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x22};
  const uint8_t test_secret3[] = {0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x23};
  const uint8_t test_secret4[] = {0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x24};
  const uint8_t test_secret5[] = {0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x25};
  const uint8_t test_secret6[] = {0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x26};
  
  switch (id % 6) {
    case 0:
      strncpy(account->label, "GitHub", sizeof(account->label) - 1);
      strncpy(account->account_name, "user@example.com", sizeof(account->account_name) - 1);
      memcpy(account->secret, test_secret, sizeof(test_secret));
      account->secret_len = sizeof(test_secret);
      account->period = 30;
      account->digits = 6;
      account->algorithm = TOTP_ALGO_SHA1;
      break;
      
    case 1:
      strncpy(account->label, "Google", sizeof(account->label) - 1);
      strncpy(account->account_name, "test@gmail.com", sizeof(account->account_name) - 1);
      memcpy(account->secret, test_secret2, sizeof(test_secret2));
      account->secret_len = sizeof(test_secret2);
      account->period = 30;
      account->digits = 6;
      account->algorithm = TOTP_ALGO_SHA1;
      break;
      
    case 2:
      strncpy(account->label, "Microsoft", sizeof(account->label) - 1);
      strncpy(account->account_name, "work@outlook.com", sizeof(account->account_name) - 1);
      memcpy(account->secret, test_secret3, sizeof(test_secret3));
      account->secret_len = sizeof(test_secret3);
      account->period = 30;
      account->digits = 8;
      break;
      
    case 3:
      strncpy(account->label, "Amazon", sizeof(account->label) - 1);
      strncpy(account->account_name, "", sizeof(account->account_name) - 1);
      memcpy(account->secret, test_secret4, sizeof(test_secret4));
      account->secret_len = sizeof(test_secret4);
      account->period = 30;
      account->digits = 6;
      account->algorithm = TOTP_ALGO_SHA1;
      break;
      
    case 4:
      strncpy(account->label, "Custom Service", sizeof(account->label) - 1);
      strncpy(account->account_name, "admin", sizeof(account->account_name) - 1);
      memcpy(account->secret, test_secret5, sizeof(test_secret5));
      account->secret_len = sizeof(test_secret5);
      account->period = 60;
      account->digits = 6;
      break;
      
    default:
      strncpy(account->label, "Test Account", sizeof(account->label) - 1);
      strncpy(account->account_name, "", sizeof(account->account_name) - 1);
      memcpy(account->secret, test_secret6, sizeof(test_secret6));
      account->secret_len = sizeof(test_secret);
      account->period = 30;
      account->digits = 6;
      account->algorithm = TOTP_ALGO_SHA1;
      break;
  }
}
#endif

// Get account count
size_t storage_get_count(void) {
#ifdef DEBUG
  return DEBUG_ACCOUNTS;
#else
  if (!persist_exists(PERSIST_KEY_COUNT)) {
    return 0;
  }
  return (size_t)persist_read_int(PERSIST_KEY_COUNT);
#endif
}

// Set account count
void storage_set_count(size_t count) {
  persist_write_int(PERSIST_KEY_COUNT, count);
}

// Load account by ID
bool storage_load_account(size_t id, TotpAccount *account) {
  if (!account) return false;

#ifdef DEBUG
  if (id >= DEBUG_ACCOUNTS) {
    return false;
  }
  prv_create_fake_account(id, account);
  return true;
#else
  uint32_t key = PERSIST_KEY_ACCOUNTS_START + id;
  if (!persist_exists(key)) {
    return false;
  }

  PersistedAccount data;
  persist_read_data(key, &data, sizeof(data));

  memset(account, 0, sizeof(*account));
  strncpy(account->label, data.label, sizeof(account->label) - 1);
  strncpy(account->account_name, data.account_name, sizeof(account->account_name) - 1);
  account->secret_len = data.secret_len;
  if (account->secret_len > SECRET_BYTES_MAX) {
    account->secret_len = SECRET_BYTES_MAX;
  }
  memcpy(account->secret, data.secret, account->secret_len);
  account->period = data.period > 0 ? data.period : DEFAULT_PERIOD;
  account->digits = data.digits >= MIN_DIGITS && data.digits <= MAX_DIGITS ? data.digits : DEFAULT_DIGITS;
  account->algorithm = (data.algorithm <= TOTP_ALGO_SHA512) ? data.algorithm : TOTP_ALGO_SHA1;

  return true;
#endif
}

// Save account by ID
bool storage_save_account(size_t id, const TotpAccount *account) {
  if (!account) return false;

  PersistedAccount data;
  memset(&data, 0, sizeof(data));
  strncpy(data.label, account->label, sizeof(data.label) - 1);
  strncpy(data.account_name, account->account_name, sizeof(data.account_name) - 1);
  data.secret_len = account->secret_len;
  memcpy(data.secret, account->secret, account->secret_len);
  data.period = account->period;
  data.digits = account->digits;
  data.algorithm = account->algorithm;

  uint32_t key = PERSIST_KEY_ACCOUNTS_START + id;
  return persist_write_data(key, &data, sizeof(data)) == sizeof(data);
}

// Delete account by ID
void storage_delete_account(size_t id) {
  uint32_t key = PERSIST_KEY_ACCOUNTS_START + id;
  persist_delete(key);
}

// Load account count from storage
void storage_load_accounts(void) {
  s_total_account_count = storage_get_count();
}

// ============================================================================
// PIN management
// ============================================================================

// Simple hash function for 3-digit PIN (0-999)
static uint32_t prv_hash_pin(int digit1, int digit2, int digit3) {
  uint32_t pin = (digit1 * 100) + (digit2 * 10) + digit3;
  // Simple XOR-based hash with some mixing
  uint32_t hash = pin;
  hash ^= (hash << 13);
  hash ^= (hash >> 17);
  hash ^= (hash << 5);
  hash ^= 0xDEADBEEF;  // Salt
  return hash;
}

bool storage_has_pin(void) {
  return persist_exists(PERSIST_KEY_PIN_HASH);
}

uint32_t storage_get_pin_hash(void) {
  if (!persist_exists(PERSIST_KEY_PIN_HASH)) {
    return 0;
  }
  return (uint32_t)persist_read_int(PERSIST_KEY_PIN_HASH);
}

void storage_set_pin(int pin_digit1, int pin_digit2, int pin_digit3) {
  uint32_t hash = prv_hash_pin(pin_digit1, pin_digit2, pin_digit3);
  persist_write_int(PERSIST_KEY_PIN_HASH, hash);
}

bool storage_verify_pin(int pin_digit1, int pin_digit2, int pin_digit3) {
  if (!storage_has_pin()) {
    return true;  // No PIN set, always valid
  }
  
  uint32_t entered_hash = prv_hash_pin(pin_digit1, pin_digit2, pin_digit3);
  uint32_t stored_hash = storage_get_pin_hash();
  
  return entered_hash == stored_hash;
}

void storage_clear_pin(void) {
  persist_delete(PERSIST_KEY_PIN_HASH);
}

// ============================================================================
// Status bar management
// ============================================================================

bool storage_is_statusbar_enabled(void) {
  if (!persist_exists(PERSIST_KEY_STATUSBAR_ENABLED)) {
    return true;  // Default: enabled
  }
  return (bool)persist_read_bool(PERSIST_KEY_STATUSBAR_ENABLED);
}

void storage_set_statusbar_enabled(bool enabled) {
  persist_write_bool(PERSIST_KEY_STATUSBAR_ENABLED, enabled);
}

