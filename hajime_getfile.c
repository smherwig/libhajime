#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <rho/rho.h>

#include "hajime.h"

#define HAJIME_GETFILE_USAGE \
    "hajime_getfile [options] IP PORT REMOTE_FILE LOCAL_FILE\n" \
    "-h, --help\n" \
    "   display this help message and exit\n\n" \
    "-t, --timeout SECS\n" \
    "   timeout after SECS of inactivity\n\n" \
    "-v, --verbose\n" \
    "   verbose otuput"


struct download_ctx {
    FILE *fh;
};

#if 0
bool g_verbose = false;;
#endif

static void
usage(int exit_code)
{
    fprintf(stderr, "%s\n", HAJIME_GETFILE_USAGE);
    exit(exit_code);
}

#if 0
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
#endif

static int
download_callback(void *userdata, uint8_t *buf, size_t size)
{
    int error = 1;
    FILE *fh = userdata;
    size_t nput = 0;

    nput = fwrite(buf, 1, size, fh);
    if (nput != size) {
        rho_warn("fwrite did not put entire buffer");
        error = 0;
    }

    fflush(fh);
    return (error);
}

/*  XXX: Currently, does not unpack payload after download completes 
 *       (use the hajime_parse_payload tool).
 */
int
main(int argc, char *argv[])
{
    int error = 0;
    struct hajime_bot *bot = NULL;
    FILE *fh = NULL;
    struct timespec timeout = { 0 };

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
#if 0
            g_verbose = true;
#endif
            break;
        default:
            usage(EXIT_FAILURE);
        }
    }
    argc -= optind;
    argv += optind;

    if (argc != 4)
        usage(EXIT_FAILURE);

    fh = fopen(argv[3], "wb");
    if (fh == NULL) {
        rho_errno_warn(errno, "fopen(\"%s\", \"wb\") failed", argv[3]);
        goto fail; 
    }

    hajime_bot_libinit();

    if (timeout.tv_sec == 0)
        bot = hajime_bot_create_client(NULL);
    else
        bot = hajime_bot_create_client(&timeout);

    /* TODO: error checking */
    hajime_bot_connect(bot, argv[0], atoi(argv[1]));
    hajime_bot_file_request(bot, argv[2], download_callback, fh);
    hajime_bot_close(bot);
    hajime_bot_destroy(bot);

    fclose(fh);

    hajime_bot_libfini();

    goto succeed;

fail:
    error = 1;
succeed:
    return (error);
}
