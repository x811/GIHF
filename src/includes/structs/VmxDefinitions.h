#pragma once
#include <stdafx.h>

typedef struct _VMM_CONTEXT VMM_CONTEXT, * PVMM_CONTEXT;
typedef struct _VIRTUAL_CPU VIRTUAL_CPU, * PVIRTUAL_CPU;
typedef struct _NMI_PLACEHOLDER NMI_DATA, * PNMI_DATA;
typedef struct _EPT_GUEST_PAGING EPT_GUEST_PAGING, * PEPT_GUEST_PAGING;
typedef struct _SPARE_PAGES SPARE_PAGES, * PSPARE_PAGES;

typedef struct _CKPRCB
{
	ULONG MxCsr;                                                            //0x0
	UCHAR LegacyNumber;                                                     //0x4
	UCHAR ReservedMustBeZero;                                               //0x5
	UCHAR InterruptRequest;                                                 //0x6
	UCHAR IdleHalt;                                                         //0x7
	struct _KTHREAD* CurrentThread;                                         //0x8
	struct _KTHREAD* NextThread;                                            //0x10
	struct _KTHREAD* IdleThread;                                            //0x18
	UCHAR NestingLevel;                                                     //0x20
	UCHAR ClockOwner;                                                       //0x21
	union
	{
		UCHAR PendingTickFlags;                                             //0x22
		struct
		{
			UCHAR PendingTick : 1;                                            //0x22
			UCHAR PendingBackupTick : 1;                                      //0x22
		};
	};
	UCHAR IdleState;                                                        //0x23
	ULONG Number;                                                           //0x24
	ULONGLONG RspBase;                                                      //0x28
	ULONGLONG PrcbLock;                                                     //0x30
	union _KPRIORITY_STATE* PriorityState;                                  //0x38
	CHAR CpuType;                                                           //0x40
	CHAR CpuID;                                                             //0x41
	union
	{
		USHORT CpuStep;                                                     //0x42
		struct
		{
			UCHAR CpuStepping;                                              //0x42
			UCHAR CpuModel;                                                 //0x43
		};
	};
	ULONG MHz;                                                              //0x44
	ULONGLONG HalReserved[8];                                               //0x48
	USHORT MinorVersion;                                                    //0x88
	USHORT MajorVersion;                                                    //0x8a
	UCHAR BuildType;                                                        //0x8c
	UCHAR CpuVendor;                                                        //0x8d
	UCHAR LegacyCoresPerPhysicalProcessor;                                  //0x8e
	UCHAR LegacyLogicalProcessorsPerCore;                                   //0x8f
	ULONGLONG TscFrequency;                                                 //0x90
	struct _KPRCB_TRACEPOINT_LOG* TracepointLog;                            //0x98
	ULONG CoresPerPhysicalProcessor;                                        //0xa0
	ULONG LogicalProcessorsPerCore;                                         //0xa4
	ULONG SelfIpiRequestSummary;                                            //0xa8
	UCHAR QpcToTscIncrementShift;                                           //0xac
	UCHAR PrcbPad03[3];                                                     //0xad
	ULONGLONG QpcToTscIncrement;                                            //0xb0
	ULONGLONG PrcbPad04;                                                    //0xb8
	struct _KSCHEDULER_SUBNODE* SchedulerSubNode;                           //0xc0
	ULONGLONG GroupSetMember;                                               //0xc8
	UCHAR Group;                                                            //0xd0
	UCHAR GroupIndex;                                                       //0xd1
	UCHAR PrcbPad05[2];                                                     //0xd2
	ULONG InitialApicId;                                                    //0xd4
	ULONG ScbOffset;                                                        //0xd8
	ULONG ApicMask;                                                         //0xdc
} CKPRCB, * CPKPRCB;

