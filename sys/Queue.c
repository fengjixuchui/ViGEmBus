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


#include "busenum.h"
#include "queue.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, Bus_EvtIoDefault)
#endif

//
// Responds to I/O control requests sent to the FDO.
// 
VOID Bus_EvtIoDeviceControl(
    IN WDFQUEUE Queue,
    IN WDFREQUEST Request,
    IN size_t OutputBufferLength,
    IN size_t InputBufferLength,
    IN ULONG IoControlCode
)
{
    NTSTATUS                    status = STATUS_INVALID_PARAMETER;
    WDFDEVICE                   Device;
    size_t                      length = 0;
    PXUSB_SUBMIT_REPORT         xusbSubmit = NULL;
    PXUSB_REQUEST_NOTIFICATION  xusbNotify = NULL;
    PDS4_SUBMIT_REPORT          ds4Submit = NULL;
    PDS4_REQUEST_NOTIFICATION   ds4Notify = NULL;
    PXGIP_SUBMIT_REPORT         xgipSubmit = NULL;
    PXGIP_SUBMIT_INTERRUPT      xgipInterrupt = NULL;
    PVIGEM_CHECK_VERSION        pCheckVersion = NULL;
    PXUSB_GET_USER_INDEX        pXusbGetUserIndex = NULL;

    Device = WdfIoQueueGetDevice(Queue);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_QUEUE, "%!FUNC! Entry (device: 0x%p)", Device);

    switch (IoControlCode)
    {
#pragma region IOCTL_VIGEM_CHECK_VERSION
    case IOCTL_VIGEM_CHECK_VERSION:

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_QUEUE,
            "IOCTL_VIGEM_CHECK_VERSION");

        status = WdfRequestRetrieveInputBuffer(Request, sizeof(VIGEM_CHECK_VERSION), (PVOID)&pCheckVersion, &length);

        if (!NT_SUCCESS(status) || length != sizeof(VIGEM_CHECK_VERSION))
        {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        status = (pCheckVersion->Version == VIGEM_COMMON_VERSION) ? STATUS_SUCCESS : STATUS_NOT_SUPPORTED;

        TraceEvents(TRACE_LEVEL_VERBOSE,
            TRACE_QUEUE,
            "Requested version: 0x%04X, compiled version: 0x%04X",
            pCheckVersion->Version, VIGEM_COMMON_VERSION);

        break;
#pragma endregion 

#pragma region IOCTL_VIGEM_PLUGIN_TARGET
    case IOCTL_VIGEM_PLUGIN_TARGET:

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_QUEUE,
            "IOCTL_VIGEM_PLUGIN_TARGET");

        status = Bus_PlugInDevice(Device, Request, FALSE, &length);

        break;
#pragma endregion 

#pragma region IOCTL_VIGEM_UNPLUG_TARGET
    case IOCTL_VIGEM_UNPLUG_TARGET:

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_QUEUE,
            "IOCTL_VIGEM_UNPLUG_TARGET");

        status = Bus_UnPlugDevice(Device, Request, FALSE, &length);

        break;
#pragma endregion 

#pragma region IOCTL_XUSB_SUBMIT_REPORT
    case IOCTL_XUSB_SUBMIT_REPORT:

        TraceEvents(TRACE_LEVEL_VERBOSE,
            TRACE_QUEUE,
            "IOCTL_XUSB_SUBMIT_REPORT");

        status = WdfRequestRetrieveInputBuffer(Request, sizeof(XUSB_SUBMIT_REPORT), (PVOID)&xusbSubmit, &length);

        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_QUEUE,
                "WdfRequestRetrieveInputBuffer failed with status %!STATUS!",
                status);
            break;
        }

        if ((sizeof(XUSB_SUBMIT_REPORT) == xusbSubmit->Size) && (length == InputBufferLength))
        {
            // This request only supports a single PDO at a time
            if (xusbSubmit->SerialNo == 0)
            {
                TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_QUEUE,
                    "Invalid serial 0 submitted");

                status = STATUS_INVALID_PARAMETER;
                break;
            }

            status = Bus_XusbSubmitReport(Device, xusbSubmit->SerialNo, xusbSubmit, FALSE);
        }

        break;
#pragma endregion 

