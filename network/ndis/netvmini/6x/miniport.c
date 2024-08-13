/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

   Miniport.C

Abstract:

    The purpose of this sample is to illustrate functionality of a deserialized
    NDIS miniport driver without requiring a physical network adapter. This
    sample is based on E100BEX sample present in the DDK. It is basically a
    simplified version of E100bex driver. The driver can be installed either
    manually using Add Hardware wizard as a root enumerated virtual miniport
    driver or on a virtual bus (like toaster bus). Since the driver does not
    interact with any hardware, it makes it very easy to understand the miniport
    interface and the usage of various NDIS functions without the clutter of
    hardware specific code normally found in a fully functional driver.

    This sample provides an example of minimal driver intended for education
    purposes. Neither the driver or its sample test programs are intended
    for use in a production environment.

Revision History:

Notes:

Help:
    如果光标在某个函数或宏上，按下F1键会自动打开相关的 MicroSoft 官方文档页面。

--*/
#include "netvmin6.h"
#include "miniport.tmh"

NDIS_STATUS
DriverEntry(
    _In_  PVOID DriverObject,
    _In_  PVOID RegistryPath);

// 标记负责初始化驱动程序的 DriverEntry 函数，使其仅在初始化期间运行一次。
#pragma NDIS_INIT_FUNCTION(DriverEntry)
// 标记负责卸载驱动程序的 DriverUnload 函数，使其在分页内存中运行。
#pragma NDIS_PAGEABLE_FUNCTION(DriverUnload)


MP_GLOBAL       GlobalData;
NDIS_HANDLE     NdisDriverHandle;

static
NDIS_STATUS
InitializeAdapterListLock();

static
VOID
FreeAdapterListLock();

NDIS_STATUS
DriverEntry(
    _In_  PVOID DriverObject,
    _In_  PVOID RegistryPath)