typedef struct _HOST_STACK
{
	union
	{
		struct
		{
			uint8_t reserved[KERNEL_STACK_SIZE - sizeof(PVMM_CONTEXT)];
			PVMM_CONTEXT m_pVmmBacklink;
		} StackContents;

		uint8_t data[KERNEL_STACK_SIZE];
	};
} HOST_STACK, * PHOST_STACK;
static_assert(sizeof(HOST_STACK) == KERNEL_STACK_SIZE, "Host Stack Size is Greater Than KERNEL_STACK_SIZE");

/*
	This is architecturally 104 bytes in size. In this case I extended it to hold actual
	pointers to the pre-allocated stack, which will be used by the ISR that are marked to
	have the IST.

	The reason It can work, is that I set own Limit for the TSS using indexing by TR
	to the +1 out of what Windows uses. Adding a padding of 4 bytes is a safety measure
	so the Host won't interpret some of the last structure member as something was in the
	highest QWORD bits (e.g. not as 0xFE10, but as 0x800000000000FE10).

	This is essentially by itself undefined behaviour, but hope is on the hardware here...
*/

#pragma pack(push, 1)
typedef struct _CPU_TSS
{
	uint32_t reserved0;
	uint64_t RSP0;
	uint64_t RSP1;
	uint64_t RSP2;
	uint64_t reserved1;
	uint64_t IST1;
	uint64_t IST2;
	uint64_t IST3;
	uint64_t IST4;
	uint64_t IST5;
	uint64_t IST6;
	uint64_t IST7;
	uint64_t reserved2;
	uint32_t reserved_IOPB;
	uint32_t padding3;
	PVOID IST1_STACK;
	PVOID IST2_STACK;
} CPU_TSS, * PCPU_TSS;
static_assert(sizeof(CPU_TSS) == 0x7C, "The size of CPU TSS isn't matched with custom x64");
#pragma pack(pop)

/*
	This will be the struct of PAGE_SIZE that holds everything possible about
	this VMM and it will be in the HOST_FS_BASE, so that when interrupt
	occurs (e.g. NMI) the data contained here will be available for the NMI
	handler without using dumb ambiguous ways to put that data there...

	Future versions of Windows Kernel can start to use this, so it requires to
	be aware of all possible Windows Kernel updates in future.
*/

typedef struct _CPU_FS
{
	PNMI_DATA m_pNmiWorkItem;
	PVIRTUAL_CPU m_pCurrentCpu;
	PEPT_GUEST_PAGING m_pCurrentEpt[2];
} CPU_FS, * PCPU_FS;

typedef struct _MSR_BITMAP
{
	RTL_BITMAP m_pBitMapHeader;
	PVMX_MSR_BITMAP m_pMsrBitmap;
} MSR_BITMAP, * PMSR_BITMAP;

typedef struct _COMM_PAGE
{
	PVOID m_pPage;
	uint64_t m_iPhysicalAddress;
	PTE_64* m_sPageEntry;
} COMM_PAGE, *PCOMM_PAGE;

typedef struct _SPARE_PAGE
{
	bool m_bIsTaken;
	PVOID m_pPage;
	uint64_t m_iPhysicalAddress;
	uint16_t m_iIndex;
} SPARE_PAGE, * PSPARE_PAGE;

typedef struct _SPLIT_PAGE : _SPARE_PAGE
{
	uint64_t m_iPageVirtualAddress;
} SPLIT_PAGE, * PSPLIT_PAGE;

typedef struct _WATCH_PAGE : _SPARE_PAGE
{
	uint64_t m_iPageVirtualAddress;
	uint64_t m_iWatchedAddress;
} WATCH_PAGE, * PWATCH_PAGE;

typedef struct _HOOK_PAGE
{
	bool m_bIsTaken;
	uint8_t m_iIndex;

	PVOID m_pOriginalPage;
	uint64_t m_iOriginalPhysAddr;

	PVOID m_pModifiedPage;
	uint64_t m_iModifiedPhys;
} HOOK_PAGE, * PHOOK_PAGE;

