/*
 * hdr_packed_histogram.c -- Phase-2 sparse/packed HdrHistogram variant.
 * Released to the public domain, as explained at
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * See hdr_packed_histogram.h for scope and rationale.
 *
 * Backing store: a sparse vector of populated buckets kept sorted by virtual
 * index:
 *     idx[i] : int32 virtual bucket index  (ascending, unique)
 *     cnt    : byte blob, size = cap * width, holding one count per slot at a
 *              uniform adaptive width (1/2/4/8 B) that widens on overflow.
 *
 * record -> binary search; hit: cnt += delta; miss: insert (memmove), grow x2.
 * Sorted order makes value_at_percentile a plain ascending prefix-sum, exactly
 * mirroring the dense scan (unpopulated indices contribute 0).
 *
 * Geometry oracle: a shared hdr_packed_config wrapping a dense struct
 * hdr_histogram with counts==NULL. The dense value<->index helpers read only
 * geometry fields, never counts[], so we reuse them verbatim.
 */
/* glibc feature macro: exposes struct timespec (used by hdr_time.h) and the
   endian.h helpers that hdr_endian.h wraps on Linux. No-op on other platforms,
   where hdr_endian.h selects the native byte-order API. */
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <zlib.h>

#include "hdr/hdr_histogram.h"
#include "hdr/hdr_time.h"            /* hdr_timespec, needed by the log header */
#include "hdr/hdr_histogram_log.h"  /* canonical HDR_ error codes (single source) */
#include "hdr_endian.h"             /* portable byte-order helpers (all platforms) */
#include "hdr/hdr_packed_histogram.h"

#if defined(_MSC_VER)
/* On MSVC, hdr_endian.h maps the byte-order helpers to winsock htonll/ntohll,
   which live in ws2_32; mirror the dense hdr_histogram_log.c linkage directive. */
#pragma comment(lib, "ws2_32.lib")
#endif

/* Exported by the hdr static lib but not declared in the public header. */
extern int32_t counts_index_for(const struct hdr_histogram* h, int64_t value);
extern int zig_zag_encode_i64(uint8_t* buffer, int64_t signed_value);
extern int zig_zag_decode_i64(const uint8_t* buffer, int64_t* signed_value);

/* Allocation/compress hooks. Default build maps straight to libc/zlib with zero
   overhead; a coverage build (-DPACKED_FAULT_HOOKS) can force any single UUT
   allocation or the compress call to fail, to exercise the defensive branches.
   Same idea as the dense lib's hdr_calloc/hdr_malloc indirection. */
#ifdef PACKED_FAULT_HOOKS
int pk_fail_calloc = 0, pk_fail_realloc = 0, pk_fail_malloc = 0, pk_fail_compress = 0;
static int pk_take(int* f) { if (*f == 1) { *f = 0; return 1; } if (*f > 1) (*f)--; return 0; }
#define PK_CALLOC(n, s)     (pk_take(&pk_fail_calloc)  ? NULL : calloc((n), (s)))
#define PK_REALLOC(p, s)    (pk_take(&pk_fail_realloc) ? NULL : realloc((p), (s)))
#define PK_MALLOC(s)        (pk_take(&pk_fail_malloc)  ? NULL : malloc((s)))
#define PK_COMPRESS(d,dl,s,sl) (pk_take(&pk_fail_compress) ? Z_BUF_ERROR : compress((d),(dl),(s),(sl)))
#else
#define PK_CALLOC   calloc
#define PK_REALLOC  realloc
#define PK_MALLOC   malloc
#define PK_COMPRESS compress
#endif

#define HDR_PACKED_INITIAL_CAP 4

struct hdr_packed_config
{
    struct hdr_histogram geom;   /* geometry only; geom.counts == NULL */
};

