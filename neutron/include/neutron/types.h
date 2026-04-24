/* ===========================================================================
 * Neutron Kernel — Fundamental Types
 * =========================================================================== */

#ifndef NEUTRON_TYPES_H
#define NEUTRON_TYPES_H

/* ---- Fixed-width integers ----------------------------------------------- */
typedef unsigned char           u8;
typedef unsigned short          u16;
typedef unsigned int            u32;
typedef unsigned long           u64;
typedef signed char             s8;
typedef signed short            s16;
typedef signed int              s32;
typedef signed long             s64;

/* ---- Size and address types --------------------------------------------- */
typedef u64                     usize;
typedef s64                     ssize;
typedef u64                     paddr_t;        /* Physical address */
typedef u64                     vaddr_t;        /* Virtual address */
typedef u64                     reg_t;          /* CPU register width */

/* ---- Boolean ------------------------------------------------------------ */
typedef _Bool                   bool;
#define true                    1
#define false                   0

/* ---- NULL --------------------------------------------------------------- */
#ifndef NULL
#define NULL                    ((void *)0)
#endif

/* ---- Limits ------------------------------------------------------------- */
#define U8_MAX                  ((u8)0xFF)
#define U16_MAX                 ((u16)0xFFFF)
#define U32_MAX                 ((u32)0xFFFFFFFF)
#define U64_MAX                 ((u64)0xFFFFFFFFFFFFFFFF)
#define S64_MAX                 ((s64)0x7FFFFFFFFFFFFFFF)
#define USIZE_MAX               U64_MAX

/* ---- Utility macros ----------------------------------------------------- */
#define ALIGN_UP(x, a)          (((x) + ((a) - 1)) & ~((a) - 1))
#define ALIGN_DOWN(x, a)        ((x) & ~((a) - 1))
#define IS_ALIGNED(x, a)        (((x) & ((a) - 1)) == 0)
#define ARRAY_SIZE(a)           (sizeof(a) / sizeof((a)[0]))
#define MIN(a, b)               ((a) < (b) ? (a) : (b))
#define MAX(a, b)               ((a) > (b) ? (a) : (b))
#define CLAMP(x, lo, hi)        MIN(MAX(x, lo), hi)
#define BIT(n)                  (1ULL << (n))
#define MASK(n)                 (BIT(n) - 1)

/* ---- Compiler attributes ------------------------------------------------ */
#define PACKED                  __attribute__((packed))
#define ALIGNED(n)              __attribute__((aligned(n)))
#define UNUSED                  __attribute__((unused))
#define NORETURN                __attribute__((noreturn))
#define ALWAYS_INLINE           __attribute__((always_inline)) inline
#define LIKELY(x)               __builtin_expect(!!(x), 1)
#define UNLIKELY(x)             __builtin_expect(!!(x), 0)
#define SECTION(s)              __attribute__((section(s)))

/* ---- Memory barriers ---------------------------------------------------- */
#define barrier()               __asm__ volatile("" ::: "memory")
#define dsb(opt)                __asm__ volatile("dsb " #opt ::: "memory")
#define dmb(opt)                __asm__ volatile("dmb " #opt ::: "memory")
#define isb()                   __asm__ volatile("isb" ::: "memory")

/* ---- Page constants ----------------------------------------------------- */
#define PAGE_SHIFT              12
#define PAGE_SIZE               (1UL << PAGE_SHIFT)     /* 4 KB */
#define PAGE_MASK               (~(PAGE_SIZE - 1))

/* ---- Neutron result codes ----------------------------------------------- */
typedef enum {
    NK_OK           =  0,
    NK_ERR_NOMEM    = -1,       /* Out of memory */
    NK_ERR_INVAL    = -2,       /* Invalid argument */
    NK_ERR_PERM     = -3,       /* Permission denied (capability check failed) */
    NK_ERR_NOENT    = -4,       /* Not found */
    NK_ERR_BUSY     = -5,       /* Resource busy */
    NK_ERR_FAULT    = -6,       /* Bad address */
    NK_ERR_EXIST    = -7,       /* Already exists */
    NK_ERR_RANGE    = -8,       /* Out of range */
    NK_ERR_DEADLK   = -9,       /* Deadlock detected */
    NK_ERR_IO       = -10,      /* I/O error */
    NK_ERR_NOSYS    = -11,      /* Not implemented */
} nk_result_t;

/* ---- Capability types (forward declarations) ---------------------------- */
typedef u64 cap_id_t;           /* Capability identifier */
typedef u32 cap_rights_t;       /* Capability rights bitmask */

#endif /* NEUTRON_TYPES_H */
