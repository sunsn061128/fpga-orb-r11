#ifndef ORB60_MOCK_H
#define ORB60_MOCK_H
#include <stdint.h>
typedef uint64_t XTime;
#define COUNTS_PER_SECOND 333333343ull
uint8_t *orb_memory(uint32_t address);
#define MEM(a) orb_memory(a)
void XTime_GetTime(XTime *time);
void Xil_Out32(uint32_t address,uint32_t value);
uint32_t Xil_In32(uint32_t address);
void Xil_DCacheInvalidateRange(uint32_t address,uint32_t size);
void Xil_DCacheFlushRange(uint32_t address,uint32_t size);
#endif