struct hdr_packed_histogram
{
    /* ordered largest-alignment-first to pack into 64 B on LP64 (no padding
       holes): 6 x 8-byte, then 2 x 4-byte, then 2 x 1-byte. */
    const struct hdr_packed_config* cfg;
    int32_t* idx;                /* sorted ascending, length = size */
    uint8_t* cnt;                /* cap * width bytes */
    int64_t  total_count;
    int64_t  min_value;          /* raw min recorded value (INT64_MAX if none) */
    int64_t  max_value;          /* raw max recorded value (0 if none) */
    int32_t  size;               /* populated buckets */
    int32_t  cap;                /* allocated slots */
    uint8_t  width;              /* count byte width: 1,2,4,8 */
    bool     owns_cfg;
};

/* ---- count width helpers -------------------------------------------------- */
static int64_t width_max(uint8_t w)
{
    switch (w)
    {
        case 1:  return 0xFFLL;
        case 2:  return 0xFFFFLL;
        case 4:  return 0xFFFFFFFFLL;
        default: return INT64_MAX;
    }
}

static int64_t slot_get(const struct hdr_packed_histogram* h, int32_t i)
{
    const uint8_t* p = h->cnt + (size_t) i * h->width;
    switch (h->width)
    {
        case 1:  return *p;
        case 2:  { uint16_t v; memcpy(&v, p, 2); return v; }
        case 4:  { uint32_t v; memcpy(&v, p, 4); return v; }
        default: { uint64_t v; memcpy(&v, p, 8); return (int64_t) v; }
    }
}

static void slot_set(struct hdr_packed_histogram* h, int32_t i, int64_t val)
{
    uint8_t* p = h->cnt + (size_t) i * h->width;
    switch (h->width)
    {
        case 1:  *p = (uint8_t) val; break;
        case 2:  { uint16_t v = (uint16_t) val; memcpy(p, &v, 2); break; }
        case 4:  { uint32_t v = (uint32_t) val; memcpy(p, &v, 4); break; }
        default: { uint64_t v = (uint64_t) val; memcpy(p, &v, 8); break; }
    }
}

/* Grow the uniform count width to hold `need`, re-packing existing values. */
static bool widen_to_fit(struct hdr_packed_histogram* h, int64_t need)
{
    uint8_t nw = h->width;
    while (need > width_max(nw) && nw < 8)
    {
        nw = (uint8_t)(nw * 2);
    }
    if (nw == h->width)
    {
        return true;
    }
    uint8_t* nb = (uint8_t*) PK_REALLOC(h->cnt, (size_t) h->cap * nw);
    if (!nb)
    {
        return false;
    }
    h->cnt = nb;
    /* expand in place, high slot first so a wider dest never clobbers an
       unread narrower source (i*nw > i*ow >= j*ow for all j<i). */
    uint8_t ow = h->width;
    for (int32_t i = h->size - 1; i >= 0; i--)
    {
        uint64_t v = 0;
        memcpy(&v, nb + (size_t) i * ow, ow);   /* read narrow */
        uint8_t* d = nb + (size_t) i * nw;
        memset(d, 0, nw);
        memcpy(d, &v, ow);                       /* little-endian widen */
    }
    h->width = nw;
    return true;
}

/* ---- sparse-array core ---------------------------------------------------- */

static int32_t lower_bound(const struct hdr_packed_histogram* h, int32_t key)
{
    int32_t lo = 0, hi = h->size;
    while (lo < hi)
    {
        int32_t mid = lo + (int32_t)(((uint32_t)(hi - lo)) >> 1); /* unsigned shift */
        if (h->idx[mid] < key) lo = mid + 1;
        else                   hi = mid;
    }
    return lo;
}

static bool ensure_cap(struct hdr_packed_histogram* h)
{
    if (h->size < h->cap)
    {
        return true;
    }
    if (h->cap > (INT32_MAX >> 1))   /* doubling would overflow int32 */
    {
        return false; /* GCOV_EXCL_DEFENSIVE: needs >2^30 distinct buckets;
                         guards int32 overflow, not reachable in practice. */
    }
    int32_t new_cap = h->cap ? h->cap * 2 : HDR_PACKED_INITIAL_CAP;
    int32_t* ni = (int32_t*) PK_REALLOC(h->idx, (size_t) new_cap * sizeof(int32_t));
    if (!ni) return false;
    h->idx = ni;
    uint8_t* nc = (uint8_t*) PK_REALLOC(h->cnt, (size_t) new_cap * h->width);
    if (!nc) return false;
    h->cnt = nc;
    h->cap = new_cap;
    return true;
}

