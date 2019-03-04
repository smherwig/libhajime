#ifndef _HAJIME_H_
#define _HAJIME_H_

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "rho/rho_decls.h"

RHO_DECLS_BEGIN;

#define HAJIME_MSGTYPE_KEY_EXCHANGE ((uint8_t)0)
#define HAJIME_MSGTYPE_FILE_REQUEST  ((uint8_t)2)
#define HAJIME_MSGTYPE_FILE_CONTENT  ((uint8_t)3)

#define HAJIME_KEY_LENGTH 32

enum hajime_payload_type {
    HAJIME_PAYLOAD_TYPE_CONFIG = 0,
    HAJIME_PAYLOAD_TYPE_STAGE2_UPDATE,
    HAJIME_PAYLOAD_TYPE_EXECUTABLE_MODULE
};

struct hajime_payload_header {
    char filename[32]; /* nul- terminated */
    bool lz4_compressed;
    enum hajime_payload_type type;
    time_t creation_time;
    uint32_t  body_size;        /* might not need this one */
    uint8_t key[1024];
    uint32_t body_size_decompressed;
    uint32_t body_size_compressed;
};

struct hajime_bot {
    int xfd;
    uint8_t pubkey[HAJIME_KEY_LENGTH];
    /* TODO: keypair, rc4key, timeout */
};


/* return 0 for success, -1 for failure -- to stop */
typedef int (*hajime_download_cb) (void *userdata, uint8_t *buf, size_t size);

void hajime_bot_libinit(void);
void hajime_bot_libfini(void);

/* timeout is NULL for no timeout (blocking) */
struct hajime_bot * hajime_bot_create_client(struct timespec *timeout);
void hajime_bot_destroy(struct hajime_bot *bot);
int hajime_bot_connect(struct hajime_bot *bot, const char *ipstr, uint16_t port);
int hajime_bot_file_request(struct hajime_bot *bot, const char *filename,
        hajime_download_cb cb, void *userdata);
int hajime_bot_key_exchange(struct hajime_bot *bot, uint8_t *peerpubkey);
int hajime_bot_close(struct hajime_bot *bot);

int hajime_decode_payload(uint8_t *payload, size_t payload_len, 
        struct hajime_payload_header *header, 
        uint8_t **body, size_t *body_len);

const char * hajime_payload_type_to_string(enum hajime_payload_type type);

RHO_DECLS_END;

#endif /* ! _HAJIME_H_ */