#pragma region IOCTL_XUSB_REQUEST_NOTIFICATION
    case IOCTL_XUSB_REQUEST_NOTIFICATION:

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_QUEUE,
            "IOCTL_XUSB_REQUEST_NOTIFICATION");

        // Don't accept the request if the output buffer can't hold the results
        if (OutputBufferLength < sizeof(XUSB_REQUEST_NOTIFICATION))
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_QUEUE,
                "Output buffer %d too small, require at least %d",
                (int)OutputBufferLength, (int)sizeof(XUSB_REQUEST_NOTIFICATION));
            break;
        }

        status = WdfRequestRetrieveInputBuffer(Request, sizeof(XUSB_REQUEST_NOTIFICATION), (PVOID)&xusbNotify, &length);

        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_QUEUE,
                "WdfRequestRetrieveInputBuffer failed with status %!STATUS!",
                status);
            break;
        }

        if ((sizeof(XUSB_REQUEST_NOTIFICATION) == xusbNotify->Size) && (length == InputBufferLength))
        {
            // This request only supports a single PDO at a time
            if (xusbNotify->SerialNo == 0)
            {
                TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_QUEUE,
                    "Invalid serial 0 submitted");

                status = STATUS_INVALID_PARAMETER;
                break;
            }

            status = Bus_QueueNotification(Device, xusbNotify->SerialNo, Request);
        }

        break;
#pragma endregion 

#pragma region IOCTL_DS4_SUBMIT_REPORT
    case IOCTL_DS4_SUBMIT_REPORT:

        TraceEvents(TRACE_LEVEL_VERBOSE,
            TRACE_QUEUE,
            "IOCTL_DS4_SUBMIT_REPORT");

        status = WdfRequestRetrieveInputBuffer(Request, sizeof(DS4_SUBMIT_REPORT), (PVOID)&ds4Submit, &length);

        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_QUEUE,
                "WdfRequestRetrieveInputBuffer failed with status %!STATUS!",
                status);
            break;
        }

        if ((sizeof(DS4_SUBMIT_REPORT) == ds4Submit->Size) && (length == InputBufferLength))
        {
            // This request only supports a single PDO at a time
            if (ds4Submit->SerialNo == 0)
            {
                TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_QUEUE,
                    "Invalid serial 0 submitted");

                status = STATUS_INVALID_PARAMETER;
                break;
            }

            status = Bus_Ds4SubmitReport(Device, ds4Submit->SerialNo, ds4Submit, FALSE);
        }

        break;
#pragma endregion 

#pragma region IOCTL_DS4_REQUEST_NOTIFICATION
    case IOCTL_DS4_REQUEST_NOTIFICATION:

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_QUEUE,
            "IOCTL_DS4_REQUEST_NOTIFICATION");

        // Don't accept the request if the output buffer can't hold the results
        if (OutputBufferLength < sizeof(DS4_REQUEST_NOTIFICATION))
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_QUEUE,
                "Output buffer %d too small, require at least %d",
                (int)OutputBufferLength, (int)sizeof(DS4_REQUEST_NOTIFICATION));
            break;
        }

        status = WdfRequestRetrieveInputBuffer(Request, sizeof(DS4_REQUEST_NOTIFICATION), (PVOID)&ds4Notify, &length);

        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_QUEUE,
                "WdfRequestRetrieveInputBuffer failed with status %!STATUS!",
                status);
            break;
        }

        if ((sizeof(DS4_REQUEST_NOTIFICATION) == ds4Notify->Size) && (length == InputBufferLength))
        {
            // This request only supports a single PDO at a time
            if (ds4Notify->SerialNo == 0)
            {
                TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_QUEUE,
                    "Invalid serial 0 submitted");

                status = STATUS_INVALID_PARAMETER;
                break;
            }

            status = Bus_QueueNotification(Device, ds4Notify->SerialNo, Request);
        }

        break;
#pragma endregion 

#pragma region IOCTL_XGIP_SUBMIT_REPORT
    case IOCTL_XGIP_SUBMIT_REPORT:

        TraceEvents(TRACE_LEVEL_VERBOSE,
            TRACE_QUEUE,
            "IOCTL_XGIP_SUBMIT_REPORT");

        status = WdfRequestRetrieveInputBuffer(Request, sizeof(XGIP_SUBMIT_REPORT), (PVOID)&xgipSubmit, &length);

        if (!NT_SUCCESS(status))
        {
            KdPrint((DRIVERNAME "WdfRequestRetrieveInputBuffer failed 0x%x\n", status));
            break;
        }

        if ((sizeof(XGIP_SUBMIT_REPORT) == xgipSubmit->Size) && (length == InputBufferLength))
        {
            // This request only supports a single PDO at a time
            if (xgipSubmit->SerialNo == 0)
            {
                status = STATUS_INVALID_PARAMETER;
                break;
            }

            status = Bus_XgipSubmitReport(Device, xgipSubmit->SerialNo, xgipSubmit, FALSE);
        }

        break;