static bool sparse_add(struct hdr_packed_histogram* h, int32_t vindex, int64_t delta)
{
    /* delta is >= 0 (record rejects count<0; decode values are non-negative). */
    int32_t p = lower_bound(h, vindex);
    if (p < h->size && h->idx[p] == vindex)
    {
        /* cur + delta cannot overflow: record_values already refuses a total
           past INT64_MAX (and cur <= total_count), and decode only inserts
           (monotonic index -> always the miss path below), never this hit path. */
        int64_t nv = slot_get(h, p) + delta;
        if (!widen_to_fit(h, nv))   /* no-op fast path when nv fits current width */
        {
            return false;
        }
        slot_set(h, p, nv);
        return true;
    }
    if (!ensure_cap(h))
    {
        return false;
    }
    if (!widen_to_fit(h, delta))
    {
        return false;
    }
    memmove(&h->idx[p + 1], &h->idx[p], (size_t)(h->size - p) * sizeof(int32_t));
    memmove(h->cnt + (size_t)(p + 1) * h->width,
            h->cnt + (size_t) p * h->width,
            (size_t)(h->size - p) * h->width);
    h->idx[p] = vindex;
    h->size++;               /* size updated before slot_set uses width offset */
    slot_set(h, p, delta);
    return true;
}

static int64_t sparse_get(const struct hdr_packed_histogram* h, int32_t vindex)
{
    int32_t p = lower_bound(h, vindex);
    if (p < h->size && h->idx[p] == vindex)
    {
        return slot_get(h, p);
    }
    return 0;
}

/* ---- geometry / config ---------------------------------------------------- */

int hdr_packed_config_create(
    int64_t lowest_discernible_value,
    int64_t highest_trackable_value,
    int significant_figures,
    struct hdr_packed_config** result)
{
    struct hdr_histogram_bucket_config bcfg;
    int r = hdr_calculate_bucket_config(
        lowest_discernible_value, highest_trackable_value, significant_figures, &bcfg);
    if (r)
    {
        return r;
    }
    struct hdr_packed_config* c = (struct hdr_packed_config*) PK_CALLOC(1, sizeof(*c));
    if (!c)
    {
        return ENOMEM;
    }
    hdr_init_preallocated(&c->geom, &bcfg);
    c->geom.counts = NULL;
    *result = c;
    return 0;
}

void hdr_packed_config_destroy(struct hdr_packed_config* cfg)
{
    free(cfg);
}

size_t hdr_packed_config_memory_size(const struct hdr_packed_config* cfg)
{
    (void) cfg;
    return sizeof(struct hdr_packed_config);
}

/* ---- histogram lifecycle -------------------------------------------------- */

static int packed_init_with(const struct hdr_packed_config* cfg, bool owns,
                            struct hdr_packed_histogram** result)
{
    struct hdr_packed_histogram* h = (struct hdr_packed_histogram*) PK_CALLOC(1, sizeof(*h));
    if (!h)
    {
        return ENOMEM;
    }
    h->cfg = cfg;
    h->owns_cfg = owns;
    h->idx = NULL;
    h->cnt = NULL;
    h->size = 0;
    h->cap = 0;
    h->width = 1;
    h->total_count = 0;
    h->min_value = INT64_MAX;
    h->max_value = 0;
    *result = h;
    return 0;
}

int hdr_packed_init_shared(const struct hdr_packed_config* cfg,
                           struct hdr_packed_histogram** result)
{
    return packed_init_with(cfg, false, result);
}

int hdr_packed_init(
    int64_t lowest_discernible_value,
    int64_t highest_trackable_value,
    int significant_figures,
    struct hdr_packed_histogram** result)
{
    struct hdr_packed_config* c = NULL;
    int r = hdr_packed_config_create(
        lowest_discernible_value, highest_trackable_value, significant_figures, &c);
    if (r)
    {
        return r;
    }
    r = packed_init_with(c, true, result);
    if (r)
    {
        hdr_packed_config_destroy(c);
    }
    return r;
}

