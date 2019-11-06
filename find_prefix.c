#include "find_prefix.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

//
#include <arpa/inet.h>

//
#include <zidx.h>

enum {
    TABLE_DUMP_V2 = 13,
    TABLE_DUMP_V2_RIB_IPV4_UNICAST = 2,
    TABLE_DUMP_V2_RIB_IPV4_MULTICAST = 3,
    TABLE_DUMP_V2_RIB_IPV6_UNICAST = 4,
    TABLE_DUMP_V2_RIB_IPV6_MULTICAST = 5,
    TABLE_DUMP_V2_RIB_GENERIC = 6,

    TABLE_DUMP_V2_SUBTYPE_BEGIN = TABLE_DUMP_V2_RIB_IPV4_UNICAST,
    TABLE_DUMP_V2_SUBTYPE_END = TABLE_DUMP_V2_RIB_GENERIC
};

typedef struct prefix_checkpoint_t prefix_checkpoint_t;
typedef struct mrt_header_t mrt_header_t;

typedef struct __attribute__((packed)) tdv2_minimal_t {
    mrt_header_t header;
    uint32_t sequence_number;
    uint8_t prefix_length;
    char addr[0];
} tdv2_minimal_t;

static const struct prefix_t* get_pfx_from_tdv2(const char* window) {
    assert(window != NULL);
    const tdv2_minimal_t* msg = (const void*)window;
    assert(ntohs(msg->header.type) == TABLE_DUMP_V2);
    assert(ntohs(msg->header.subtype) >= TABLE_DUMP_V2_SUBTYPE_BEGIN &&
           ntohs(msg->header.subtype) < TABLE_DUMP_V2_SUBTYPE_END);
    return (const void*)(window + offsetof(tdv2_minimal_t, prefix_length));
}

static off_t align_to_first_header(const char* window, size_t len) {
    assert(window);
    const int threshold = 5;
    for (size_t off = 0; off + sizeof(mrt_header_t) < len; off++) {
        uint32_t timestamp = 0;
        uint8_t found = 0;
        off_t off_temp = off;
        do {
            mrt_header_t header = get_header(window + off_temp);
            if (header.type != TABLE_DUMP_V2 ||
                header.subtype < TABLE_DUMP_V2_SUBTYPE_BEGIN ||
                header.subtype >= TABLE_DUMP_V2_SUBTYPE_END ||
                header.timestamp < timestamp ||
                header.length < sizeof(tdv2_minimal_t) - sizeof(mrt_header_t))
                break;
            if (++found == threshold) return off;
            timestamp = header.timestamp;
            off_temp += sizeof(mrt_header_t) + header.length;
        } while (off_temp + sizeof(mrt_header_t) < len);
        /* mrt_header_t header = get_header(headerp);
        while (header.type == TABLE_DUMP_V2 &&
               header.subtype >= TABLE_DUMP_V2_SUBTYPE_BEGIN &&
               header.subtype < TABLE_DUMP_V2_SUBTYPE_END &&
               header.timestamp >= timestamp &&
               header.length >= sizeof(tdv2_minimal_t) - sizeof(mrt_header_t)) {
            if (++found == threshold) return off;
            if (off + header.length + sizeof(mrt_header_t) > len) break;
            timestamp = header.timestamp;
            headerp = (void*)(((char*)headerp) + sizeof(mrt_header_t) +
                              header.length);
            header = get_header(headerp);
        } */
    }
    return -1;
}

static int prefix_cmp(const struct prefix_t* lhs, const struct prefix_t* rhs) {
    uint8_t cmp_len = lhs->len < rhs->len ? lhs->len : rhs->len;
    uint8_t bytes = cmp_len / 8;
    uint8_t bits = cmp_len % 8;

    int cmp = memcmp(lhs->addr, rhs->addr, bytes);
    if (cmp != 0) return cmp;

    if (bits) {
        uint8_t msb = 0xFFU << bits;
        int cmp = (int)(lhs->addr[bytes] & msb) - (int)(rhs->addr[bytes] & msb);
        if (cmp != 0) return cmp;
    }
    return (int)lhs->len - (int)rhs->len;
}

