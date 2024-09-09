/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

   Adapter.H

Abstract:

    This module contains structure definitons and function prototypes.

Revision History:

Notes:

--*/


//
// Utility macros
// -----------------------------------------------------------------------------
//

#define MP_SET_FLAG(_M, _F)             ((_M)->Flags |= (_F))
#define MP_CLEAR_FLAG(_M, _F)           ((_M)->Flags &= ~(_F))
#define MP_TEST_FLAG(_M, _F)            (((_M)->Flags & (_F)) != 0)
#define MP_TEST_FLAGS(_M, _F)           (((_M)->Flags & (_F)) == (_F))


//
// return true if the adapter is not in the six states: reset, disconnected, halted, paused, low power, or pause in progress
// return false if the adapter is in the the six states: reset, disconnected, halted, paused, low power, or pause in progress
//
#define MP_IS_READY(_M)        (((_M)->Flags &                             \
                                 (fMP_DISCONNECTED                         \
                                    | fMP_RESET_IN_PROGRESS                \
                                    | fMP_ADAPTER_HALT_IN_PROGRESS         \
                                    | fMP_ADAPTER_PAUSE_IN_PROGRESS        \
                                    | fMP_ADAPTER_PAUSED                   \
                                    | fMP_ADAPTER_LOW_POWER                \
                                    )) == 0)

//
// Each receive DPC is tracked by this structure, kept in a list in the MP_ADAPTER structure.
// It tracks the DPC object, the processor affinity of the DPC, and which MP_ADAPTER_RECEIVE_BLOCK
// indexes the DPC should consume for receive NBLs.
//
typedef struct _MP_ADAPTER_RECEIVE_DPC
{
    LIST_ENTRY Entry;

    //
    // Kernel DPC used for recieve
    //
    KDPC Dpc; // 《Windows内核情景分析》P770

	//
	// Processor affinity of the DPC
	//
    USHORT ProcessorGroup;
    ULONG ProcessorNumber;

    //
    // Tracks which receive blocks need to be recieved on this DPC.
    //
    BOOLEAN RecvBlock[NIC_SUPPORTED_NUM_QUEUES]; // 表示每个接收队列的状态
    volatile LONG RecvBlockCount; // 跟踪当前被阻塞的接收队列的数量

    //
    // Sets up the maximum amount of NBLs that can be indicated by a single
    // receive block. This is initially NIC_MAX_RECVS_PER_INDICATE.
    //
    ULONG MaxNblCountPerIndicate;

    //
    // Work item used if we need to avoid DPC timeout(超时)
    //
    NDIS_HANDLE WorkItem;
    volatile LONG WorkItemQueued; // 表示 WorkItem 是否已队列

    //
    // Pointer back to owner Adapter structure (accesed within work item)
    //
    struct _MP_ADAPTER *Adapter;

} MP_ADAPTER_RECEIVE_DPC, * PMP_ADAPTER_RECEIVE_DPC;

//
// This structure is used to track pending receives on the adpater (consumed by receive DPCs).
// One receive block maintained for each VMQ queue (if enabled),
// otherwise a single structure is used to track receives on the adapter.
//
typedef struct DECLSPEC_CACHEALIGN _MP_ADAPTER_RECEIVE_BLOCK
{
    //
    // List of pending RCB blocks that need to be indicated up to NDIS
	// 需要向上传递至 NDIS 的待处理 RCB 块链表
    //
    LIST_ENTRY ReceiveList;
    NDIS_SPIN_LOCK ReceiveListLock;
    volatile LONG PendingReceives;
} MP_ADAPTER_RECEIVE_BLOCK, * PMP_ADAPTER_RECEIVE_BLOCK;