#pragma endregion 

#pragma region IOCTL_XGIP_SUBMIT_INTERRUPT
    case IOCTL_XGIP_SUBMIT_INTERRUPT:

        KdPrint((DRIVERNAME "IOCTL_XGIP_SUBMIT_INTERRUPT\n"));

        status = WdfRequestRetrieveInputBuffer(Request, sizeof(XGIP_SUBMIT_INTERRUPT), (PVOID)&xgipInterrupt, &length);

        if (!NT_SUCCESS(status))
        {
            KdPrint((DRIVERNAME "WdfRequestRetrieveInputBuffer failed 0x%x\n", status));
            break;
        }

        if ((sizeof(XGIP_SUBMIT_INTERRUPT) == xgipInterrupt->Size) && (length == InputBufferLength))
        {
            // This request only supports a single PDO at a time
            if (xgipInterrupt->SerialNo == 0)
            {
                status = STATUS_INVALID_PARAMETER;
                break;
            }

            status = Bus_XgipSubmitInterrupt(Device, xgipSubmit->SerialNo, xgipInterrupt, FALSE);
        }

        break;
#pragma endregion 

#pragma region IOCTL_XUSB_GET_USER_INDEX
    case IOCTL_XUSB_GET_USER_INDEX:

        KdPrint((DRIVERNAME "IOCTL_XUSB_GET_USER_INDEX"));

        // Don't accept the request if the output buffer can't hold the results
        if (OutputBufferLength < sizeof(XUSB_GET_USER_INDEX))
        {
            KdPrint((DRIVERNAME "IOCTL_XUSB_GET_USER_INDEX: output buffer too small: %ul\n", OutputBufferLength));
            break;
        }

        status = WdfRequestRetrieveInputBuffer(
            Request, 
            sizeof(XUSB_GET_USER_INDEX), 
            (PVOID)&pXusbGetUserIndex, 
            &length);

        if (!NT_SUCCESS(status))
        {
            KdPrint((DRIVERNAME "WdfRequestRetrieveInputBuffer failed 0x%x\n", status));
            break;
        }

        if ((sizeof(XUSB_GET_USER_INDEX) == pXusbGetUserIndex->Size) && (length == InputBufferLength))
        {
            // This request only supports a single PDO at a time
            if (pXusbGetUserIndex->SerialNo == 0)
            {
                status = STATUS_INVALID_PARAMETER;
                break;
            }

            status = Xusb_GetUserIndex(Device, pXusbGetUserIndex);
        }

        break;
#pragma endregion

    default:

        TraceEvents(TRACE_LEVEL_WARNING,
            TRACE_QUEUE,
            "Unknown I/O control code 0x%X", IoControlCode);

        break; // default status is STATUS_INVALID_PARAMETER
    }

    if (status != STATUS_PENDING)
    {
        WdfRequestCompleteWithInformation(Request, status, length);
    }
    
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_QUEUE, "%!FUNC! Exit with status %!STATUS!", status);
}

//
// Gets called upon driver-to-driver communication.
// 
// TODO: incomplete and unused currently
// 
VOID Bus_EvtIoInternalDeviceControl(
    _In_ WDFQUEUE   Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t     OutputBufferLength,
    _In_ size_t     InputBufferLength,
    _In_ ULONG      IoControlCode
)
{
    NTSTATUS    status = STATUS_INVALID_PARAMETER;
    WDFDEVICE   Device;
    size_t      length = 0;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    Device = WdfIoQueueGetDevice(Queue);

    KdPrint((DRIVERNAME "Bus_EvtIoInternalDeviceControl: 0x%p\n", Device));

    switch (IoControlCode)
    {
    case IOCTL_VIGEM_PLUGIN_TARGET:

        KdPrint((DRIVERNAME "IOCTL_VIGEM_PLUGIN_TARGET\n"));

        status = Bus_PlugInDevice(Device, Request, TRUE, &length);

        break;

    case IOCTL_VIGEM_UNPLUG_TARGET:

        KdPrint((DRIVERNAME "IOCTL_VIGEM_UNPLUG_TARGET\n"));

        status = Bus_UnPlugDevice(Device, Request, TRUE, &length);

        break;
    }

    if (status != STATUS_PENDING)
    {
        WdfRequestCompleteWithInformation(Request, status, length);
    }
}

//
// Catches unsupported requests.
// 
VOID Bus_EvtIoDefault(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request
)
{
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(Request);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "%!FUNC! Entry");

    WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "%!FUNC! Exit");
}