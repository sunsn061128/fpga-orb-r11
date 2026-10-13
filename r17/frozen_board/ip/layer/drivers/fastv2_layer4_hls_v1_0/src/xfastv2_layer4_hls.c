// ==============================================================
// File generated on Tue Oct 13 15:03:35 +0800 2026
// Vivado(TM) HLS - High-Level Synthesis from C, C++ and SystemC v2018.3 (64-bit)
// SW Build 2405991 on Thu Dec  6 23:38:27 MST 2018
// IP Build 2404404 on Fri Dec  7 01:43:56 MST 2018
// Copyright 1986-2018 Xilinx, Inc. All Rights Reserved.
// ==============================================================
/***************************** Include Files *********************************/
#include "xfastv2_layer4_hls.h"

/************************** Function Implementation *************************/
#ifndef __linux__
int XFastv2_layer4_hls_CfgInitialize(XFastv2_layer4_hls *InstancePtr, XFastv2_layer4_hls_Config *ConfigPtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(ConfigPtr != NULL);

    InstancePtr->Control_BaseAddress = ConfigPtr->Control_BaseAddress;
    InstancePtr->IsReady = XIL_COMPONENT_IS_READY;

    return XST_SUCCESS;
}
#endif

void XFastv2_layer4_hls_Start(XFastv2_layer4_hls *InstancePtr) {
    u32 Data;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XFastv2_layer4_hls_ReadReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_AP_CTRL) & 0x80;
    XFastv2_layer4_hls_WriteReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_AP_CTRL, Data | 0x01);
}

u32 XFastv2_layer4_hls_IsDone(XFastv2_layer4_hls *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XFastv2_layer4_hls_ReadReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_AP_CTRL);
    return (Data >> 1) & 0x1;
}

u32 XFastv2_layer4_hls_IsIdle(XFastv2_layer4_hls *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XFastv2_layer4_hls_ReadReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_AP_CTRL);
    return (Data >> 2) & 0x1;
}

u32 XFastv2_layer4_hls_IsReady(XFastv2_layer4_hls *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XFastv2_layer4_hls_ReadReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_AP_CTRL);
    // check ap_start to see if the pcore is ready for next input
    return !(Data & 0x1);
}

void XFastv2_layer4_hls_EnableAutoRestart(XFastv2_layer4_hls *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XFastv2_layer4_hls_WriteReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_AP_CTRL, 0x80);
}

void XFastv2_layer4_hls_DisableAutoRestart(XFastv2_layer4_hls *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XFastv2_layer4_hls_WriteReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_AP_CTRL, 0);
}

u32 XFastv2_layer4_hls_Get_return(XFastv2_layer4_hls *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XFastv2_layer4_hls_ReadReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_AP_RETURN);
    return Data;
}
void XFastv2_layer4_hls_Set_image_r(XFastv2_layer4_hls *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XFastv2_layer4_hls_WriteReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_IMAGE_R_DATA, Data);
}

u32 XFastv2_layer4_hls_Get_image_r(XFastv2_layer4_hls *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XFastv2_layer4_hls_ReadReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_IMAGE_R_DATA);
    return Data;
}

void XFastv2_layer4_hls_Set_blurred(XFastv2_layer4_hls *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XFastv2_layer4_hls_WriteReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_BLURRED_DATA, Data);
}

u32 XFastv2_layer4_hls_Get_blurred(XFastv2_layer4_hls *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XFastv2_layer4_hls_ReadReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_BLURRED_DATA);
    return Data;
}

void XFastv2_layer4_hls_Set_resized(XFastv2_layer4_hls *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XFastv2_layer4_hls_WriteReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_RESIZED_DATA, Data);
}

u32 XFastv2_layer4_hls_Get_resized(XFastv2_layer4_hls *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XFastv2_layer4_hls_ReadReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_RESIZED_DATA);
    return Data;
}

void XFastv2_layer4_hls_Set_selected(XFastv2_layer4_hls *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XFastv2_layer4_hls_WriteReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_SELECTED_DATA, Data);
}

u32 XFastv2_layer4_hls_Get_selected(XFastv2_layer4_hls *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XFastv2_layer4_hls_ReadReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_SELECTED_DATA);
    return Data;
}

void XFastv2_layer4_hls_Set_result(XFastv2_layer4_hls *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XFastv2_layer4_hls_WriteReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_RESULT_DATA, Data);
}