//
// Each adapter managed by this driver has a MP_ADAPTER struct.
// 被此 driver 管理的每个 adapter 都有一个 MP_ADAPTER 结构。
//
typedef struct _MP_ADAPTER
{
    LIST_ENTRY              List; // 连接多个适配器对象的链表

    //
    // Keep track of various device objects(追踪多种设备对象).
    //
    PDEVICE_OBJECT          Pdo; // Point to Physical Device Object
    PDEVICE_OBJECT          Fdo; // Point to Functional Device Object
    PDEVICE_OBJECT          NextDeviceObject; // 指向同一个驱动程序中的下一个设备对象
	// 因为一个驱动程序可以对应多个不同类型的设备对象，常见于过滤驱动层链

    NDIS_HANDLE             AdapterHandle; // NDIS 提供的适配器句柄，用于和 NDIS 接口进行交互


    //
    // Status flags(状态标志)
    //

#define fMP_RESET_IN_PROGRESS               0x00000001 // 重置正在进行
#define fMP_DISCONNECTED                    0x00000002 // 断开连接
#define fMP_ADAPTER_HALT_IN_PROGRESS        0x00000004 // 适配器停止正在进行
#define fMP_ADAPTER_PAUSE_IN_PROGRESS       0x00000010 // 适配器暂停正在进行
#define fMP_ADAPTER_PAUSED                  0x00000020 // 适配器已暂停
#define fMP_ADAPTER_SURPRISE_REMOVED        0x00000100 // 适配器被意外移除
#define fMP_ADAPTER_LOW_POWER               0x00000200 // 适配器处于低功率状态

    ULONG                   Flags; // 适配器的状态标志位，使用上述宏定义指示状态


    UCHAR                   PermanentAddress[NIC_MACADDR_SIZE]; // 永久MAC地址
    UCHAR                   CurrentAddress[NIC_MACADDR_SIZE]; // 当前MAC地址，可能因软件配置而改变

    //
    // Send tracking
    // -------------------------------------------------------------------------
    // TCB = Transmit Control Block
	//

    // Pool of unused TCBs
    PVOID                   TcbMemoryBlock; // 未使用的 TCB 内存池

    // List of unused TCBs (sliced out of TcbMemoryBlock)
    LIST_ENTRY              FreeTcbList;     // 未使用的 TCB 链表
    NDIS_SPIN_LOCK          FreeTcbListLock; // 用于保护未使用的 TCB 链表的自旋锁

    // List of net buffers to send that are waiting for a free TCB
    LIST_ENTRY              SendWaitList;     // 用于发送 等待释放的 TCB 的网络缓冲区链表
    NDIS_SPIN_LOCK          SendWaitListLock; // 用于保护发送 等待释放的 TCB 的网络缓冲区链表

    // List of TCBs that are being read by the NIC hardware
    LIST_ENTRY              BusyTcbList;     // 正在被 NIC 硬件读取的 TCB 链表
    NDIS_SPIN_LOCK          BusyTcbListLock; // 用于保护正在被 NIC 硬件读取的 TCB 链表的自旋锁

    // A DPC that simulates interrupt processing for send completes
    NDIS_HANDLE             SendCompleteTimer; // 用于模拟发送完成中断处理的 DPC

    //
    // Work item used if we need to avoid DPC timeout
    //
    NDIS_HANDLE             SendCompleteWorkItem;        // 避免DPC超时的工作项
    volatile LONG           SendCompleteWorkItemQueued;  // 指示发送完成工作项是否已排队
    volatile BOOLEAN        SendCompleteWorkItemRunning; // 指示发送完成工作项是否正在运行


    // Number of transmit NBLs from the protocol that we still have
    volatile LONG           nBusySend; // 当前正在处理的发送数目

    // Spin lock to ensure only one CPU is sending at a time
    KSPIN_LOCK              SendPathSpinLock; // 确保在同一时间只有一个 CPU 进行发送操作的自旋锁


    //
    // Receive tracking
    // -------------------------------------------------------------------------
    // RCB = Receive Control Block
	//

    // Pool of unused RCBs
    PVOID                   RcbMemoryBlock; // 未使用的 RCB 内存池

    // List of unused RCBs (sliced out of RcbMemoryBlock)
    LIST_ENTRY              FreeRcbList;     // 未使用的 RCB 链表
    NDIS_SPIN_LOCK          FreeRcbListLock; // 用于保护未使用的 RCB 链表的自旋锁

    NDIS_HANDLE             RecvNblPoolHandle; // 接收网络缓冲区池句柄

    //
    // List of receive DPCs allocated for various ProcessorAffinity values
    // (if only one needed, then only default is present in the list)
    //
    LIST_ENTRY               RecvDpcList;     // 接收 DPC 链表，用于处理不同的处理器亲和性
    NDIS_SPIN_LOCK           RecvDpcListLock; // 用于保护接收 DPC 链表的自旋锁
    PMP_ADAPTER_RECEIVE_DPC  DefaultRecvDpc;  // 默认接收 DPC

    //
    // Async pause and reset tracking
    // -------------------------------------------------------------------------
    //
    NDIS_HANDLE             AsyncBusyCheckTimer; // 异步检查忙碌状态的计时器
    LONG                    AsyncBusyCheckCount; // 异步检查忙碌状态的计数器


    //
    // NIC configuration
    // -------------------------------------------------------------------------
    //
    ULONG                   PacketFilter;    // 数据包过滤器配置
    ULONG                   ulLookahead;     // 预读取数据的长度
    ULONG64                 ulLinkSendSpeed; // 链路发送速度
    ULONG64                 ulLinkRecvSpeed; // 链路接收速度
    ULONG                   ulMaxBusySends;  // 最大繁忙发送数
    ULONG                   ulMaxBusyRecvs;  // 最大繁忙接收数

    // multicast list
    ULONG                   ulMCListSize;
    UCHAR                   MCList[NIC_MAX_MCAST_LIST][NIC_MACADDR_SIZE];


    //
    // Statistics
    // -------------------------------------------------------------------------
    //

    // Packet counts
    ULONG64                 FramesRxDirected;  // 接收定向帧数
    ULONG64                 FramesRxMulticast; // 接收多播帧数
    ULONG64                 FramesRxBroadcast; // 接收广播帧数
    ULONG64                 FramesTxDirected;  // 发送定向帧数
    ULONG64                 FramesTxMulticast; // 发送多播帧数
    ULONG64                 FramesTxBroadcast; // 发送广播帧数

    // Byte counts
    ULONG64                 BytesRxDirected;  // 接收定向字节数
    ULONG64                 BytesRxMulticast; // 接收多播字节数
    ULONG64                 BytesRxBroadcast; // 接收广播字节数
    ULONG64                 BytesTxDirected;  // 发送定向字节数
    ULONG64                 BytesTxMulticast; // 发送多播字节数
    ULONG64                 BytesTxBroadcast; // 发送广播字节数

    // Count of transmit errors
    ULONG                   TxAbortExcessCollisions;  // 因碰撞过多而终止的发送数
    ULONG                   TxLateCollisions;         // 晚碰撞数
    ULONG                   TxDmaUnderrun;            // DMA 下溢数
    ULONG                   TxLostCRS;                // 丢失 CRS 的次数
    ULONG                   TxOKButDeferred;          // 发送成功但延迟的次数
    ULONG                   OneRetry;                 // 重试了一次的次数
    ULONG                   MoreThanOneRetry;         // 重试了多次的次数
    ULONG                   TotalRetries;             // 总重试次数
    ULONG                   TransmitFailuresOther;    // 其他发送失败次数

    // Count of receive errors
    ULONG                   RxCrcErrors;        // 接收 CRC 错误数
    ULONG                   RxAlignmentErrors;  // 接收对齐错误数
    ULONG                   RxResourceErrors;   // 接收资源错误数
    ULONG                   RxDmaOverrunErrors; // 接收 DMA 溢出错误数
    ULONG                   RxCdtFrames;        // 接收 CDT 帧数
    ULONG                   RxRuntErrors;       // 接收短帧错误数

    //
    // Reference to the allocated root of MP_ADAPTER memory, which may not be cache aligned.
    // When allocating, the pointer returned will be UnalignedBuffer + an offset that will make
    // the base pointer cache aligned.
    //
    PVOID                   UnalignedAdapterBuffer;     // 未对齐的适配器内存缓冲区
    ULONG                   UnalignedAdapterBufferSize; // 未对齐的适配器内存缓冲区大小

    //
    // Tracks any pending NBLs for the particular receiver
    // (either 0 for non-VMQ scenarios, or the corresponding VMQ queue).
    // These are consumed by the receive DPCs(被接收 DPC 处理).
    //
    MP_ADAPTER_RECEIVE_BLOCK ReceiveBlock[NIC_SUPPORTED_NUM_QUEUES]; // 用于追踪每一个接收队列的 NBL

    //
    // An OID request that could not be fulfulled at the time of the call. These OIDs are serialized(串行化)
    // so we will not receive new queue management OID's until this one is complete.
    // Currently this is used only for freeing a Queue (which may still have outstanding references)
    //
    PNDIS_OID_REQUEST PendingRequest; // 待处理的 OID 请求

    NDIS_DEVICE_POWER_STATE CurrentPowerState; // 当前电源状态

#if (NDIS_SUPPORT_NDIS620)

    //
    // VMQ related data
    //
    MP_ADAPTER_VMQ_DATA     VMQData;

#endif

#if (NDIS_SUPPORT_NDIS630)

    //
    // NDIS QoS related data
    //
    MP_ADAPTER_QOS_DATA     QOSData;

#endif

#if (NDIS_SUPPORT_NDIS680)

    //
    // NDIS RSSv2-related data
    //
    MP_ADAPTER_RSS_DATA     RSSData;

#endif

} MP_ADAPTER, *PMP_ADAPTER;

