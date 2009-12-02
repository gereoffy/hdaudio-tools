
static inline uint16_t bswap_16(uint16_t x){
#ifdef BSWAP
    x= (x>>8) | (x<<8);
#endif
    return x;
}

static inline uint32_t bswap_32(uint32_t x){
#ifdef BSWAP
    x= ((x<<8)&0xFF00FF00) | ((x>>8)&0x00FF00FF);
    x= (x>>16) | (x<<16);
#endif
    return x;
}
