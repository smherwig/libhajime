#include <stdio.h>

#include <rho/rho.h>

#include "hajime.h"

int
main(int argc, char *argv[])
{
    int error = 0;
    uint8_t *payload = NULL;
    size_t payload_len = 0;
    struct hajime_payload_header header;
    uint8_t *body = NULL;
    size_t body_len = 0;
    
    error = rho_fileutil_readall(argv[1], &payload, &payload_len);
    if (error != 0)
        goto fail;

    fprintf(stderr, "file_len: %lu\n", (unsigned long)payload_len);

    fprintf(stderr, "\n");

    error = hajime_decode_payload(payload, payload_len,
            &header, &body, &body_len);
    

    fprintf(stderr, "filename: %s\n", header.filename);
    fprintf(stderr, "lz4_compressed: %s\n", header.lz4_compressed ? "yes" : "no");
    fprintf(stderr, "type: %s\n", hajime_payload_type_to_string(header.type));
    fprintf(stderr, "creation_time: %lu\n", (unsigned long)header.creation_time);
    fprintf(stderr, "body_size: %lu\n", (unsigned long)header.body_size);
    fprintf(stderr, "body_size_compressed: %lu\n",
            (unsigned long)header.body_size_compressed);
    fprintf(stderr, "body_size_decompressed: %lu\n",
            (unsigned long)header.body_size_decompressed);

    fprintf(stderr, "\n");

    fprintf(stderr, "body_len: %zu\n", body_len);

    /* TODO: error-checking */
    error = rho_fileutil_writeall(argv[2], body, body_len);

fail:
    if (payload != NULL)
        rhoL_free(payload);

    if (body != NULL)
        rhoL_free(body);

    return (error);
}