void hdr_packed_close(struct hdr_packed_histogram* h)
{
    if (h)
    {
        if (h->owns_cfg)
        {
            hdr_packed_config_destroy((struct hdr_packed_config*) h->cfg);
        }
        free(h->idx);
        free(h->cnt);
        free(h);
    }
}

/* ---- record / query ------------------------------------------------------- */

bool hdr_packed_record_values(struct hdr_packed_histogram* h, int64_t value, int64_t count)
{
    const struct hdr_histogram* g = &h->cfg->geom;
    if (value < 0 || g->highest_trackable_value < value)
    {
        return false;
    }
    if (count < 0)
    {
        /* Packed stores counts unsigned; a negative count cannot round-trip and
           would diverge from dense. Reject it (stricter than dense, which is UB
           on negative counts anyway). */
        return false;
    }
    int32_t counts_index = counts_index_for(g, value);
    if ((uint32_t) counts_index >= (uint32_t) g->counts_len)
    {
        return false; /* GCOV_EXCL_DEFENSIVE: bounds guard, parity with dense
                         hdr_record_values; the value<=highest check above bounds
                         the index for any valid geometry. Kept, not covered. */
    }
    if (count > INT64_MAX - h->total_count)
    {
        return false; /* total_count would overflow int64 (checked before any
                         mutation so the histogram stays consistent) */
    }
    /* count==0 matches dense: no bucket change, but min/max/total still update. */
    if (count != 0 && !sparse_add(h, counts_index, count))
    {
        return false;
    }
    h->total_count += count;
    if (value < h->min_value && value != 0)
    {
        h->min_value = value;
    }
    if (value > h->max_value)
    {
        h->max_value = value;
    }
    return true;
}

bool hdr_packed_record_value(struct hdr_packed_histogram* h, int64_t value)
{
    return hdr_packed_record_values(h, value, 1);
}

void hdr_packed_reset(struct hdr_packed_histogram* h)
{
    /* Mirrors dense hdr_reset: retains the allocated arrays AND the current count
       width, zeroing only the contents. No realloc -> no reset/re-widen thrash in
       a record->snapshot->reset loop, and get_memory_size stays accurate (it
       reports the bytes actually held, which are unchanged). Capacity/width are
       reclaimed only by hdr_packed_close, exactly as dense keeps counts[]. */
    h->size = 0;
    h->total_count = 0;
    h->min_value = INT64_MAX;
    h->max_value = 0;
}

int64_t hdr_packed_total_count(const struct hdr_packed_histogram* h)
{
    return h->total_count;
}

double hdr_packed_mean(const struct hdr_packed_histogram* h)
{
    if (h->total_count == 0)
    {
        return 0.0;   /* Java semantics for an empty histogram */
    }
    const struct hdr_histogram* g = &h->cfg->geom;
    /* accumulate in double: on an untrusted decoded histogram a width-8 count at
       a large value makes the int64 product c*median overflow (UB). double is
       within tolerance of dense for in-range data and well-defined for all. */
    double total = 0.0;
    for (int32_t i = 0; i < h->size; i++)
    {
        int64_t c = slot_get(h, i);
        if (c != 0)
        {
            total += (double) c * (double) hdr_median_equivalent_value(g, hdr_value_at_index(g, h->idx[i]));
        }
    }
    return total / (double) h->total_count;
}

double hdr_packed_stddev(const struct hdr_packed_histogram* h)
{
    if (h->total_count == 0)
    {
        return 0.0;
    }
    const struct hdr_histogram* g = &h->cfg->geom;
    double mean = hdr_packed_mean(h);
    double geometric_dev_total = 0.0;
    for (int32_t i = 0; i < h->size; i++)
    {
        int64_t c = slot_get(h, i);
        if (c != 0)
        {
            double dev = (hdr_median_equivalent_value(g, hdr_value_at_index(g, h->idx[i])) * 1.0) - mean;
            geometric_dev_total += (dev * dev) * c;
        }
    }
    return sqrt(geometric_dev_total / h->total_count);
}

