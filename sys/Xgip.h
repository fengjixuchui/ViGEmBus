/*
* Virtual Gamepad Emulation Framework - Windows kernel-mode bus driver
*
* MIT License
*
* Copyright (c) 2016-2019 Nefarius Software Solutions e.U. and Contributors
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/


// 
// For children emulating XGIP devices, the following dummy interfaces 
// have to be exposed by the PDO or else the child devices won't start
// 

// Below these interfaces are from wdmguid.h being used as dummies
// {70211B0E-0AFB-47DB-AFC1-410BF842497A} PNP_LOCATION_INTERFACE
// {B38290E5-3CD0-4F9D-9937-F5FE2B44D47A} D3COLD_SUPPORT_INTERFACE
// {2AEB0243-6A6E-486B-82FC-D815F6B97006} REENUMERATE_SELF_INTERFACE_STANDARD

// {DC7A8E51-49B3-4A3A-9E81-625205E7D729} Seems to be GUID_POWER_THREAD_INTERFACE
// Source: https://github.com/microsoft/Windows-Driver-Frameworks/blob/master/src/framework/shared/irphandlers/pnp/fxpkgpnp.cpp#L63
DEFINE_GUID(GUID_DEVINTERFACE_XGIP_GUID_POWER_THREAD_INTERFACE_DUMMY,
    0xDC7A8E51, 0x49B3, 0x4A3A, 0x9E, 0x81, 0x62, 0x52, 0x05, 0xE7, 0xD7, 0x29);

// {DEEE98EA-C0A1-42C3-9738-A04606C84E93}
DEFINE_GUID(GUID_DEVINTERFACE_XGIP_UNKNOWN_4,
    0xDEEE98EA, 0xC0A1, 0x42C3, 0x97, 0x38, 0xA0, 0x46, 0x06, 0xC8, 0x4E, 0x93);


#pragma once

#define XGIP_DESCRIPTOR_SIZE            0x0040
#define XGIP_CONFIGURATION_SIZE         0x88
#define XGIP_REPORT_SIZE                0x12
#define XGIP_SYS_INIT_PACKETS           0x0F
#define XGIP_SYS_INIT_PERIOD            0x32

typedef struct _XGIP_DEVICE_DATA
{
    UCHAR Report[XGIP_REPORT_SIZE];

    //
    // Queue for incoming interrupt transfer
    //
    WDFQUEUE PendingUsbInRequests;

    //
    // Queue for inverted calls
    //
    WDFQUEUE PendingNotificationRequests;

    WDFCOLLECTION XboxgipSysInitCollection;

    BOOLEAN XboxgipSysInitReady;

    WDFTIMER XboxgipSysInitTimer;
} XGIP_DEVICE_DATA, *PXGIP_DEVICE_DATA;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(XGIP_DEVICE_DATA, XgipGetData)


NTSTATUS
Bus_XgipSubmitInterrupt(
    WDFDEVICE Device,
    ULONG SerialNo,
    PXGIP_SUBMIT_INTERRUPT Report,
    _In_ BOOLEAN FromInterface
);

//
// XGIP-specific functions
// 
NTSTATUS Xgip_PreparePdo(
    PWDFDEVICE_INIT DeviceInit,
    PUNICODE_STRING DeviceId,
    PUNICODE_STRING DeviceDescription
);
NTSTATUS Xgip_PrepareHardware(WDFDEVICE Device);
NTSTATUS Xgip_AssignPdoContext(WDFDEVICE Device);
VOID Xgip_GetConfigurationDescriptorType(PUCHAR Buffer, ULONG Length);
VOID Xgip_GetDeviceDescriptorType(PUSB_DEVICE_DESCRIPTOR pDescriptor, PPDO_DEVICE_DATA pCommon);
VOID Xgip_SelectConfiguration(PUSBD_INTERFACE_INFORMATION pInfo);