u32 XFastv2_layer4_hls_Get_result(XFastv2_layer4_hls *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XFastv2_layer4_hls_ReadReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_RESULT_DATA);
    return Data;
}

void XFastv2_layer4_hls_Set_stats(XFastv2_layer4_hls *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XFastv2_layer4_hls_WriteReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_STATS_DATA, Data);
}

u32 XFastv2_layer4_hls_Get_stats(XFastv2_layer4_hls *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XFastv2_layer4_hls_ReadReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_STATS_DATA);
    return Data;
}

void XFastv2_layer4_hls_Set_level(XFastv2_layer4_hls *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XFastv2_layer4_hls_WriteReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_LEVEL_DATA, Data);
}

u32 XFastv2_layer4_hls_Get_level(XFastv2_layer4_hls *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XFastv2_layer4_hls_ReadReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_LEVEL_DATA);
    return Data;
}

void XFastv2_layer4_hls_Set_budget(XFastv2_layer4_hls *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XFastv2_layer4_hls_WriteReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_BUDGET_DATA, Data);
}

u32 XFastv2_layer4_hls_Get_budget(XFastv2_layer4_hls *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XFastv2_layer4_hls_ReadReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_BUDGET_DATA);
    return Data;
}

void XFastv2_layer4_hls_Set_high_threshold(XFastv2_layer4_hls *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XFastv2_layer4_hls_WriteReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_HIGH_THRESHOLD_DATA, Data);
}

u32 XFastv2_layer4_hls_Get_high_threshold(XFastv2_layer4_hls *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XFastv2_layer4_hls_ReadReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_HIGH_THRESHOLD_DATA);
    return Data;
}

void XFastv2_layer4_hls_Set_low_threshold(XFastv2_layer4_hls *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XFastv2_layer4_hls_WriteReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_LOW_THRESHOLD_DATA, Data);
}

u32 XFastv2_layer4_hls_Get_low_threshold(XFastv2_layer4_hls *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XFastv2_layer4_hls_ReadReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_LOW_THRESHOLD_DATA);
    return Data;
}

void XFastv2_layer4_hls_Set_dual_threshold(XFastv2_layer4_hls *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XFastv2_layer4_hls_WriteReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_DUAL_THRESHOLD_DATA, Data);
}

u32 XFastv2_layer4_hls_Get_dual_threshold(XFastv2_layer4_hls *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XFastv2_layer4_hls_ReadReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_DUAL_THRESHOLD_DATA);
    return Data;
}

void XFastv2_layer4_hls_Set_selected_capacity(XFastv2_layer4_hls *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XFastv2_layer4_hls_WriteReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_SELECTED_CAPACITY_DATA, Data);
}

u32 XFastv2_layer4_hls_Get_selected_capacity(XFastv2_layer4_hls *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XFastv2_layer4_hls_ReadReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_SELECTED_CAPACITY_DATA);
    return Data;
}

void XFastv2_layer4_hls_InterruptGlobalEnable(XFastv2_layer4_hls *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XFastv2_layer4_hls_WriteReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_GIE, 1);
}

void XFastv2_layer4_hls_InterruptGlobalDisable(XFastv2_layer4_hls *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XFastv2_layer4_hls_WriteReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_GIE, 0);
}

void XFastv2_layer4_hls_InterruptEnable(XFastv2_layer4_hls *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XFastv2_layer4_hls_ReadReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_IER);
    XFastv2_layer4_hls_WriteReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_IER, Register | Mask);
}

void XFastv2_layer4_hls_InterruptDisable(XFastv2_layer4_hls *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XFastv2_layer4_hls_ReadReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_IER);
    XFastv2_layer4_hls_WriteReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_IER, Register & (~Mask));
}

void XFastv2_layer4_hls_InterruptClear(XFastv2_layer4_hls *InstancePtr, u32 Mask) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XFastv2_layer4_hls_WriteReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_ISR, Mask);
}

u32 XFastv2_layer4_hls_InterruptGetEnabled(XFastv2_layer4_hls *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XFastv2_layer4_hls_ReadReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_IER);
}

u32 XFastv2_layer4_hls_InterruptGetStatus(XFastv2_layer4_hls *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XFastv2_layer4_hls_ReadReg(InstancePtr->Control_BaseAddress, XFASTV2_LAYER4_HLS_CONTROL_ADDR_ISR);
}

