
extern void *memcpy_armv5te(void *dest, const void *src, int n);

#ifdef RPI_BEST_MEMCPY_ONLY

extern void *memcpy_armv5te_no_overfetch(void *dest, const void *src, int n);

extern void *memcpy_armv5te_overfetch(void *dest, const void *src, int n);

#else

extern void *memcpy_armv5te_no_overfetch_align_16_block_write_8_preload_96(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_align_16_block_write_16_preload_96(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_align_16_block_write_16_preload_early_96(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_align_16_block_write_16_preload_early_128(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_align_32_block_write_8_preload_96(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_align_32_block_write_16_preload_64(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_align_32_block_write_16_preload_96(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_align_32_block_write_16_preload_128(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_align_32_block_write_16_preload_160(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_align_32_block_write_16_preload_192(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_align_32_block_write_16_preload_256(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_align_32_block_write_32_preload_64(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_align_32_block_write_32_preload_96(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_align_32_block_write_32_preload_128(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_align_32_block_write_32_preload_160(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_align_32_block_write_32_preload_192(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_align_32_block_write_32_preload_256(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_align_32_block_write_16_preload_early_96(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_align_32_block_write_16_preload_early_128(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_align_32_block_write_16_preload_early_192(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_align_32_block_write_16_preload_early_256(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_align_32_block_write_32_preload_early_128(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_align_32_block_write_32_preload_early_192(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_align_32_block_write_32_preload_early_256(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_align_32_block_write_16_no_preload(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_align_32_block_write_32_no_preload(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_line_64_align_32_block_write_32_preload_early_128(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_line_64_align_32_block_write_32_preload_early_192(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_line_64_align_32_block_write_32_preload_early_256(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_line_64_align_32_block_write_32_preload_early_320(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_line_64_align_64_block_write_32_preload_early_256(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_no_overfetch_line_64_align_64_block_write_32_preload_early_320(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_overfetch_align_16_block_write_16_preload_early_128(void *dest,
    const void *src, int n);

extern void *memcpy_armv5te_overfetch_align_32_block_write_32_preload_early_192(void *dest,
    const void *src, int n);

#endif
