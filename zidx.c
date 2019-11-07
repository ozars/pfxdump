#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <streamlike.h>
#include <streamlike/file.h>
#include <zidx.h>
#include <zlib.h>

void create_index(const char *gzfile, const char *indexfile, long int span, int is_uncompressed)
{
    streamlike_t *gzf    = NULL;
    streamlike_t *indexf = NULL;
    zidx_index *zidx     = NULL;
    const size_t len = 128*1024;
    int ret;

    gzf = sl_fopen(gzfile, "rb");
    assert(gzf);

    indexf = sl_fopen(indexfile, "wb");
    assert(gzf);

    zidx = zidx_index_create();
    assert(zidx);

    ret = zidx_index_init_ex(zidx, gzf, ZX_STREAM_GZIP_OR_ZLIB,
                             ZX_CHECKSUM_DEFAULT, NULL,
                             ZX_DEFAULT_INITIAL_LIST_CAPACITY,
                             ZX_DEFAULT_WINDOW_SIZE,
                             len, len);
    /* ret = zidx_index_init(zidx, gzf); */
    assert(ret == ZX_RET_OK);

    ret = zidx_build_index(zidx, span, is_uncompressed ? 1 : 0);
    assert(ret == ZX_RET_OK);

    ret = zidx_export(zidx, indexf);
    assert(ret == ZX_RET_OK);

    ret = sl_fclose(gzf);
    assert(ret == ZX_RET_OK);

    ret = sl_fclose(indexf);
    assert(ret == ZX_RET_OK);
}

#ifndef NDEBUG
void verify_index(const char *gzfile, const char *indexfile)
{

    streamlike_t *gzf    = NULL;
    streamlike_t *indexf = NULL;
    zidx_index *zidx     = NULL;
    gzFile gz = NULL;
    const size_t len = 128*1024;
    char buf1[len];
    char buf2[len];
    int ret, read1, read2;

    gzf = sl_fopen(gzfile, "rb");
    assert(gzf);

    indexf = sl_fopen(indexfile, "rb");
    assert(gzf);

    zidx = zidx_index_create();
    assert(zidx);

    ret = zidx_index_init_ex(zidx, gzf, ZX_STREAM_GZIP_OR_ZLIB,
                             ZX_CHECKSUM_DEFAULT, NULL,
                             ZX_DEFAULT_INITIAL_LIST_CAPACITY,
                             ZX_DEFAULT_WINDOW_SIZE,
                             len, len);
    /* ret = zidx_index_init(zidx, gzf); */
    assert(ret == ZX_RET_OK);

    ret = zidx_import(zidx, indexf);
    assert(ret == ZX_RET_OK);

    gz = gzopen(gzfile, "rb");
    assert(gz);

    assert(gzbuffer(gz, len) == Z_OK);

    do {
        read1 = gzread(gz, buf1, len);
        read2 = zidx_read(zidx, (uint8_t*)buf2, len);
        /* printf("%d %d\n", read1, read2); */
        assert(read1 == read2);
        assert(!memcmp(buf1, buf2, read1));
    } while (read1 == len);

    ret = sl_fclose(gzf);
    assert(ret == ZX_RET_OK);

    ret = sl_fclose(indexf);
    assert(ret == ZX_RET_OK);
}
#endif

int main(int argc, char *argv[])
{
    if (argc != 5) {
        printf("Usage: %s <gzip-file> <index-file> <checkpoint-span> <is-spans-based-on-uncompressed-size>\n", argv[0]);
        return 1;
    }
    long int span = atol(argv[3]);
    int is_uncompressed = atoi(argv[4]);
    create_index(argv[1], argv[2], span, is_uncompressed);
#ifndef NDEBUG
    verify_index(argv[1], argv[2]);
#endif
    return 0;

}