/*++
Routine Description:

    In the context of its DriverEntry function, a miniport driver associates
    itself with NDIS, specifies the NDIS version that it is using, and
    registers its entry points(与 NDIS 关联、指定 NDIS 版本、注册入口点).


Arguments:
    PVOID DriverObject - pointer to the driver object(指向驱动对象的指针).
    PVOID RegistryPath - pointer to the driver registry path(指向驱动注册表路径的指针).

    Return Value:

    NTSTATUS code

--*/
{
    NDIS_STATUS Status;
    // NDIS_MINIPORT_DRIVER_CHARACTERISTICS structure to define its miniport driver characteristics, 
    // including the entry points for its MiniportXxx functions.
    NDIS_MINIPORT_DRIVER_CHARACTERISTICS MPChar;

    // The WPP_INIT_TRACING macro registers the provider GUID and initializes the structures 
    // that are needed for software tracing in a kernel-mode driver.
    WPP_INIT_TRACING(DriverObject,RegistryPath);

    DEBUGP(MP_TRACE, "---> DriverEntry built on "__DATE__" at "__TIME__ "\n");
    PAGED_CODE();

    do
    {
        //
        // Initialize any driver-global variables here(初始化所有驱动全局变量).
        //

        // NdisZeroMemory(Destination, Length): fills a block of memory with zeros.
        NdisZeroMemory(&GlobalData, sizeof(GlobalData));

        //
        // The ApaterList in the GlobalData structure is used to track multiple
        // adapters controlled by this miniport(用于追踪由这个 miniport 控制的多个适配器).
        //
        NdisInitializeListHead(&GlobalData.AdapterList);


        //
        // The FrameDataLookaside list is used to help emulate an Ethernet hub(用于帮助模拟以太网集线器).
        //  NdisInitializeNPagedLookasideList() initializes a lookaside list.
        NdisInitializeNPagedLookasideList(
                &GlobalData.FrameDataLookaside,
                HWFrameAllocate,
                HWFrameFree,
                0,  // Reserved for system use
                sizeof(FRAME),
                NIC_TAG_FRAME,
                0); // Reserved for system use
        /*
            |= 操作可以将 GlobalData.Flags 的某些特定位置1，达到高效管理和状态跟踪的目的
        */
        // #define fGLOBAL_LOOKASIDE_INITIALIZED 0x0002
        GlobalData.Flags |= fGLOBAL_LOOKASIDE_INITIALIZED;

        //
        // Fill in the Miniport characteristics structure with the version numbers
        // and the entry points for driver-supplied MiniportXxx(填充注册所需的数据结构)
        //

        NdisZeroMemory(&MPChar, sizeof(MPChar));


#if (NDIS_SUPPORT_NDIS680)
        {C_ASSERT(sizeof(MPChar) >= NDIS_SIZEOF_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_3); }
        MPChar.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_DRIVER_CHARACTERISTICS;
        MPChar.Header.Size = NDIS_SIZEOF_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_3;
        MPChar.Header.Revision = NDIS_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_3;
#elif (NDIS_SUPPORT_NDIS620)
        {C_ASSERT(sizeof(MPChar) >= NDIS_SIZEOF_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_2); }
        MPChar.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_DRIVER_CHARACTERISTICS;
        MPChar.Header.Size = NDIS_SIZEOF_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_2;
        MPChar.Header.Revision = NDIS_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_2;
#elif (NDIS_SUPPORT_NDIS6)
        {C_ASSERT(sizeof(MPChar) >= NDIS_SIZEOF_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_1);}
        MPChar.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_DRIVER_CHARACTERISTICS;
        MPChar.Header.Size = NDIS_SIZEOF_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_1;
        MPChar.Header.Revision = NDIS_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_1;
#endif // NDIS MINIPORT VERSION

        MPChar.MajorNdisVersion = MP_NDIS_MAJOR_VERSION;
        MPChar.MinorNdisVersion = MP_NDIS_MINOR_VERSION;

        MPChar.MajorDriverVersion = NIC_MAJOR_DRIVER_VERSION;
        MPChar.MinorDriverVersion = NIC_MINOR_DRIVER_VERISON;

        MPChar.Flags = 0;

        MPChar.SetOptionsHandler = MPSetOptions; // Optional
        MPChar.InitializeHandlerEx = MPInitializeEx;
        MPChar.HaltHandlerEx = MPHaltEx;
        MPChar.UnloadHandler = DriverUnload;
        MPChar.PauseHandler = MPPause;
        MPChar.RestartHandler = MPRestart;
        MPChar.OidRequestHandler = MPOidRequest;
        MPChar.SendNetBufferListsHandler = MPSendNetBufferLists;
        MPChar.ReturnNetBufferListsHandler = MPReturnNetBufferLists;
        MPChar.CancelSendHandler = MPCancelSend;
        MPChar.CheckForHangHandlerEx = MPCheckForHangEx;
        MPChar.ResetHandlerEx = MPResetEx;
        MPChar.DevicePnPEventNotifyHandler = MPDevicePnpEventNotify;
        MPChar.ShutdownHandlerEx = MPShutdownEx;
        MPChar.CancelOidRequestHandler = MPCancelOidRequest;
#if (NDIS_SUPPORT_NDIS680)
        MPChar.SynchronousOidRequestHandler = MPSynchronousOidRequest;
#endif

        //
        // Associate the miniport driver with NDIS by calling the
        // NdisMRegisterMiniportDriver. This function returns an NdisDriverHandle.
        // The miniport driver must retain this handle but it should never attempt
        // to access or interpret this handle.
        //
        // By calling NdisMRegisterMiniportDriver, the driver indicates that it
        // is ready for NDIS to call the driver's MiniportSetOptions and
        // MiniportInitializeEx handlers.
        // 
        // Miniport Driver/DriverEntry 通过调用 NdisMRegisterMiniportDriver 
        // 向 NDIS 注册一组 MiniportXxx 函数
        //
        DEBUGP(MP_LOUD, "Calling NdisMRegisterMiniportDriver...\n");
        NDIS_DECLARE_MINIPORT_DRIVER_CONTEXT(MP_GLOBAL);
        Status = NdisMRegisterMiniportDriver(
                DriverObject,
                RegistryPath,
                &GlobalData,
                &MPChar,
                &NdisDriverHandle); // 对于输出参数 NdisDriverHandle, 必须传入其指针以便在函数内修改其内容
        if (NDIS_STATUS_SUCCESS != Status)
        {
            DEBUGP(MP_ERROR, "NdisMRegisterMiniportDriver failed: %d\n", Status);
            DriverUnload(DriverObject);
            Status = NDIS_STATUS_FAILURE;
            break; // 跳出 while 循环
        }
        // #define fGLOBAL_MINIPORT_REGISTERED   0x0004
        GlobalData.Flags |= fGLOBAL_MINIPORT_REGISTERED;

        //
        // The AdapterListLock protects the AdapterList
        //
        Status = InitializeAdapterListLock();
        if (Status != NDIS_STATUS_SUCCESS)
        {
            DEBUGP(MP_ERROR, "InitializeAdapterListLock failed 0x%08x", Status);
            DriverUnload(DriverObject);
            Status = NDIS_STATUS_FAILURE;
            break;
        }
        // #define fGLOBAL_LOCK_ALLOCATED        0x0001
        GlobalData.Flags |= fGLOBAL_LOCK_ALLOCATED;
    }while(FALSE);

    DEBUGP(MP_TRACE, "<--- DriverEntry Status 0x%08x\n", Status);
    return Status;
}