#define MP_ADAPTER_FROM_CONTEXT(_ctx_) ((PMP_ADAPTER)(_ctx_))

PMP_ADAPTER_RECEIVE_DPC
NICAllocReceiveDpc(
    _In_ PMP_ADAPTER      Adapter,
    ULONG ProcessorNumber,
    USHORT  ProcessorGroup,
    _In_ _In_range_(0, NIC_SUPPORTED_NUM_QUEUES-1) ULONG BlockId);

VOID
NICReceiveDpcRemoveOwnership(
    _In_ PMP_ADAPTER_RECEIVE_DPC ReceiveDpc,
    _In_ _In_range_(0, NIC_SUPPORTED_NUM_QUEUES-1) ULONG BlockId);

PMP_ADAPTER_RECEIVE_DPC
NICGetDefaultReceiveDpc(
    _In_ PMP_ADAPTER      Adapter,
    _In_ _In_range_(0, NIC_SUPPORTED_NUM_QUEUES-1) ULONG BlockId);

NDIS_STATUS
NICAllocRCBData(
    _In_ PMP_ADAPTER Adapter,
    ULONG NumberOfRcbs,
    _Outptr_result_bytebuffer_(NumberOfRcbs * sizeof(RCB)) PVOID *RcbMemoryBlock,
    _Inout_ PLIST_ENTRY FreeRcbList,
    _Inout_ PNDIS_SPIN_LOCK FreeRcbListLock,
    _Inout_ PNDIS_HANDLE RecvNblPoolHandle);