int64_t hdr_packed_count_at_value(const struct hdr_packed_histogram* h, int64_t value)
{
    const struct hdr_histogram* g = &h->cfg->geom;
    int32_t counts_index = counts_index_for(g, value);
    if ((uint32_t) counts_index >= (uint32_t) g->counts_len)
    {
        return 0;
    }
    return sparse_get(h, counts_index);
}

/* Top of value v's bucket = dense's next_non_equivalent(v)-1, but overflow-safe:
   when the bucket's upper edge exceeds INT64_MAX (only reachable with a top-bucket
   value at highest_trackable_value == INT64_MAX) the dense expression wraps
   (signed-overflow UB). Clamp to INT64_MAX -- the value dense's wrap yields on a
   two's-complement target -- so results match dense without the UB. Used on both
   the query paths and the untrusted decode recompute. */
static int64_t packed_highest_equivalent(const struct hdr_histogram* g, int64_t v)
{
    int64_t leq  = hdr_lowest_equivalent_value(g, v);
    int64_t size = hdr_size_of_equivalent_value_range(g, v);
    return (leq > INT64_MAX - size) ? INT64_MAX : leq + size - 1;
}

int64_t hdr_packed_max(const struct hdr_packed_histogram* h)
{
    if (0 == h->max_value)
    {
        return 0;
    }
    return packed_highest_equivalent(&h->cfg->geom, h->max_value);
}

int64_t hdr_packed_min(const struct hdr_packed_histogram* h)
{
    if (sparse_get(h, 0) > 0)
    {
        return 0;
    }
    if (INT64_MAX == h->min_value)
    {
        return INT64_MAX;
    }
    return hdr_lowest_equivalent_value(&h->cfg->geom, h->min_value);
}

int64_t hdr_packed_value_at_percentile(const struct hdr_packed_histogram* h, double percentile)
{
    const struct hdr_histogram* g = &h->cfg->geom;
    double requested = percentile < 100.0 ? percentile : 100.0;
    /* Resolve the target cumulative count to [1, total_count] before the int64
       cast. The cast must be guarded (NaN/+/-inf/negative/>=2^63 are all UB), and
       the target is clamped to total_count rather than INT64_MAX: when
       total_count is just below INT64_MAX the double product rounds up to 2^63,
       and clamping to INT64_MAX would make the target unreachable by the running
       sum (which maxes at total_count), wrongly returning bucket 0 for p100. */
    double cc = ((requested / 100.0) * (double) h->total_count) + 0.5;
    int64_t count_at_percentile;
    if (!(cc >= 1.0))                            /* NaN or below 1 (incl. -inf) */
        count_at_percentile = 1;
    else if (cc >= (double) h->total_count)      /* p100/+inf, or a target that rounds >= total */
        count_at_percentile = h->total_count;    /* clamp to the reachable total -> last/max bucket.
                                                    Keeps p100 == max even when total_count > 2^53
                                                    (where dense's FP-rounded int64 target undershoots
                                                    to an earlier bucket) and in the dense-UB band near
                                                    INT64_MAX. packed is more correct than dense here. */
    else
        count_at_percentile = (int64_t) cc;      /* < total: bit-for-bit dense's target */

    int64_t running = 0;
    int64_t value_from_idx = 0;
    for (int32_t i = 0; i < h->size; i++)
    {
        /* saturate: a crafted decoded histogram can have buckets summing past
           INT64_MAX before reaching the target percentile; a bare += would be
           signed-overflow UB on this untrusted-input path. */
        int64_t c = slot_get(h, i);
        running = (c > INT64_MAX - running) ? INT64_MAX : running + c;
        if (running >= count_at_percentile)
        {
            value_from_idx = hdr_value_at_index(g, h->idx[i]);
            break;
        }
    }

    if (percentile == 0.0)
    {
        return hdr_lowest_equivalent_value(g, value_from_idx);
    }
    return packed_highest_equivalent(g, value_from_idx);
}

