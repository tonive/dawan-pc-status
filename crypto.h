#ifndef PC_STATUS_CRYPTO_H
#define PC_STATUS_CRYPTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PC_KEY_LENGTH 20
#define PC_NONCE_LENGTH 32
#define PC_PROOF_LENGTH 64

void pc_hex(char *out, const uint8_t *bytes, unsigned count);
void pc_sign(char out[PC_PROOF_LENGTH + 1], const char key[PC_KEY_LENGTH],
             const char *kind, const char client[PC_NONCE_LENGTH], const char server[PC_NONCE_LENGTH]);
bool pc_equal(const char *left, const char *right, unsigned count);
void pc_wipe(void *value, size_t count);

#endif
