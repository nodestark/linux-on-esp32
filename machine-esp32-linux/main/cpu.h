uint64_t pack754_64(double d){ return *((uint64_t*) &d); }
double unpack754_64(uint64_t ieee754){ return *((double*) &ieee754); }

uint32_t pack754_32(float f){ return *(uint32_t*) &f; }
float unpack754_32(uint32_t ieee754){ return *(float*) &ieee754; }