int hdr_packed_value_at_percentiles(const struct hdr_packed_histogram* h,
    const double* percentiles, int64_t* values, size_t length)
{
    if (NULL == percentiles || NULL == values)
    {
        return EINVAL;
    }
    for (size_t i = 0; i < length; i++)
    {
        values[i] = hdr_packed_value_at_percentile(h, percentiles[i]);
    }
    return 0;
}

size_t hdr_packed_get_memory_size(const struct hdr_packed_histogram* h)
{
    size_t sz = sizeof(struct hdr_packed_histogram)
              + (size_t) h->cap * (sizeof(int32_t) + h->width);
    if (h->owns_cfg)
    {
        sz += hdr_packed_config_memory_size(h->cfg);
    }
    return sz;
}

int32_t hdr_packed_populated(const struct hdr_packed_histogram* h)
{
    return h->size;
}

int hdr_packed_count_width(const struct hdr_packed_histogram* h)
{
    return h->width;
}

/* ##  V2 serialization  ####################################################### */

#define PK_MAX_LEB128 9
static const uint32_t PK_V2_ENCODING_COOKIE    = 0x1c849303;
static const uint32_t PK_V2_COMPRESSION_COOKIE = 0x1c849304;

#pragma pack(push, 1)
typedef struct {
    uint32_t cookie;
    int32_t  payload_len;
    int32_t  normalizing_index_offset;
    int32_t  significant_figures;
    int64_t  lowest_discernible_value;
    int64_t  highest_trackable_value;
    uint64_t conversion_ratio_bits;
    uint8_t  counts[1];
} pk_encoding_flyweight_t;

typedef struct {
    uint32_t cookie;
    int32_t  length;
    uint8_t  data[1];
} pk_compression_flyweight_t;
#pragma pack(pop)

#define PK_SIZEOF_ENC (sizeof(pk_encoding_flyweight_t) - sizeof(uint8_t))
#define PK_SIZEOF_CMP (sizeof(pk_compression_flyweight_t) - sizeof(uint8_t))

static uint32_t pk_cookie_base(uint32_t raw_be)
{
    return be32toh(raw_be) & ~0xf0U;
}

static uint64_t pk_dbl_to_bits(double d)
{
    uint64_t l; memcpy(&l, &d, sizeof l); return l;
}
/* Packed histograms do not carry conversion_ratio; encode emits the 1.0 default
   and decode ignores the field, so no bits->double helper is needed. */

/* Recompute total/min/max from the sparse counts -- mirrors the dense
   hdr_reset_internal_counters so a decoded packed histogram answers queries
   identically to a decoded dense one. */
static void packed_recompute_stats(struct hdr_packed_histogram* h)
{
    const struct hdr_histogram* g = &h->cfg->geom;
    int64_t total = 0;
    int32_t max_i = -1, min_nz = -1;
    for (int32_t k = 0; k < h->size; k++)
    {
        int64_t c = slot_get(h, k);
        if (c > 0)
        {
            /* saturate: a crafted multi-bucket stream can sum past INT64_MAX
               (untrusted decode path; dense hdr_reset_internal_counters has the
               same int64 sum but is not fed untrusted input here). */
            total = (c > INT64_MAX - total) ? INT64_MAX : total + c;
            max_i = h->idx[k];
            if (min_nz == -1 && h->idx[k] != 0) min_nz = h->idx[k];
        }
    }
    h->total_count = total;
    h->max_value = (max_i == -1) ? 0
        : packed_highest_equivalent(g, hdr_value_at_index(g, max_i));
    h->min_value = (min_nz == -1) ? INT64_MAX : hdr_value_at_index(g, min_nz);
}

