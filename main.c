#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
#include <arpa/inet.h>

//
#include <parsebgp.h>
#include <streamlike/file.h>
#include <streamlike/http.h>
#include <zidx.h>

//
#include "find_prefix.h"

char startswith(const char *string, const char *prefix) {
    while (*prefix)
        if (*prefix++ != *string++) return 0;
    return 1;
}

#define errfail(...)                  \
    do {                              \
        fprintf(stderr, __VA_ARGS__); \
        goto fail;                    \
    } while (0);

static void errexit(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    exit(1);
}

void usageexit(const char *program) {
    errexit(
        "usage: %s <gzipped-mrt-file-or-url> <zidx-file> "
        "<ip-address>/<prefix-length> [-i] [-d]\n"
        "\t-i: ignore zidx file provided (optional)\n"
        "\t-d: debug print (optional)\n",
        program);
}

int main(int argc, char **argv) {
    const char *program = argv[0];
    if (argc < 4) usageexit(program);

    const char *gzipped_mrt_path = argv[1];
    const char *zidx_path = argv[2];
    char *addr_str = argv[3];
    char *prefix_len_str = strchr(addr_str, '/');

    _Bool debug = 0;
    _Bool ignore_zidx = 0;

    for (int i = 4; i < argc; i++) {
        if (!strcmp(argv[i], "-d"))
            debug = 1;
        else if (!strcmp(argv[i], "-i"))
            ignore_zidx = 1;
        else
            usageexit(program);
    }

    if (prefix_len_str == NULL)
        errexit("error: couldn't find '/' denoting prefix length\n");
    *prefix_len_str++ = '\0';
    for (char *d = prefix_len_str; *d; d++)
        if (*d < '0' || *d > '9')
            errexit(
                "error: prefix length should consist of digits only: '%c'\n",
                *d);

    long prefix_len_long = strtol(prefix_len_str, NULL, 10);
    if (prefix_len_long < 0 || prefix_len_long > 128)
        errexit("error: prefix length should be in the range of [0, 128]\n");

    struct afi_prefix_t pfx;
    pfx.prefix.len = prefix_len_long;
    if (inet_pton(AF_INET, addr_str, pfx.prefix.addr)) {
        if (pfx.prefix.len > 24)
            errexit(
                "error: prefix length shouldn't be more than 24 for IPv4\n");
        pfx.type = AFI_TYPE_IPV4;
    } else if (inet_pton(AF_INET6, addr_str, pfx.prefix.addr)) {
        pfx.type = AFI_TYPE_IPV6;
    } else {
        errexit("error: couldn't parse ip address\n");
    }

    streamlike_t *gzip_stream;
    int is_url = startswith(gzipped_mrt_path, "http") &&
                 (startswith(gzipped_mrt_path + 4, "://") ||
                  startswith(gzipped_mrt_path + 4, "s://"));
    gzip_stream = is_url ? sl_http_create(gzipped_mrt_path)
                         : sl_fopen(gzipped_mrt_path, "rb");

    if (gzip_stream == NULL)
        errexit("error: couldn't open gzip stream '%s'\n", gzipped_mrt_path);

    // errfail on error after this point

    zidx_index *index = zidx_index_create();
    streamlike_t *index_stream = NULL;
    struct prefix_checkpoint_t pfx_chkp = {-1, 0};

    if (index == NULL) errfail("error: couldn't create zidx index\n");

    if (zidx_index_init(index, gzip_stream) != ZX_RET_OK)
        errfail("error: couldn't initialize zidx index\n");

    if (!ignore_zidx) {
        index_stream = sl_fopen(zidx_path, "rb");
        if (index_stream == NULL)
            errfail("error: couldn't open index stream '%s'\n", zidx_path);
        if (zidx_import(index, index_stream) != ZX_RET_OK)
            errfail("error: couldn't import zidx index\n");
        sl_fclose(index_stream);
        index_stream = NULL;

        pfx_chkp = find_prefix_checkpoint(&pfx, index);
        if (pfx_chkp.index < -1) errfail("error: couldn't find checkpoint");
    }



    zidx_checkpoint *chkp = NULL;
    if (pfx_chkp.index >= 0) chkp = zidx_get_checkpoint(index, pfx_chkp.index);

    uint8_t buffer[32768];
    const void *bufferp;
    _Bool seek_needed = pfx_chkp.index >= 0;
    enum { SEARCHING, EXACT_MATCH_FOUND, NOT_FOUND } status = SEARCHING;

    size_t off;
    size_t len;
    if (pfx_chkp.index >= 0) {
        // seek into window
        off = pfx_chkp.first_mrt_offset;
        len = zidx_get_checkpoint_window(chkp, &bufferp);
        memcpy(buffer, bufferp + off, len - off);
        len -= off;
        off = 0;
    } else {
        int ret = zidx_read(index, buffer, sizeof(buffer));
        if (ret < 0) errfail("error: couldn't make initial read");
        len = ret;
        off = 0;
    }

    while (len > 0 && status == SEARCHING) {
        assert(off + sizeof(struct mrt_header_t) <= len);
        while (status == SEARCHING) {
            if (sizeof(struct mrt_header_t) > len) {
                memmove(buffer, buffer + off, len);
                off = len;
                break;
            }

            struct mrt_header_t header = get_header(buffer + off);

            if (sizeof(struct mrt_header_t) + header.length > len) {
                memmove(buffer, buffer + off, len);
                off = len;
                break;
            }

            if (header.subtype == 1) {  // skip peer_index_table
                off += sizeof(struct mrt_header_t) + header.length;
                len -= sizeof(struct mrt_header_t) + header.length;
                continue;
            }

            struct afi_prefix_t mrt_prefix = get_prefix(buffer + off);
            if (debug) {
                printf("debug: ");
                prefix_printf(mrt_prefix);
                printf("\n");
            }
            int cmp = afi_prefix_cmp(&mrt_prefix, &pfx);
            if (cmp == 0) {
                status = EXACT_MATCH_FOUND;

                parsebgp_opts_t opts;
                parsebgp_opts_init(&opts);
                opts.ignore_not_implemented = 1;
                parsebgp_msg_t *msg = parsebgp_create_msg();
                if (parsebgp_decode(opts, PARSEBGP_MSG_TYPE_MRT, msg,
                                    buffer + off, &len) != PARSEBGP_OK) {
                    parsebgp_destroy_msg(msg);
                    errfail("error: prefix found, but failed to decode");
                }
                parsebgp_dump_msg(msg);
                parsebgp_destroy_msg(msg);
            } else if (cmp > 0) {
                status = NOT_FOUND;
            } else {
                off += sizeof(struct mrt_header_t) + header.length;
                len -= sizeof(struct mrt_header_t) + header.length;
            }
        }
        if (status == SEARCHING) {
            if (seek_needed) {
                if (zidx_seek(index, zidx_get_checkpoint_offset(chkp)) !=
                    ZX_RET_OK)
                    errfail("error: couldn't seek to mrt record");
                seek_needed = 0;
            }
            int ret = zidx_read(index, buffer + off, sizeof(buffer) - off);
            if (ret < 0) errfail("error: while reading zidx stream");
            len = off + ret;
            off = 0;
        }
    }

    if (status != EXACT_MATCH_FOUND) errfail("Prefix not found");

    if (is_url)
        sl_http_destroy(gzip_stream);
    else
        sl_fclose(gzip_stream);
    zidx_index_destroy(index);
    free(index);

    return 0;

fail:
    if (is_url)
        sl_http_destroy(gzip_stream);
    else
        sl_fclose(gzip_stream);

    if (index_stream) sl_fclose(index_stream);
    if (index) zidx_index_destroy(index);
    free(index);
    return 1;
}