VOID
DriverUnload(
    _In_  PDRIVER_OBJECT  DriverObject)
/*++

Routine Description:

    The unload handler is called during driver unload to free up resources
    acquired in DriverEntry. This handler is registered in DriverEntry through
    NdisMRegisterMiniportDriver. Note that an unload handler differs from
    a MiniportHalt function in that this unload handler releases resources that
    are global to the driver, while the halt handler releases resource for a
    particular adapter(DriverUnload 函数释放对于驱动的全局资源).

    Runs at IRQL = PASSIVE_LEVEL.

Arguments:

    DriverObject        Not used

Return Value:

    None.

--*/
{
    UNREFERENCED_PARAMETER(DriverObject);

    DEBUGP(MP_TRACE, "---> DriverUnload\n");
    PAGED_CODE();


    //
    // Clean up all globals that were allocated in DriverEntry
    //


    ASSERT(IsListEmpty(&GlobalData.AdapterList));

    /*
        下面连续三个分支语句通过判断特定位的状态来决定是否执行相关操作
    */
    if (GlobalData.Flags & fGLOBAL_MINIPORT_REGISTERED)
    {
        //
        // Since DriverEntry has successfully called NdisMRegisterMiniportDriver,
        // NdisMDeregisterMiniportDriver must be called to release NDIS's per-driver
        // resources.
        //
        DEBUGP(MP_LOUD, "Calling NdisMDeregisterMiniportDriver...\n");
        NdisMDeregisterMiniportDriver(NdisDriverHandle);
    }

    if (GlobalData.Flags & fGLOBAL_LOOKASIDE_INITIALIZED)
    {
        NdisDeleteNPagedLookasideList(&GlobalData.FrameDataLookaside);
    }

    if (GlobalData.Flags & fGLOBAL_LOCK_ALLOCATED)
    {
        FreeAdapterListLock();
    }

    WPP_CLEANUP(DriverObject->DeviceObject);

    DEBUGP(MP_TRACE, "<--- DriverUnload\n");
}


NDIS_STATUS
MPSetOptions(
    _In_  NDIS_HANDLE  DriverHandle,
    _In_  NDIS_HANDLE  DriverContext)
