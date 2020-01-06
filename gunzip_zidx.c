#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <streamlike.h>
#include <streamlike/file.h>
#include <string.h>
#include <zidx.h>
#include <zlib.h>

#define DEBUG_PRINT(...)

typedef struct chunk_args_s {
    zidx_index *opt_index;
    const char *gzip_file_name;
    const char *zidx_file_name;
    const char *output_file_name;
    off_t cur;
    off_t end;
} chunk_args_t;

static int *new_int(int code) {
    DEBUG_PRINT("Returning %d\n", code);
    int *r = malloc(sizeof(int));
    if (!r) return NULL;
    *r = code;
    return r;
}

#define END_WITH_CODE(code) \
    do {                    \
        ret = code;         \
        goto fail;          \
    } while (0)

#define END_IF_NOT_OK(expr)              \
    do {                                 \
        ret = (expr);                    \
        if (ret != ZX_RET_OK) goto fail; \
    } while (0)

void *decompress_procedure(void *vargs) {
    const chunk_args_t *args = vargs;
    int ret;

    char *buffer = NULL;
    off_t bytes = args->cur == -1 ? 50 * 1024 * 1024 : args->end - args->cur;
    if (bytes < 0) return new_int(-1024);

    zidx_index *index = NULL;
    streamlike_t *gzip_stream = NULL;
    streamlike_t *zx_stream = NULL;
    FILE *outf = NULL;
    int read;

    if (args->opt_index) {
        index = args->opt_index;
    } else {
        index = zidx_index_create();
        if (!index) return new_int(-1025);

        gzip_stream = sl_fopen(args->gzip_file_name, "rb");
        if (!gzip_stream) {
            free(index);
            return new_int(-1027);
        }

        END_IF_NOT_OK(zidx_index_init(index, gzip_stream));

        if (args->cur != -1) {
            zx_stream = sl_fopen(args->zidx_file_name, "rb");
            if (!zx_stream) END_WITH_CODE(-1027);
            END_IF_NOT_OK(zidx_import(index, zx_stream));
        }

    }
    buffer = malloc(bytes);
    DEBUG_PRINT("BYTES: %ld\n", bytes);
    if (!buffer) END_WITH_CODE(-1026);

    outf = fopen(args->output_file_name, "r+b");
    if (!outf) END_WITH_CODE(-1028);

    if (args->cur == -1) {
        while ((read = zidx_read(index, buffer, bytes)) > 0) {
            if (fwrite(buffer, read, 1, outf) != 1)
                return new_int(-1030);
        }
        if (read < 0) END_WITH_CODE(zidx_error(index));
    } else {
        END_IF_NOT_OK(zidx_seek(index, args->cur));
        if (zidx_read(index, buffer, bytes) < bytes)
            END_WITH_CODE(zidx_error(index));
        if (fseek(outf, args->cur, SEEK_SET) != 0) END_WITH_CODE(-1029);
        if (fwrite(buffer, bytes, 1, outf) != 1)
            return new_int(-1030);
    }

    ret = 0;

fail:
    // leaks... leaks everywhere

    /* free(buffer); */
    /* zidx_index_destroy(index); */
    /* free(index); */
    /* if (gzip_stream) sl_fclose(gzip_stream); */
    /* if (zx_stream) sl_fclose(zx_stream); */
    /* if (outf) fclose(outf); */
    return ret ? new_int(ret) : NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr,
                "Usage: %s <thread-count> <gzip-file> <zidx-file> <output-file>\n",
                argv[0]);
        return 1;
    }
    int *ret = NULL;
    int thread_count = atoi(argv[1]);
    chunk_args_t args = {NULL, argv[2], argv[3], argv[4], -1, -1};

    FILE *fp = fopen(argv[4], "wb");
    if (!fp) return 8;
    fclose(fp);

    if (thread_count == 0) {
        ret = decompress_procedure(&args);
        if (ret) DEBUG_PRINT("Program returned error %d.\n", *ret);
        else DEBUG_PRINT("Program completed successfully.\n");
        free(ret);
    } else {
        zidx_index *index = NULL;
        streamlike_t *gzip_stream = NULL;
        streamlike_t *zx_stream = NULL;

        index = zidx_index_create();
        if (!index) return 2;

        gzip_stream = sl_fopen(args.gzip_file_name, "rb");
        if (!gzip_stream) return 3;

        if (zidx_index_init(index, gzip_stream) != ZX_RET_OK) return 4;

        zx_stream = sl_fopen(argv[3], "rb");
        if (!zx_stream) return 10;
        if(zidx_import(index, zx_stream) != ZX_RET_OK) return 11;

        off_t sz = zidx_uncomp_size(index);
        if (sz < 0) return 5;

        DEBUG_PRINT("SIZE: %ld\n", sz);

        pthread_t threads[thread_count];
        chunk_args_t thread_args[thread_count];
        off_t prev = zidx_get_checkpoint_offset(zidx_get_checkpoint(index, zidx_checkpoint_count(index) / thread_count));

        thread_args[0] = (chunk_args_t){index, NULL, NULL, argv[4], 0, prev};
        DEBUG_PRINT("Thread 0: [0, %ld)\n", prev);
        if (pthread_create(&threads[0], NULL, decompress_procedure, &thread_args[0]) != 0) return 6;
        for(int i = 1; i < thread_count; i++) {
            off_t next = i == thread_count - 1 ? sz : zidx_get_checkpoint_offset(zidx_get_checkpoint(index, (i + 1) * zidx_checkpoint_count(index) / thread_count));
            thread_args[i] = (chunk_args_t){NULL, argv[2], argv[3], argv[4], prev, next};
            DEBUG_PRINT("Thread %i: [%ld, %ld)\n", i, prev, next);
            prev = next;
            if (pthread_create(&threads[i], NULL, decompress_procedure, &thread_args[i]) != 0) return 6;
        }

        for(int i = 0; i < thread_count; i++) {
            if (pthread_join(threads[i], (void**)&ret) != 0) return 7;
            if (ret) DEBUG_PRINT("Thread %d returned error %d.\n", i, *ret);
            else DEBUG_PRINT("Thread %d completed successfully.\n", i);
            /* free(ret); */
        }
    }

    return 0;
}