typedef struct _SPARE_PAGES
{
	SPARE_PAGE m_sSparePages[24];
	SPLIT_PAGE m_sSplitPages[24];
	HOOK_PAGE m_sHookPages[12];
} SPARE_PAGES, * PSPARE_PAGES;

typedef struct _EPT_GUEST_PAGING
{
	EPT_POINTER m_pEptTranslation;
	EPT_PML4* m_pEptPml4;
	EPDPTE(*m_pEptPdpt)[512];
	EPDE_2MB(*m_pEptPd)[512][512];

} EPT_GUEST_PAGING, * PEPT_GUEST_PAGING;

typedef struct _VARIABLE_MTRR
{
	IA32_MTRR_PHYSBASE_REGISTER m_sMtrr0;
	IA32_MTRR_PHYSMASK_REGISTER m_sMtrrMask0;
} VARIABLE_MTRR, * PVARIABLE_MTRR;

/*
*   This struct is a packet that the Hypervisor must process.
*   It's not passed directly to the Hypervisor rather than the
*   address of it.
*/


typedef struct _COMMON_HV_PACKET
{
	uint8_t m_iMessageId;
	uint8_t m_iOperationId;
} COMMON_PACKET, * PCOMMON_PACKET;

typedef struct _BUFFER_PACKET : _COMMON_HV_PACKET
{
	uint8_t m_sMessageBuffer[0x1000 - sizeof(uint8_t) * 2];
} BUFFER_PACKET, *PBUFFER_PACKET;

typedef struct _HV_PACKET_ACKNOLEDGE_PRESENCE : _COMMON_HV_PACKET
{
	uint64_t m_iResponse;
} PACKET_ACKNOLEDGE_PRESENCE;

typedef struct _VIRTUAL_CPU
{
	uint32_t m_iCoreCount;
	CR3 m_sGuestPhysicalBase;

	uint64_t m_iCommunicationPageAddressOriginal;

	VMXON* m_pVmxonRegion;
	VMCS* m_pVmcsRegion;

	PCPU_TSS m_pCpuTss;
	PCPU_FS m_pCpuFSBase;

	PVOID m_pGlobalDescriptorTable;
	PVOID m_pInterruptTable;

	PVOID m_pApicPage;

	CPKPRCB m_pKprсb;
	PCOMM_PAGE m_pCommPage;
	PSPARE_PAGES m_pSparePages;

	/*
	*   The reason behind TWO EPTPs is quite simple.
	*
	*   First can be used in order to hook an executable memory, modules.
	*   Other can be used in order to fake the memory that is being modified
	*   through the first EPTP.
	*
	*   Condition: First is modifiable memory, second is legit (fake) memory.
	*/
	PEPT_GUEST_PAGING m_pEptGuestPaging[2];
	PML4E_64* m_pHostPagingBase;
	PHOST_STACK m_pHostStack;
	PVMM_CONTEXT m_pVmmBacklink;
} VIRTUAL_CPU, * PVIRTUAL_CPU;

typedef struct _NMI_PLACEHOLDER
{
	bool m_bSendByHost;
	bool m_bPendingForExecution;
	bool m_bTlbFlush;
	uint16_t m_iIssuedCoreId;
	uint16_t m_iCompletedCount;
	uint16_t m_iArrivedCount;
	uint16_t m_iFuncCode;

	uint64_t m_iFuncArgs[24];
} NMI_DATA, * PNMI_DATA;

typedef struct _VMM_CONTEXT
{
	bool m_bTrueControls;
	bool m_bFixedMtrrs;
	bool m_bMappedDriver;
	uint8_t m_iVariableMtrrsCount;

	uint32_t m_iPhyWidthBits;
	uint32_t m_iCoreCount;

	uint64_t m_iHvBase;
	uint32_t m_iHvSize;

	PVIRTUAL_CPU* m_pCpuArray;
	MSR_BITMAP m_MsrBitmap;

	PNMI_DATA m_pNmiWorkerItem;
	PVARIABLE_MTRR m_pVariableMtrrs;
} VMM_CONTEXT, * PVMM_CONTEXT;