/*++
Routine Description:

    The MiniportSetOptions function registers optional handlers.  For each
    optional handler that should be registered, this function makes a call
    to NdisSetOptionalHandlers(注册可选的 Handler).

    MiniportSetOptions runs at IRQL = PASSIVE_LEVEL.

Arguments:

    DriverContext  The context handle

Return Value:

    NDIS_STATUS_xxx code

--*/
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    PMP_GLOBAL Global = (PMP_GLOBAL)DriverContext;

    DEBUGP(MP_TRACE, "---> MPSetOptions\n");
    // 标记未使用的参数，避免编译器发出警告
    UNREFERENCED_PARAMETER(DriverHandle);
    UNREFERENCED_PARAMETER(Global);


    //
    // Set any optional handlers by filling out the appropriate struct and
    // calling NdisSetOptionalHandlers here.
    //


    DEBUGP(MP_TRACE, "<--- MPSetOptions Status = 0x%08x\n", Status);
    return Status;
}

// 检查给定的 Adapter 是否已经连接到 GlobalData.AdapterList 中
BOOLEAN
MPIsAdapterAttached(
    _In_ PMP_ADAPTER Adapter)
{
    PLIST_ENTRY CurrentEntry = NULL;
    for (
            CurrentEntry = GlobalData.AdapterList.Flink;
            CurrentEntry != &GlobalData.AdapterList;
            CurrentEntry = CurrentEntry->Flink)
    {
        if(CurrentEntry == &Adapter->List)
        {
            return TRUE; // Adapter 已连接
        }
    }

    return FALSE;
}

// 将给定的 Adapter 链接到 GlobalData.AdapterList 中
VOID
MPAttachAdapter(
    _In_  PMP_ADAPTER Adapter)
{
    MP_LOCK_STATE LockState;    

    DEBUGP(MP_TRACE, "[%p] ---> MPAttachAdapter\n", Adapter);

    LOCK_ADAPTER_LIST_FOR_WRITE(&LockState, 0);

    if(!MPIsAdapterAttached(Adapter))
    {
        // 如果给定的 Adapter 不在 GlobalData.AdapterList 中，就执行环形链表的插入操作
        InsertTailList(&GlobalData.AdapterList, &Adapter->List);
    }

    UNLOCK_ADAPTER_LIST(&LockState);

    DEBUGP(MP_TRACE, "[%p] <--- MPAttachAdapter\n", Adapter);
}

// 将给定的 Adapter 从 GlobalData.AdapterList 中移除
VOID
MPDetachAdapter(
    _In_  PMP_ADAPTER Adapter)
{
    MP_LOCK_STATE LockState;
    DEBUGP(MP_TRACE, "[%p] ---> MPDetachAdapter\n", Adapter);

    LOCK_ADAPTER_LIST_FOR_WRITE(&LockState, 0);

    if(MPIsAdapterAttached(Adapter))
    {
        // 如果给定的 Adapter 在 GlobalData.AdapterList 中，就执行环形链表的删除操作
        RemoveEntryList(&Adapter->List);
    }

    UNLOCK_ADAPTER_LIST(&LockState);

    DEBUGP(MP_TRACE, "[%p] <--- MPDetachAdapter\n", Adapter);
}


#if (NDIS_SUPPORT_NDIS620)

NDIS_STATUS
InitializeAdapterListLock()
{
    GlobalData.Lock = NdisAllocateRWLock(NdisDriverHandle);
    if (!GlobalData.Lock)
    {
        return NDIS_STATUS_RESOURCES;
    }

    return NDIS_STATUS_SUCCESS;
}

VOID
FreeAdapterListLock()
{
    ASSERT(GlobalData.Lock);
    NdisFreeRWLock(GlobalData.Lock);
    GlobalData.Lock = NULL;
}

#elif (NDIS_SUPPORT_NDIS6)

NDIS_STATUS
InitializeAdapterListLock()
{
    NdisInitializeReadWriteLock(&GlobalData.Lock);
    return NDIS_STATUS_SUCCESS;
}

VOID
FreeAdapterListLock()
{
    // No action needed to clean up a NDIS 6.0 RW lock.
}