NDIS_STATUS
NICInitializeReceiveBlock(
    _In_ PMP_ADAPTER Adapter,
    _In_ _In_range_(0, NIC_SUPPORTED_NUM_QUEUES-1) ULONG BlockId);

VOID
NICFlushReceiveBlock(
    _In_ PMP_ADAPTER Adapter,
    _In_ _In_range_(0, NIC_SUPPORTED_NUM_QUEUES-1) ULONG BlockId);

NDIS_STATUS
NICReferenceReceiveBlock(
    _In_ PMP_ADAPTER Adapter,
    _In_ _In_range_(0, NIC_SUPPORTED_NUM_QUEUES-1) ULONG BlockId);

VOID
NICDereferenceReceiveBlock(
    _In_ PMP_ADAPTER Adapter,
    _In_ _In_range_(0, NIC_SUPPORTED_NUM_QUEUES-1) ULONG BlockId,
    _Out_opt_ ULONG *RefCount);


BOOLEAN
NICIsBusy(
    _In_  PMP_ADAPTER  Adapter);


#define RECEIVE_BLOCK_REFERENCE_COUNT(Adapter, BlockIndex) Adapter->ReceiveBlock[BlockIndex].PendingReceives


//
// return ture if the receive block is busy
//
#define RECEIVE_BLOCK_IS_BUSY(Adapter, BlockIndex) Adapter->ReceiveBlock[BlockIndex].PendingReceives != 0

// Prototypes for standard NDIS miniport entry points
MINIPORT_INITIALIZE                 MPInitializeEx;
MINIPORT_HALT                       MPHaltEx;
MINIPORT_UNLOAD                     DriverUnload;
MINIPORT_PAUSE                      MPPause;
MINIPORT_RESTART                    MPRestart;
MINIPORT_SEND_NET_BUFFER_LISTS      MPSendNetBufferLists;
MINIPORT_RETURN_NET_BUFFER_LISTS    MPReturnNetBufferLists;
MINIPORT_CANCEL_SEND                MPCancelSend;
MINIPORT_CHECK_FOR_HANG             MPCheckForHangEx;
MINIPORT_RESET                      MPResetEx;
MINIPORT_DEVICE_PNP_EVENT_NOTIFY    MPDevicePnpEventNotify;
MINIPORT_SHUTDOWN                   MPShutdownEx;
MINIPORT_CANCEL_OID_REQUEST         MPCancelOidRequest;