int afi_prefix_cmp(const struct afi_prefix_t* lhs,
                   const struct afi_prefix_t* rhs) {
    int cmp = prefix_cmp(&lhs->prefix, &rhs->prefix);
    if (cmp != 0) return cmp;
    return (int)lhs->type - (int)rhs->type;
}

static enum afi_type_t get_tdv2_afi_type(const char* window) {
    int subtype = ntohs(((const mrt_header_t*)(window))->subtype);
    switch (subtype) {
        case TABLE_DUMP_V2_RIB_IPV4_UNICAST:
        case TABLE_DUMP_V2_RIB_IPV4_MULTICAST:
            return AFI_TYPE_IPV4;
        case TABLE_DUMP_V2_RIB_IPV6_UNICAST:
        case TABLE_DUMP_V2_RIB_IPV6_MULTICAST:
            return AFI_TYPE_IPV6;
        default:
            assert(0 && "Unknown AFI type");
    }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

void prefix_printf(struct afi_prefix_t afi_prefix) {
    char dst[255];
    uint8_t bytes = afi_prefix.prefix.len / 8;
    uint8_t bits = afi_prefix.prefix.len % 8;
    memset(afi_prefix.prefix.addr + bytes + (bits > 0), 0,
           16 - bytes - (bits > 0));
    if (bits) {
        uint8_t msb = 0xFFU << bits;
        afi_prefix.prefix.addr[bytes] &= msb;
    }
    switch (afi_prefix.type) {
        case AFI_TYPE_IPV4:
            /* *(uint32_t*)pfx.addr = htonl(*(uint32_t*)pfx.addr); */
            printf("%s/%u",
                   inet_ntop(AF_INET, afi_prefix.prefix.addr, dst,
                             INET_ADDRSTRLEN),
                   afi_prefix.prefix.len);
            break;
        case AFI_TYPE_IPV6:
            printf("%s/%u",
                   inet_ntop(AF_INET6, afi_prefix.prefix.addr, dst,
                             INET6_ADDRSTRLEN),
                   afi_prefix.prefix.len);
            break;
        default:
            assert(0 && "Unknown AFI type");
    }
}

struct prefix_checkpoint_t find_prefix_checkpoint(
    const struct afi_prefix_t* pfx, zidx_index* index) {
    int chkp_cnt = zidx_checkpoint_count(index);
    if (chkp_cnt < 0) return (prefix_checkpoint_t){-2};

    /* invariant: i <= k < j, k is inclusive upperbound. */
    int i = 0;
    int j = chkp_cnt;
    int shift = 0;
    prefix_checkpoint_t ret = {-1, 0};

    while (j - i > shift * 2) {
      int k = i + (j - i) / 2 - shift;
      const char* window;
      zidx_checkpoint* chkp = zidx_get_checkpoint(index, k);
      if (chkp == NULL) return (prefix_checkpoint_t){-3};
      size_t len = zidx_get_checkpoint_window(chkp, (const void**)&window);
      assert(window);
      off_t off = align_to_first_header(window, len);
      if (off >= 0) {
          const struct prefix_t* off_pfx = get_pfx_from_tdv2(window + off);
          int cmp = prefix_cmp(off_pfx, &pfx->prefix);

          if (cmp == 0) {
              enum afi_type_t mrt_afi_type = get_tdv2_afi_type(window + off);
              cmp = mrt_afi_type - pfx->type;
          }
          if (cmp < 0) {
              ret = (prefix_checkpoint_t){k, off};
              i = k + 1;
          } else if (cmp > 0) {
              j = k - 1;
          } else /*if (cmp == 0)*/ {
              return (prefix_checkpoint_t){k, off};
          }
          shift = 0;
      } else {
          shift++;
      }
    }
    return ret;  // return last candidate
}

struct afi_prefix_t get_prefix(const void* mrt_data) {
    assert(mrt_data);
    return (struct afi_prefix_t){get_tdv2_afi_type(mrt_data),
                                 *get_pfx_from_tdv2(mrt_data)};
}

struct mrt_header_t get_header(const void* mrt_data) {
    const mrt_header_t* headerp = mrt_data;
    return (mrt_header_t){ntohl(headerp->timestamp), ntohs(headerp->type),
                          ntohs(headerp->subtype), ntohl(headerp->length)};
}

#pragma GCC diagnostic pop
