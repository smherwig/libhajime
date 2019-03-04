#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <string.h>

#include <lz4.h>

#include <rho/rho.h>
#include <xutp.h>

#include "hajime.h"

#define CAST_UINT32(b, off) (*((uint32_t *)((b) + (off))))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define HAJIME_FILE_REQUEST_MESSAGE_MAXSIZE (4 + 1 + 32)
#define HAJIME_KEY_EXCHANGE_MESSAGE_SIZE    (4 + 1 + 1 + 32)

/* a 'recvall' function */
static int
hajime_recvn(int xfd, uint8_t *buffer, size_t n)
{
    ssize_t nr = 0;
    size_t tot = 0;
    uint8_t *buf = NULL;

    /* I think you're getting an infinite loop here because xutp_recv
     * isn't returning 0 on close
     */
    buf = buffer;
    for (tot = 0; tot < n; ) {
        nr = xutp_recv(xfd, buf, n - tot);

        if (nr == 0)
            return (tot);   /* EOF */

        if (nr < 0)
            return (nr);    /* error */

        tot += nr;
        buf += nr;
    }

    return (tot);
}

/* TODO: have a hajime_message_type_to_string function */

const char *
hajime_payload_type_to_string(enum hajime_payload_type type)
{
    switch (type) {
    case HAJIME_PAYLOAD_TYPE_CONFIG:
        return "config";
    case HAJIME_PAYLOAD_TYPE_STAGE2_UPDATE:
        return "stage2_update";
    case HAJIME_PAYLOAD_TYPE_EXECUTABLE_MODULE:
        return "executable_module";
    default:
        return "unknown";
    }
}

/* TODO: lz4 decompression, if needed */
int
hajime_decode_payload(uint8_t *payload, size_t payload_len, 
        struct hajime_payload_header *header,
        uint8_t **body, size_t *body_len)
{
    int error = 0;
    uint8_t *compressed_payload = NULL;
    uint32_t offset = 0;

    *body = NULL;
    *body_len = 0;
    memset(header, 0x00, sizeof(*header));

    if (payload_len < 0xB4) {
        rho_warn("payload is %lu bytes, but must be at least %lu\n",
                (unsigned long)payload_len, (unsigned long)0xB4);
        error = -1;
        goto fail;
    }

    memcpy(&header->filename, payload, 0x20);
    header->lz4_compressed = payload[0x20];
    header->type = payload[0x22];
    header->creation_time = ntohl(CAST_UINT32(payload,0x24));
    header->body_size = ntohl(CAST_UINT32(payload, 0x28));
    memcpy(&header->key, payload + 0x2c, 128);

    if (header->lz4_compressed) {
        header->body_size_decompressed = ntohl(CAST_UINT32(payload, 0xac));
        header->body_size_compressed = ntohl(CAST_UINT32(payload, 0xb0));
        compressed_payload = payload + 0xb4;
        *body = rhoL_zalloc(header->body_size_decompressed);

        fprintf(stderr, "%02x %u %u\n", compressed_payload[0],
                header->body_size_compressed, header->body_size_decompressed);

        /* usually -12 for config, -8 for atk, although there are a few samples
         * where I had to manually set to a different value */
        if (header->type == HAJIME_PAYLOAD_TYPE_CONFIG)
            offset = 12;
        else
            offset = 8;

        error = LZ4_decompress_safe((char *)compressed_payload, (char *)*body,
                header->body_size_compressed - offset, header->body_size_decompressed);
        if (error < 0) {
            rho_warn("LZ4_decompress_safe failed: returned %d\n", error);
            goto fail;
        }

        *body_len = error;
    }

    goto succeed;

fail:
    if (*body != NULL) {
        rhoL_free(*body);
        *body = NULL;
    }
    *body_len = 0;

succeed:
    return (error);
}

void
hajime_bot_libinit(void)
{
    xutp_init();
}

void
hajime_bot_libfini(void)
{
    xutp_fini();
}

struct hajime_bot *
hajime_bot_create_client(struct timespec *timeout)
{
    struct hajime_bot *bot = NULL;

    bot = rhoL_zalloc(sizeof(*bot));
    rho_rand_randombytes(bot->pubkey, HAJIME_KEY_LENGTH);
    bot->xfd = xutp_newsock(NULL, 0);
    
    if (timeout != NULL)
        (void)xutp_settimeout(bot->xfd, timeout);

    return (bot);
}

void
hajime_bot_destroy(struct hajime_bot *bot)
{
    rhoL_free(bot);
}

int
hajime_bot_connect(struct hajime_bot *bot, const char *ipstr, uint16_t port)
{
    int error = 0;
    struct sockaddr_in saddr;

    memset(&saddr, 0x00, sizeof(saddr));
    saddr.sin_family = AF_INET;
    if (inet_pton(AF_INET, ipstr, &saddr.sin_addr) != 1) {
        rho_warn("inet_pton('%s') failed", ipstr);
        goto fail;
    }
    saddr.sin_port = htons(port);
    error = xutp_connect(bot->xfd, (struct sockaddr *)&saddr, sizeof(saddr));
    if (error != 0) {
        //rho_warn("xutp_connect returned %d", error);
        goto fail;
    }

    goto succeed;

fail:
    error = -1;
succeed:
    return (error);
}

