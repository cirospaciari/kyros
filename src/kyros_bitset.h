#ifndef KYROS_BITSET_H
#define KYROS_BITSET_H
#include <assert.h>
#include <stdint.h>

#define __kyros_bitset_n(N) kyros_bitset_##N
#define kyros_bitset_n(N) __kyros_bitset_n(N)

#define declare_kyros_bitset_n(N) \
    typedef struct {              \
        unsigned _BitInt(N) mask; \
    } __kyros_bitset_n(N)

#define kyros_bitset_empty_n(N) (__kyros_bitset_n(N)) { .mask = 0 }
#define kyros_bitset_full_n(N) (__kyros_bitset_n(N)) { .mask = ~(unsigned _BitInt(N))0 }

#define kyros_bitset_count_n(N, self)      \
    ({                                     \
        __builtin_popcountg((self)->mask); \
    })

#define kyros_bitset_capacity_n(N)       \
    ({                                   \
        sizeof(unsigned _BitInt(N)) * 8; \
    })

#define kyros_bitset_assert_index(N, index)              \
    { { assert(index < sizeof(unsigned _BitInt(N)) * 8); \
    }                                                    \
    }

#define kyros_bitset_set_n(N, self, index)                \
    ({                                                    \
        kyros_bitset_assert_index(N, index);              \
        auto* tmp = self;                                 \
        tmp->mask |= (((unsigned _BitInt(N))1) << index); \
    })

#define kyros_bitset_unset_n(N, self, index)               \
    ({                                                     \
        kyros_bitset_assert_index(N, index);               \
        auto* tmp = self;                                  \
        tmp->mask &= ~(((unsigned _BitInt(N))1) << index); \
    })

#define kyros_bitset_first_set_n(N, self)        \
    ({                                           \
        auto* tmp = self;                        \
        auto mask = tmp->mask;                   \
        (mask == 0 ? -1 : __builtin_ctzg(mask)); \
    })

typedef struct kyros_bitset {
    uint64_t mask;
} kyros_bitset;

#define kyros_bitset_empty (kyros_bitset) { .mask = 0 }
#define kyros_bitset_full (kyros_bitset) { .mask = ~(uint64_t)0 }

static inline uint32_t kyros_bitset_count(kyros_bitset self)
{
    return __builtin_popcountg(self.mask);
}

static inline void kyros_bitset_set(kyros_bitset* self, uint32_t index)
{
    assert(index < 64);
    self->mask |= (1ULL << index);
}

static inline void kyros_bitset_unset(kyros_bitset* self, uint32_t index)
{
    assert(index < 64);
    self->mask &= (1ULL << index);
}

static inline int32_t kyros_bitset_first_set(kyros_bitset* self)
{
    auto mask = self->mask;
    if (mask == 0)
        return -1;
    return __builtin_ctzg(mask);
}
#endif