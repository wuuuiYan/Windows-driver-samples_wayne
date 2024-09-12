/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

   TcbRcb.H

Abstract:

    This module declares the TCB and RCB structures, and the functions to manipulate them.

    See the comments in TcbRcb.c.

--*/


#ifndef _TCBRCB_H
#define _TCBRCB_H

//
// TCB (Transmit Control Block)
// -----------------------------------------------------------------------------
//

typedef struct _TCB
{
    LIST_ENTRY              TcbLink;
	// 传输操作通常只需处理单个缓冲区或者相对简单的数据
	// 使用 NET_BUFFER 这种较简单、轻量级的结构已足够。它便于快速访问和操作单个数据缓冲区，从而优化传输性能
    PNET_BUFFER             NetBuffer;         // Each NB structure packages a packet of network data.
    ULONG                   FrameType;
    ULONG                   BytesActuallySent; // 实际发送的字节数
} TCB, *PTCB;

VOID
ReturnTCB(
    _In_  PMP_ADAPTER  Adapter,
    _In_  PTCB         Tcb);

//
// RCB (Receive Control Block)
// -----------------------------------------------------------------------------
//

typedef struct _RCB
{
    LIST_ENTRY              RcbLink;
	// 接收操作通常需要处理复杂的数据，因为接收的数据可能由多个部分组成，需要组合在一起进行处理
	// 使用 NET_BUFFER_LIST，driver 可以方便地处理包含多个缓冲区的数据包，确保数据完整性和处理效率
    PNET_BUFFER_LIST        Nbl;
    PVOID                   Data;
#if (NDIS_SUPPORT_NDIS620)
    PVOID                   LookaheadData; // 指向预读取的数据
#endif
} RCB, *PRCB;

// 表示函数调用者必须检查函数的返回值，特别是在代码审查工具和静态分析工具中能起到提醒作用
_Must_inspect_result_
// 表示函数在返回值不为 NULL 时执行成功。这是一个条件性成功标志，用来辅助工具和开发者理解成功的条件
_Success_(return != NULL)
PRCB
GetRCB(
    _In_  PMP_ADAPTER  Adapter,
    _In_  PNDIS_NET_BUFFER_LIST_8021Q_INFO Nbl1QInfo,
    _In_  PFRAME       Frame);

VOID
ReturnRCB(
    _In_  PMP_ADAPTER   Adapter,
    _In_  PRCB          Rcb);

#endif // _TCBRCB_H