int hdr_packed_encode_compressed(
    const struct hdr_packed_histogram* h, uint8_t** compressed_histogram, size_t* compressed_len)
{
    const struct hdr_histogram* g = &h->cfg->geom;
    int result = 0;
    pk_encoding_flyweight_t* enc = NULL;
    pk_compression_flyweight_t* cmp = NULL;

    /* identical limit to dense: counts_index_for(max)+1 clamped to counts_len */
    int32_t len_to_max = counts_index_for(g, h->max_value) + 1;
    int32_t counts_limit = len_to_max < g->counts_len ? len_to_max : g->counts_len;
    if (counts_limit < 0) counts_limit = 0;

    /* Emit-token budget is O(populated), NOT O(counts_limit): at most one value
       token per populated bucket plus one zero-run token before each and one
       trailing => <= 2*size+1 tokens, each <= PK_MAX_LEB128 bytes. This both
       sizes the scratch to the populated set and makes encode O(populated) in
       time (we walk the sparse array, not the index range). */
    size_t max_tokens = (size_t) 2 * (size_t) h->size + 1;
    size_t enc_cap = PK_SIZEOF_ENC + (size_t) PK_MAX_LEB128 * max_tokens;
    enc = (pk_encoding_flyweight_t*) PK_CALLOC(enc_cap ? enc_cap : 1, 1);
    if (!enc) { result = ENOMEM; goto done; }

    int64_t data_index = 0;          /* size_t-range cursor (payload_len is int32) */
    int32_t prev = 0;                /* next index expected to be emitted */
    for (int32_t k = 0; k < h->size && h->idx[k] < counts_limit; k++)
    {
        int32_t gap = h->idx[k] - prev;               /* zeros before this bucket */
        if (gap > 0)
            data_index += zig_zag_encode_i64(&enc->counts[data_index], -(int64_t) gap);
        data_index += zig_zag_encode_i64(&enc->counts[data_index], slot_get(h, k));
        prev = h->idx[k] + 1;
    }
    if (prev < counts_limit)          /* trailing zero-run (also the empty case) */
        data_index += zig_zag_encode_i64(&enc->counts[data_index], -(int64_t)(counts_limit - prev));

    if (data_index > INT32_MAX) { result = EOVERFLOW; goto done; } /* payload_len is int32 */

    enc->cookie                   = htobe32(PK_V2_ENCODING_COOKIE | 0x10U);
    enc->payload_len              = htobe32((int32_t) data_index);
    enc->normalizing_index_offset = htobe32(0);
    enc->significant_figures      = htobe32(g->significant_figures);
    enc->lowest_discernible_value = htobe64(g->lowest_discernible_value);
    enc->highest_trackable_value  = htobe64(g->highest_trackable_value);
    enc->conversion_ratio_bits    = htobe64(pk_dbl_to_bits(1.0));

    uLong enc_size = (uLong)(PK_SIZEOF_ENC + data_index);
    uLongf dest_len = compressBound(enc_size);
    cmp = (pk_compression_flyweight_t*) PK_MALLOC(PK_SIZEOF_CMP + dest_len);
    if (!cmp) { result = ENOMEM; goto done; }

    if (Z_OK != PK_COMPRESS(cmp->data, &dest_len, (Bytef*) enc, enc_size))
    {
        result = HDR_DEFLATE_FAIL; goto done;
    }
    cmp->cookie = htobe32(PK_V2_COMPRESSION_COOKIE | 0x10U);
    cmp->length = htobe32((int32_t) dest_len);

    *compressed_histogram = (uint8_t*) cmp;
    *compressed_len = PK_SIZEOF_CMP + dest_len;
    cmp = NULL;

done:
    free(enc);
    free(cmp);
    return result;
}