#endif



#ifndef DBG

VOID
DbgPrintOidName(
    _In_  NDIS_OID  Oid)
{
    UNREFERENCED_PARAMETER(Oid);
}

VOID
DbgPrintAddress(
    _In_reads_bytes_(NIC_MACADDR_SIZE) PUCHAR Address)
{
    UNREFERENCED_PARAMETER(Address);
}


#else // DBG

VOID
DbgPrintOidName(
    _In_  NDIS_OID  Oid)
{
    PCHAR oidName = NULL;

    switch (Oid){

        #undef MAKECASE
        #define MAKECASE(oidx) case oidx: oidName = #oidx "\n"; break;

        /* Operational OIDs */
        MAKECASE(OID_GEN_SUPPORTED_LIST)
        MAKECASE(OID_GEN_HARDWARE_STATUS)
        MAKECASE(OID_GEN_MEDIA_SUPPORTED)
        MAKECASE(OID_GEN_MEDIA_IN_USE)
        MAKECASE(OID_GEN_MAXIMUM_LOOKAHEAD)
        MAKECASE(OID_GEN_MAXIMUM_FRAME_SIZE)
        MAKECASE(OID_GEN_LINK_SPEED)
        MAKECASE(OID_GEN_TRANSMIT_BUFFER_SPACE)
        MAKECASE(OID_GEN_RECEIVE_BUFFER_SPACE)
        MAKECASE(OID_GEN_TRANSMIT_BLOCK_SIZE)
        MAKECASE(OID_GEN_RECEIVE_BLOCK_SIZE)
        MAKECASE(OID_GEN_VENDOR_ID)
        MAKECASE(OID_GEN_VENDOR_DESCRIPTION)
        MAKECASE(OID_GEN_VENDOR_DRIVER_VERSION)
        MAKECASE(OID_GEN_CURRENT_PACKET_FILTER)
        MAKECASE(OID_GEN_CURRENT_LOOKAHEAD)
        MAKECASE(OID_GEN_DRIVER_VERSION)
        MAKECASE(OID_GEN_MAXIMUM_TOTAL_SIZE)
        MAKECASE(OID_GEN_PROTOCOL_OPTIONS)
        MAKECASE(OID_GEN_MAC_OPTIONS)
        MAKECASE(OID_GEN_MEDIA_CONNECT_STATUS)
        MAKECASE(OID_GEN_MAXIMUM_SEND_PACKETS)
        MAKECASE(OID_GEN_SUPPORTED_GUIDS)
        MAKECASE(OID_GEN_NETWORK_LAYER_ADDRESSES)
        MAKECASE(OID_GEN_TRANSPORT_HEADER_OFFSET)
        MAKECASE(OID_GEN_MEDIA_CAPABILITIES)
        MAKECASE(OID_GEN_PHYSICAL_MEDIUM)
        MAKECASE(OID_GEN_MACHINE_NAME)
        MAKECASE(OID_GEN_VLAN_ID)
        MAKECASE(OID_GEN_RNDIS_CONFIG_PARAMETER)

        /* Operational OIDs for NDIS 6.0 */
        MAKECASE(OID_GEN_MAX_LINK_SPEED)
        MAKECASE(OID_GEN_LINK_STATE)
        MAKECASE(OID_GEN_LINK_PARAMETERS)
        MAKECASE(OID_GEN_MINIPORT_RESTART_ATTRIBUTES)
        MAKECASE(OID_GEN_ENUMERATE_PORTS)
        MAKECASE(OID_GEN_PORT_STATE)
        MAKECASE(OID_GEN_PORT_AUTHENTICATION_PARAMETERS)
        MAKECASE(OID_GEN_INTERRUPT_MODERATION)
        MAKECASE(OID_GEN_PHYSICAL_MEDIUM_EX)

        /* Statistical OIDs */
        MAKECASE(OID_GEN_XMIT_OK)
        MAKECASE(OID_GEN_RCV_OK)
        MAKECASE(OID_GEN_XMIT_ERROR)
        MAKECASE(OID_GEN_RCV_ERROR)
        MAKECASE(OID_GEN_RCV_NO_BUFFER)
        MAKECASE(OID_GEN_DIRECTED_BYTES_XMIT)
        MAKECASE(OID_GEN_DIRECTED_FRAMES_XMIT)
        MAKECASE(OID_GEN_MULTICAST_BYTES_XMIT)
        MAKECASE(OID_GEN_MULTICAST_FRAMES_XMIT)
        MAKECASE(OID_GEN_BROADCAST_BYTES_XMIT)
        MAKECASE(OID_GEN_BROADCAST_FRAMES_XMIT)
        MAKECASE(OID_GEN_DIRECTED_BYTES_RCV)
        MAKECASE(OID_GEN_DIRECTED_FRAMES_RCV)
        MAKECASE(OID_GEN_MULTICAST_BYTES_RCV)
        MAKECASE(OID_GEN_MULTICAST_FRAMES_RCV)
        MAKECASE(OID_GEN_BROADCAST_BYTES_RCV)
        MAKECASE(OID_GEN_BROADCAST_FRAMES_RCV)
        MAKECASE(OID_GEN_RCV_CRC_ERROR)
        MAKECASE(OID_GEN_TRANSMIT_QUEUE_LENGTH)

        /* Statistical OIDs for NDIS 6.0 */
        MAKECASE(OID_GEN_STATISTICS)
        MAKECASE(OID_GEN_BYTES_RCV)
        MAKECASE(OID_GEN_BYTES_XMIT)
        MAKECASE(OID_GEN_RCV_DISCARDS)
        MAKECASE(OID_GEN_XMIT_DISCARDS)

        /* Misc OIDs */
        MAKECASE(OID_GEN_GET_TIME_CAPS)
        MAKECASE(OID_GEN_GET_NETCARD_TIME)
        MAKECASE(OID_GEN_NETCARD_LOAD)
        MAKECASE(OID_GEN_DEVICE_PROFILE)
        MAKECASE(OID_GEN_INIT_TIME_MS)
        MAKECASE(OID_GEN_RESET_COUNTS)
        MAKECASE(OID_GEN_MEDIA_SENSE_COUNTS)

        /* PnP power management operational OIDs */
        MAKECASE(OID_PNP_CAPABILITIES)
        MAKECASE(OID_PNP_SET_POWER)
        MAKECASE(OID_PNP_QUERY_POWER)
        MAKECASE(OID_PNP_ADD_WAKE_UP_PATTERN)
        MAKECASE(OID_PNP_REMOVE_WAKE_UP_PATTERN)
        MAKECASE(OID_PNP_ENABLE_WAKE_UP)
        MAKECASE(OID_PNP_WAKE_UP_PATTERN_LIST)

        /* PnP power management statistical OIDs */
        MAKECASE(OID_PNP_WAKE_UP_ERROR)
        MAKECASE(OID_PNP_WAKE_UP_OK)

        /* Ethernet operational OIDs */
        MAKECASE(OID_802_3_PERMANENT_ADDRESS)
        MAKECASE(OID_802_3_CURRENT_ADDRESS)
        MAKECASE(OID_802_3_MULTICAST_LIST)
        MAKECASE(OID_802_3_MAXIMUM_LIST_SIZE)
        MAKECASE(OID_802_3_MAC_OPTIONS)

        /* Ethernet operational OIDs for NDIS 6.0 */
        MAKECASE(OID_802_3_ADD_MULTICAST_ADDRESS)
        MAKECASE(OID_802_3_DELETE_MULTICAST_ADDRESS)

        /* Ethernet statistical OIDs */
        MAKECASE(OID_802_3_RCV_ERROR_ALIGNMENT)
        MAKECASE(OID_802_3_XMIT_ONE_COLLISION)
        MAKECASE(OID_802_3_XMIT_MORE_COLLISIONS)
        MAKECASE(OID_802_3_XMIT_DEFERRED)
        MAKECASE(OID_802_3_XMIT_MAX_COLLISIONS)
        MAKECASE(OID_802_3_RCV_OVERRUN)
        MAKECASE(OID_802_3_XMIT_UNDERRUN)
        MAKECASE(OID_802_3_XMIT_HEARTBEAT_FAILURE)
        MAKECASE(OID_802_3_XMIT_TIMES_CRS_LOST)
        MAKECASE(OID_802_3_XMIT_LATE_COLLISIONS)

        /*  TCP/IP OIDs */
        MAKECASE(OID_TCP_TASK_OFFLOAD)
        MAKECASE(OID_TCP_TASK_IPSEC_ADD_SA)
        MAKECASE(OID_TCP_TASK_IPSEC_DELETE_SA)
        MAKECASE(OID_TCP_SAN_SUPPORT)
        MAKECASE(OID_TCP_TASK_IPSEC_ADD_UDPESP_SA)
        MAKECASE(OID_TCP_TASK_IPSEC_DELETE_UDPESP_SA)
        MAKECASE(OID_TCP4_OFFLOAD_STATS)
        MAKECASE(OID_TCP6_OFFLOAD_STATS)
        MAKECASE(OID_IP4_OFFLOAD_STATS)
        MAKECASE(OID_IP6_OFFLOAD_STATS)

        /* TCP offload OIDs for NDIS 6 */
        MAKECASE(OID_TCP_OFFLOAD_CURRENT_CONFIG)
        MAKECASE(OID_TCP_OFFLOAD_PARAMETERS)
        MAKECASE(OID_TCP_OFFLOAD_HARDWARE_CAPABILITIES)
        MAKECASE(OID_TCP_CONNECTION_OFFLOAD_CURRENT_CONFIG)
        MAKECASE(OID_TCP_CONNECTION_OFFLOAD_HARDWARE_CAPABILITIES)
        MAKECASE(OID_OFFLOAD_ENCAPSULATION)

#if (NDIS_SUPPORT_NDIS620)
        /* VMQ OIDs for NDIS 6.20 */
        MAKECASE(OID_RECEIVE_FILTER_FREE_QUEUE)
        MAKECASE(OID_RECEIVE_FILTER_CLEAR_FILTER)
        MAKECASE(OID_RECEIVE_FILTER_ALLOCATE_QUEUE)
        MAKECASE(OID_RECEIVE_FILTER_QUEUE_ALLOCATION_COMPLETE)
        MAKECASE(OID_RECEIVE_FILTER_SET_FILTER)
#endif

#if (NDIS_SUPPORT_NDIS630)
        /* NDIS QoS OIDs for NDIS 6.30 */
        MAKECASE(OID_QOS_PARAMETERS)
#endif

#if (NDIS_SUPPORT_NDIS680)
        /* RSSv2 OIDS*/
        MAKECASE(OID_GEN_RSS_SET_INDIRECTION_TABLE_ENTRIES)
        MAKECASE(OID_GEN_RECEIVE_SCALE_PARAMETERS_V2)
#endif
    }

    if (oidName)
    {
        DEBUGP(MP_LOUD, "%s", oidName);
    }
    else
    {
        DEBUGP(MP_LOUD, "<** Unknown OID 0x%08x **>\n", Oid);
    }
}

VOID
DbgPrintAddress(
    _In_reads_bytes_(NIC_MACADDR_SIZE) PUCHAR Address)
{
    // If your MAC address has a different size, adjust the printf accordingly.
    {C_ASSERT(NIC_MACADDR_SIZE == 6);}

    DEBUGP(MP_LOUD, "%02x-%02x-%02x-%02x-%02x-%02x\n",
            Address[0], Address[1], Address[2],
            Address[3], Address[4], Address[5]);
}



#endif //DBG
