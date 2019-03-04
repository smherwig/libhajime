#include <getopt.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <rho/rho.h>

#include "hajime.h"


#define HAJIME_GETPEERPUBKEY_USAGE \
    "hajime_getpeerpubkey [options] IP PORT\n" \
    "-h, --help\n" \
    "   display this help message and exit\n\n" \
    "-t, --timeout SECS\n" \
    "   timeout after SECS of inactivity\n\n" \
    "-v, --verbose\n" \
    "   verbose output"

bool g_verbose = false;;

static void
usage(int exit_code)
{
    fprintf(stderr, "%s\n", HAJIME_GETPEERPUBKEY_USAGE);
    exit(exit_code);
}

static void
verbose_printf(const char *fmt, ...)
{
    va_list ap;

    if (!g_verbose)
        return;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

int
main(int argc, char *argv[])
{
    int error = 0;
    struct hajime_bot *bot = NULL;
    uint8_t peerpubkey[HAJIME_KEY_LENGTH] = { 0 };
    char keyhex[HAJIME_KEY_LENGTH * 2 + 1] = { 0 };
    struct timespec timeout = { 0 };
    const char *ipstr = NULL;
    uint16_t port = 0;

    int ch = 0;
    const char *short_options = "ht:v";
    static struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"timeout", required_argument, NULL, 't'},
        {"verbose", no_argument, NULL, 'v'},
        {NULL, 0, NULL, 0}
    };

    while ((ch = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (ch) {
        case 'h':
            usage(EXIT_SUCCESS);
            break;
        case 't':
            timeout.tv_sec = rho_str_touint(optarg, 10);
            break;
        case 'v':
            g_verbose = true;
            break;
        default:
            usage(EXIT_FAILURE);
        }
    }
    argc -= optind;
    argv += optind;

    if (argc != 2)
        usage(EXIT_FAILURE);

    ipstr = argv[0];
    port = rho_str_touint16(argv[1], 10);

    hajime_bot_libinit();

    if (timeout.tv_sec == 0)
        bot = hajime_bot_create_client(NULL);
    else
        bot = hajime_bot_create_client(&timeout);

    verbose_printf("connecting...\n");
    error = hajime_bot_connect(bot, ipstr, port);
    if (error == -1) {
        printf("%s:%"PRIu16 " error: failed to connect\n", ipstr, port);
        goto fail;
    }
    verbose_printf("connected\n");

    verbose_printf("exchanging keys...\n");
    error = hajime_bot_key_exchange(bot, peerpubkey);
    if (error == -1) {
        printf("%s:%"PRIu16 " error: failed to exchange keys\n", ipstr, port);
        goto fail;
    }
    verbose_printf("keys exchanged\n");

    rho_binascii_hexlify(peerpubkey, HAJIME_KEY_LENGTH, keyhex);
    printf("%s:%"PRIu16 " pubkey=%s\n", ipstr, port, keyhex);

    if (error == 0)
        hajime_bot_close(bot);

    goto succeed;

fail:
    error = 1;

succeed:
    hajime_bot_destroy(bot);
    hajime_bot_libfini();
    return (error);
}
