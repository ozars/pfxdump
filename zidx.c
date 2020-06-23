#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <streamlike.h>
#include <streamlike/file.h>
#include <zidx.h>
#include <zlib.h>


uint32_t get_gzip_checksum(const char *filename)
{

	FILE *comp;

	comp=fopen(filename,"rb");
	if(comp==NULL)
	{
		printf("Error opening file (%s)\n",filename);
		return -1;
	}
	fseek(comp,0,SEEK_END);
	long comp_size=ftell(comp);
	fseek(comp,0,SEEK_SET);

	unsigned char *contents=malloc(comp_size+1);
	fread(contents,1,comp_size,comp);
	fclose(comp);

	long start_checksum=comp_size-8;
	long end_checksum=comp_size-4;
	uint32_t file_checksum=0;

	//little-endian conversion
	int x;
	for(x=end_checksum-1;x>=start_checksum;x--)
	{
		file_checksum=(file_checksum<<8)+contents[x];
	}
	return file_checksum;
}

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
    assert(indexf);

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

    ret = zidx_index_destroy(zidx);
    assert(ret == ZX_RET_OK);

    free(zidx);
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
    assert(indexf);

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

    int list_len=zidx_get_checkpoint_list_len(zidx);
    int x;
    for(x=0;x<list_len;x++)
    {
	    printf("Checksum of checkpoint %d = %lu\n",x,(unsigned long) zidx_get_checkpoint_checksum(zidx,x));
    }

    uint32_t zidx_checksum = zidx_get_index_running_checksum(zidx);
    uint32_t gzip_checksum = get_gzip_checksum(gzfile);
    assert(gzip_checksum==zidx_checksum);


    ret = sl_fclose(gzf);
    assert(ret == ZX_RET_OK);

    ret = sl_fclose(indexf);
    assert(ret == ZX_RET_OK);

    ret = zidx_index_destroy(zidx);
    assert(ret == ZX_RET_OK);

    free(zidx);
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
#ifndef NDEBUG
    create_index(argv[1], argv[2], span, is_uncompressed);
    verify_index(argv[1], argv[2]);
#endif
    return 0;

}
