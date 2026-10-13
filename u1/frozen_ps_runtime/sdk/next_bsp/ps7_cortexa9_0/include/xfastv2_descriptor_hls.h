// ==============================================================
// File generated on Tue Oct 13 18:46:32 +0800 2026
// Vivado(TM) HLS - High-Level Synthesis from C, C++ and SystemC v2018.3 (64-bit)
// SW Build 2405991 on Thu Dec  6 23:38:27 MST 2018
// IP Build 2404404 on Fri Dec  7 01:43:56 MST 2018
// Copyright 1986-2018 Xilinx, Inc. All Rights Reserved.
// ==============================================================
#ifndef XFASTV2_DESCRIPTOR_HLS_H
#define XFASTV2_DESCRIPTOR_HLS_H

#ifdef __cplusplus
extern "C" {
#endif

/***************************** Include Files *********************************/
#ifndef __linux__
#include "xil_types.h"
#include "xil_assert.h"
#include "xstatus.h"
#include "xil_io.h"
#else
#include <stdint.h>
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stddef.h>
#endif
#include "xfastv2_descriptor_hls_hw.h"

/**************************** Type Definitions ******************************/
#ifdef __linux__
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
#else
typedef struct {
    u16 DeviceId;
    u32 Control_BaseAddress;
} XFastv2_descriptor_hls_Config;
#endif

typedef struct {
    u32 Control_BaseAddress;
    u32 IsReady;
} XFastv2_descriptor_hls;

/***************** Macros (Inline Functions) Definitions *********************/
#ifndef __linux__
#define XFastv2_descriptor_hls_WriteReg(BaseAddress, RegOffset, Data) \
    Xil_Out32((BaseAddress) + (RegOffset), (u32)(Data))
#define XFastv2_descriptor_hls_ReadReg(BaseAddress, RegOffset) \
    Xil_In32((BaseAddress) + (RegOffset))
#else
#define XFastv2_descriptor_hls_WriteReg(BaseAddress, RegOffset, Data) \
    *(volatile u32*)((BaseAddress) + (RegOffset)) = (u32)(Data)
#define XFastv2_descriptor_hls_ReadReg(BaseAddress, RegOffset) \
    *(volatile u32*)((BaseAddress) + (RegOffset))

#define Xil_AssertVoid(expr)    assert(expr)
#define Xil_AssertNonvoid(expr) assert(expr)

#define XST_SUCCESS             0
#define XST_DEVICE_NOT_FOUND    2
#define XST_OPEN_DEVICE_FAILED  3
#define XIL_COMPONENT_IS_READY  1
#endif

/************************** Function Prototypes *****************************/
#ifndef __linux__
int XFastv2_descriptor_hls_Initialize(XFastv2_descriptor_hls *InstancePtr, u16 DeviceId);
XFastv2_descriptor_hls_Config* XFastv2_descriptor_hls_LookupConfig(u16 DeviceId);
int XFastv2_descriptor_hls_CfgInitialize(XFastv2_descriptor_hls *InstancePtr, XFastv2_descriptor_hls_Config *ConfigPtr);
#else
int XFastv2_descriptor_hls_Initialize(XFastv2_descriptor_hls *InstancePtr, const char* InstanceName);
int XFastv2_descriptor_hls_Release(XFastv2_descriptor_hls *InstancePtr);
#endif

void XFastv2_descriptor_hls_Start(XFastv2_descriptor_hls *InstancePtr);
u32 XFastv2_descriptor_hls_IsDone(XFastv2_descriptor_hls *InstancePtr);
u32 XFastv2_descriptor_hls_IsIdle(XFastv2_descriptor_hls *InstancePtr);
u32 XFastv2_descriptor_hls_IsReady(XFastv2_descriptor_hls *InstancePtr);
void XFastv2_descriptor_hls_EnableAutoRestart(XFastv2_descriptor_hls *InstancePtr);
void XFastv2_descriptor_hls_DisableAutoRestart(XFastv2_descriptor_hls *InstancePtr);
u32 XFastv2_descriptor_hls_Get_return(XFastv2_descriptor_hls *InstancePtr);

void XFastv2_descriptor_hls_Set_raw(XFastv2_descriptor_hls *InstancePtr, u32 Data);
u32 XFastv2_descriptor_hls_Get_raw(XFastv2_descriptor_hls *InstancePtr);
void XFastv2_descriptor_hls_Set_blurred(XFastv2_descriptor_hls *InstancePtr, u32 Data);
u32 XFastv2_descriptor_hls_Get_blurred(XFastv2_descriptor_hls *InstancePtr);
void XFastv2_descriptor_hls_Set_points(XFastv2_descriptor_hls *InstancePtr, u32 Data);
u32 XFastv2_descriptor_hls_Get_points(XFastv2_descriptor_hls *InstancePtr);
void XFastv2_descriptor_hls_Set_features(XFastv2_descriptor_hls *InstancePtr, u32 Data);
u32 XFastv2_descriptor_hls_Get_features(XFastv2_descriptor_hls *InstancePtr);
void XFastv2_descriptor_hls_Set_stats(XFastv2_descriptor_hls *InstancePtr, u32 Data);
u32 XFastv2_descriptor_hls_Get_stats(XFastv2_descriptor_hls *InstancePtr);
void XFastv2_descriptor_hls_Set_width(XFastv2_descriptor_hls *InstancePtr, u32 Data);
u32 XFastv2_descriptor_hls_Get_width(XFastv2_descriptor_hls *InstancePtr);
void XFastv2_descriptor_hls_Set_height(XFastv2_descriptor_hls *InstancePtr, u32 Data);
u32 XFastv2_descriptor_hls_Get_height(XFastv2_descriptor_hls *InstancePtr);
void XFastv2_descriptor_hls_Set_count(XFastv2_descriptor_hls *InstancePtr, u32 Data);
u32 XFastv2_descriptor_hls_Get_count(XFastv2_descriptor_hls *InstancePtr);
void XFastv2_descriptor_hls_Set_level(XFastv2_descriptor_hls *InstancePtr, u32 Data);
u32 XFastv2_descriptor_hls_Get_level(XFastv2_descriptor_hls *InstancePtr);
void XFastv2_descriptor_hls_Set_frame_offset(XFastv2_descriptor_hls *InstancePtr, u32 Data);
u32 XFastv2_descriptor_hls_Get_frame_offset(XFastv2_descriptor_hls *InstancePtr);
void XFastv2_descriptor_hls_Set_capacity(XFastv2_descriptor_hls *InstancePtr, u32 Data);
u32 XFastv2_descriptor_hls_Get_capacity(XFastv2_descriptor_hls *InstancePtr);

void XFastv2_descriptor_hls_InterruptGlobalEnable(XFastv2_descriptor_hls *InstancePtr);
void XFastv2_descriptor_hls_InterruptGlobalDisable(XFastv2_descriptor_hls *InstancePtr);
void XFastv2_descriptor_hls_InterruptEnable(XFastv2_descriptor_hls *InstancePtr, u32 Mask);
void XFastv2_descriptor_hls_InterruptDisable(XFastv2_descriptor_hls *InstancePtr, u32 Mask);
void XFastv2_descriptor_hls_InterruptClear(XFastv2_descriptor_hls *InstancePtr, u32 Mask);
u32 XFastv2_descriptor_hls_InterruptGetEnabled(XFastv2_descriptor_hls *InstancePtr);
u32 XFastv2_descriptor_hls_InterruptGetStatus(XFastv2_descriptor_hls *InstancePtr);

#ifdef __cplusplus
}
#endif

#endif