int hdr_packed_decode_compressed(
    uint8_t* compressed_histogram, size_t length, struct hdr_packed_histogram** result)
{
    if (length < PK_SIZEOF_CMP) return EINVAL;
    pk_compression_flyweight_t* cmp = (pk_compression_flyweight_t*) compressed_histogram;
    if (pk_cookie_base(cmp->cookie) != PK_V2_COMPRESSION_COOKIE) return HDR_COMPRESSION_COOKIE_MISMATCH;

    int32_t comp_len = be32toh(cmp->length);
    if (comp_len < 0 || length - PK_SIZEOF_CMP < (size_t) comp_len) return EINVAL;

    int rc = 0, ret = 0;
    uint8_t* counts_array = NULL;
    struct hdr_packed_config* cfg = NULL;
    struct hdr_packed_histogram* h = NULL;
    pk_encoding_flyweight_t hdr;
    memset(&hdr, 0, sizeof hdr);   /* avoid reading indeterminate bytes if the
                                      header inflates short (see avail_out check) */
    z_stream strm;
    memset(&strm, 0, sizeof strm);

    if (inflateInit(&strm) != Z_OK) return HDR_INFLATE_INIT_FAIL;

    strm.next_in = cmp->data;
    strm.avail_in = (uInt) comp_len;
    strm.next_out = (uint8_t*) &hdr;
    strm.avail_out = PK_SIZEOF_ENC;
    if (inflate(&strm, Z_SYNC_FLUSH) != Z_OK) { ret = HDR_INFLATE_FAIL; goto done; }
    if (strm.avail_out != 0) { ret = HDR_INFLATE_FAIL; goto done; } /* full header required */

    if (pk_cookie_base(hdr.cookie) != PK_V2_ENCODING_COOKIE) { ret = HDR_ENCODING_COOKIE_MISMATCH; goto done; }

    /* Packed histograms are never rotated, so a non-zero normalizing_index_offset
       (present in dense streams from decoded/shifted histograms) cannot be
       honored here. Reject rather than silently mis-index (the PR #137 class). */
    if (be32toh(hdr.normalizing_index_offset) != 0) { ret = EINVAL; goto done; }

    int32_t counts_limit = be32toh(hdr.payload_len);
    int64_t low  = be64toh(hdr.lowest_discernible_value);
    int64_t high = be64toh(hdr.highest_trackable_value);
    int32_t sig  = be32toh(hdr.significant_figures);
    if (counts_limit < 0) { ret = EINVAL; goto done; }

    rc = hdr_packed_config_create(low, high, sig, &cfg);
    if (rc) { ret = rc; goto done; }
    rc = packed_init_with(cfg, true, &h);
    if (rc) { hdr_packed_config_destroy(cfg); ret = rc; goto done; }

    /* Bound the payload against the geometry before allocating: a valid zig-zag
       stream for counts_len buckets is at most PK_MAX_LEB128 bytes per bucket.
       This caps attacker-driven allocation (decompression-bomb defense). */
    if ((int64_t) counts_limit > (int64_t) PK_MAX_LEB128 * h->cfg->geom.counts_len)
    {
        ret = HDR_ENCODED_INPUT_TOO_LONG; goto done;
    }

    counts_array = (uint8_t*) PK_CALLOC(1, (size_t) counts_limit + 9);
    if (!counts_array) { ret = ENOMEM; goto done; }
    strm.next_out = counts_array;
    strm.avail_out = (uInt) counts_limit;
    if (inflate(&strm, Z_FINISH) != Z_STREAM_END) { ret = HDR_INFLATE_FAIL; goto done; }

    /* apply zig-zag payload into the sparse structure */
    {
        int64_t data_index = 0;
        int32_t counts_index = 0;
        const int32_t clen = h->cfg->geom.counts_len;
        while (data_index < counts_limit && counts_index < clen)
        {
            int64_t value;
            data_index += zig_zag_decode_i64(&counts_array[data_index], &value);
            if (value < 0)
            {
                if (value <= INT32_MIN) { ret = HDR_TRAILING_ZEROS_INVALID; goto done; } /* before negation: -INT64_MIN is UB */
                int64_t zeros = -value;
                if (counts_index + zeros > clen) { ret = HDR_TRAILING_ZEROS_INVALID; goto done; }
                counts_index += (int32_t) zeros;
            }
            else
            {
                if (value > 0 && !sparse_add(h, counts_index, value)) { ret = ENOMEM; goto done; }
                counts_index++;
            }
        }
        if (data_index > counts_limit) { ret = HDR_VALUE_TRUNCATED; goto done; }
        if (data_index < counts_limit) { ret = HDR_ENCODED_INPUT_TOO_LONG; goto done; }
    }

    packed_recompute_stats(h);
    *result = h;
    h = NULL;

done:
    (void) inflateEnd(&strm);
    free(counts_array);
    if (h) hdr_packed_close(h);
    return ret;
}
