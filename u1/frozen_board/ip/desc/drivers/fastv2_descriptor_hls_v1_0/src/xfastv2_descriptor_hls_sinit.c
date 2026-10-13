// ==============================================================
// File generated on Tue Oct 13 18:46:32 +0800 2026
// Vivado(TM) HLS - High-Level Synthesis from C, C++ and SystemC v2018.3 (64-bit)
// SW Build 2405991 on Thu Dec  6 23:38:27 MST 2018
// IP Build 2404404 on Fri Dec  7 01:43:56 MST 2018
// Copyright 1986-2018 Xilinx, Inc. All Rights Reserved.
// ==============================================================
#ifndef __linux__

#include "xstatus.h"
#include "xparameters.h"
#include "xfastv2_descriptor_hls.h"

extern XFastv2_descriptor_hls_Config XFastv2_descriptor_hls_ConfigTable[];

XFastv2_descriptor_hls_Config *XFastv2_descriptor_hls_LookupConfig(u16 DeviceId) {
	XFastv2_descriptor_hls_Config *ConfigPtr = NULL;

	int Index;

	for (Index = 0; Index < XPAR_XFASTV2_DESCRIPTOR_HLS_NUM_INSTANCES; Index++) {
		if (XFastv2_descriptor_hls_ConfigTable[Index].DeviceId == DeviceId) {
			ConfigPtr = &XFastv2_descriptor_hls_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XFastv2_descriptor_hls_Initialize(XFastv2_descriptor_hls *InstancePtr, u16 DeviceId) {
	XFastv2_descriptor_hls_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XFastv2_descriptor_hls_LookupConfig(DeviceId);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XFastv2_descriptor_hls_CfgInitialize(InstancePtr, ConfigPtr);
}

#endif