size_t
hajime_file_request_message_init(uint8_t *buf, size_t buflen,
        const char *filename)
{
    size_t namelen = 0;
    size_t msglen = 0;

    namelen = strlen(filename);
    msglen = 4 + 1 + namelen + 1;

    if (msglen > buflen) {
        rho_warn("file request buf must be %zu bytes but is only %zu",
                msglen, buflen);
        goto fail;
    }

    *((uint32_t *)buf) = htonl(namelen + 1);/* payload length */
    buf[4] = HAJIME_MSGTYPE_FILE_REQUEST;   /* message  type */
    memcpy(buf + 5, filename, namelen);     /* payload */

    return (msglen);

fail:
    return (0);
}

static size_t
hajime_key_exchange_message_init(uint8_t *buf, size_t buflen,
        bool have_peers_key, uint8_t *pubkey)
{
    *((uint32_t *)buf) = htonl(33);     /* payload length */
    buf[4]= HAJIME_MSGTYPE_KEY_EXCHANGE; /* message type */
    buf[5] = have_peers_key ? 0x01 : 0x00;
    memcpy(buf + 6, pubkey, HAJIME_KEY_LENGTH);

    return  (HAJIME_KEY_EXCHANGE_MESSAGE_SIZE);
}

int
hajime_bot_file_request(struct hajime_bot *bot, const char *filename,
        hajime_download_cb cb, void *userdata)
{
    int error = 0;
    uint8_t msg[HAJIME_FILE_REQUEST_MESSAGE_MAXSIZE] = { 0 };
    size_t msglen = 0;
    uint8_t buf[512] = { 0 };
    int ngot = 0;
    uint32_t payload_size = 0;
    uint32_t n = 0;
    uint8_t msgtype = 0;

    msglen = hajime_file_request_message_init(msg, sizeof(msg), filename);

    /* TODO: make sure we send all of our data -- we don't send much, so this
     * is low  priority
     */
    error = xutp_send(bot->xfd, msg, msglen);
    if (error == -1) {
        rho_warn("xutp_send returned %d", error);
        goto fail;
    }

    ngot = hajime_recvn(bot->xfd, buf, 5);
    if (ngot != 5) {
        rho_warn("expected to receive 5 bytes; only got %d\n", ngot);
        goto fail;
    }

    payload_size = ntohl(*(uint32_t *)buf);
    msgtype = buf[4];
    if (msgtype != HAJIME_MSGTYPE_FILE_CONTENT) {
        rho_warn("expected a FILECONTENT message type; got msg code=%u\n", msgtype);
        goto fail;
    }

    while (n < payload_size) {
        ngot = hajime_recvn(bot->xfd, buf, MIN(sizeof(buf), payload_size - n)); 
        if (ngot == -1) {
            rho_warn("xutp_recvn returned -1");
            goto fail;
        }
        if (cb(userdata, buf, ngot) == 0)
            goto fail;

        n += ngot;
        fprintf(stderr, "++++ (%d) %lu / %lu\n", ngot, (unsigned long)n, (unsigned long)payload_size);
    }

    goto succeed;

fail:
    error = -1;
succeed:
    return (error);
}

int
hajime_bot_key_exchange(struct hajime_bot *bot, uint8_t *peerpubkey)
{
    int error = 0;
    uint8_t msg[HAJIME_KEY_EXCHANGE_MESSAGE_SIZE] = { 0 };
    size_t msglen = 0;
    uint8_t buf[512] = { 0 };
    int ngot = 0;
    uint32_t payload_size = 0;
    uint32_t n = 0;
    uint8_t msgtype = 0;

    msglen = hajime_key_exchange_message_init(msg, sizeof(msg), false,
            bot->pubkey);

    error = xutp_send(bot->xfd, msg, msglen);
    if (error == -1) {
        rho_warn("xutp_send returned %d", error);
        goto fail;
    }

    /* receive payload header */
    ngot = hajime_recvn(bot->xfd, buf, 5);
    if (ngot != 5) {
        //rho_warn("expected to receive 5 bytes; only got %d", ngot);
        goto fail;
    }

    payload_size = ntohl(*(uint32_t *)buf);
    if (payload_size < HAJIME_KEY_LENGTH) {
        rho_warn("expected KEY_EXCHANGE payload >= %d bytes; only got %" PRIu32,
                HAJIME_KEY_LENGTH, payload_size);
        goto fail;
    }

    msgtype = buf[4];
    if (msgtype != HAJIME_MSGTYPE_KEY_EXCHANGE) {
        rho_warn("expected KEY_EXCHANGE message type; got msg code=%u", msgtype);
        goto fail;
    }

    /* get 1-byte ack of my key + peer's key from payload body */
    ngot = hajime_recvn(bot->xfd, buf, HAJIME_KEY_LENGTH + 1);
    if (ngot != 33) {
        rho_warn("expected to receive 33 bytes; only got %d", ngot);
        goto fail;
    }

    if (buf[0] != 0x01) {
        rho_warn("expected peer's KEY_EXCHANGE to ack my key, but did not");
        goto fail;
    }

    memcpy(peerpubkey, buf + 1, HAJIME_KEY_LENGTH);

    /* read any trailing garbage bytes in payload body */
    n = ngot;
    while (n < payload_size) {
        ngot = hajime_recvn(bot->xfd, buf, MIN(sizeof(buf), payload_size - n)); 
        if (ngot == -1) {
            rho_warn("xutp_recvn returned -1");
            goto fail;
        }

        n += ngot;
    }

    goto succeed;

fail:
    error = -1;
succeed:
    return (error);
}

int
hajime_bot_close(struct hajime_bot *bot)
{
    int error = 0;

    error  = xutp_close(bot->xfd);
    if (error != 0) 
        rho_warn("xutp_close returned %d", error);

    return (error);
}
