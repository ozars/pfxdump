#ifndef FIND_PREFIX_H
#define FIND_PREFIX_H

#include <streamlike.h>
#include <zidx.h>

struct prefix_t {
    uint8_t len;
    uint8_t addr[16];
};

enum afi_type_t { AFI_TYPE_IPV4 = 0, AFI_TYPE_IPV6 = 1 };

struct afi_prefix_t {
    enum afi_type_t type;
    struct prefix_t prefix;
};

struct prefix_checkpoint_t {
    int index;
    size_t first_mrt_offset;
};

struct mrt_header_t {
    uint32_t timestamp;
    uint16_t type;
    uint16_t subtype;
    uint32_t length;
};

void prefix_printf(struct afi_prefix_t afi_prefix);
struct prefix_checkpoint_t find_prefix_checkpoint(
    const struct afi_prefix_t *pfx, zidx_index *index);
struct afi_prefix_t get_prefix(const void *mrt_data);
struct mrt_header_t get_header(const void *mrt_data);
int afi_prefix_cmp(const struct afi_prefix_t *lhs,
                   const struct afi_prefix_t *rhs);

#endif
