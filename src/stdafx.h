#pragma once
#include <ntifs.h>
#include <ntddk.h>
#include <ntddvdeo.h>
#include <cstdint>
#include <cstdarg>
#include <intrin.h>
#include <TraceLoggingProvider.h>
#include <ia32.h>

#include <Log.h>

#define PAGE_SHIFT_2MB			21
#define PAGE_SIZE_2MB			0x200000

#define COMM_CODE0				0x68100211
#define COMM_USER_COMM_START	0x2080
#define COMM_USER_COMM			0x2085
#define COMM_HIDE_SELF			0x2400
#define COMM_WATCH_PAGE			0x2405
#define COMM_HOOK_PAGE			0x2410

#define COMM_ACKNOLEDGE_GUEST	0x100

EXTERN_C void _sgdt(void*);
EXTERN_C uint64_t __readfsqword(uint64_t offset);

EXTERN_C uint16_t __read_cs(void);
EXTERN_C uint16_t __read_es(void);
EXTERN_C uint16_t __read_ds(void);
EXTERN_C uint16_t __read_ss(void);
EXTERN_C uint16_t __read_fs(void);
EXTERN_C uint16_t __read_gs(void);
EXTERN_C uint16_t __read_tr(void);
EXTERN_C uint16_t __read_ldtr(void);
EXTERN_C uint64_t __load_ar(uint64_t SegmentDescriptorValue);

EXTERN_C void VmExitStub(void);
EXTERN_C bool BeginVirtualization(void* pCpu);

EXTERN_C void software_nmi(void);
EXTERN_C void debugbreak(void);

EXTERN_C uint8_t __invept(uint8_t InvalidateContextType, PINVEPT_DESCRIPTOR pInveptDescriptor);
EXTERN_C void __pause(void);
EXTERN_C void crash_system(void);

#pragma intrinsic(__pause)

typedef struct _KMODULE_INFO
{
	uint64_t m_iBase;
	uint64_t m_iSize;
} KMODULE_INFO, * PKMODULE_INFO;

typedef struct _INTERRUPT_DESCRIPTOR_64
{
	union
	{
		struct
		{
			uint64_t OffsetLow : 16;
			uint64_t SegmentSelector : 16;
			uint64_t InterruptStackTable : 3;
			uint64_t reserved0 : 5;
			uint64_t Type : 4;
			uint64_t reserved1 : 1;
			uint64_t DescriptorPrivilegeLevel : 2;
			uint64_t Present : 1;
			uint64_t OffsetMid : 16;
			uint64_t OffsetHigh : 32;
			uint64_t reserved2 : 32;
		} bits;
		__m128 Flags;
	};
} INTERRUPT_DESCRIPTOR_64, *PINTERRUPT_DESCRIPTOR_64;

typedef struct _TASK_STATE_SEGMENT_64
{
	union
	{
		struct
		{
			uint64_t LimitLow : 16;
			uint64_t BaseLow : 16;
			uint64_t BaseMid : 8;
			uint64_t Type : 4;
			uint64_t reserved0 : 1;
			uint64_t DescriptorPrivilegeLevel : 2;
			uint64_t Present : 1;
			uint64_t LimitMid : 4;
			uint64_t Available : 1;
			uint64_t reserved1 : 2;
			uint64_t Granularity : 1;
			uint64_t BaseHigh : 8;
			uint64_t BaseHighest : 32;
			uint64_t reserved2 : 32;
		} bits;
		__m128 Flags;
	};
} TASK_STATE_SEGMENT_64, *PTASK_STATE_SEGMENT_64;

typedef struct _VIRTUAL_ADDRESS
{
	union
	{
		struct
		{
			uint64_t PageOffset : 12;
			uint64_t PTE : 9;
			uint64_t PDE : 9;
			uint64_t PDPTE : 9;
			uint64_t PML4E : 9;
			uint64_t CanonicalBits : 16;
		} bits;
		uint64_t Flags;
	};
} VIRTUAL_ADDRESS, * PVIRTUAL_ADDRESS;
static_assert(sizeof(VIRTUAL_ADDRESS) == sizeof(uint64_t), "Virtual Memory Address Structure Size Mismatch");

/*
*	Windows Related. These are not exported or defined by WDK
*/

enum SYSTEM_INFORMATION_CLASS
{
	SystemModuleInformation = 0xB,
};

EXTERN_C NTSTATUS ZwQuerySystemInformation(IN SYSTEM_INFORMATION_CLASS 	SystemInfoClass,
	OUT PVOID 	SystemInfoBuffer,
	IN ULONG 	SystemInfoBufferSize,
	OUT PULONG BytesReturned 	OPTIONAL
);

typedef struct _SYSTEM_MODULE_ENTRY
{
	HANDLE Section;
	PVOID MappedBase;
	PVOID ImageBase;
	ULONG ImageSize;
	ULONG Flags;
	USHORT LoadOrderIndex;
	USHORT InitOrderIndex;
	USHORT LoadCount;
	USHORT OffsetToFileName;
	UCHAR FullPathName[256];
} SYSTEM_MODULE_ENTRY, * PSYSTEM_MODULE_ENTRY;

typedef struct _SYSTEM_MODULE_INFORMATION
{
	ULONG Count;
	SYSTEM_MODULE_ENTRY Module[1];
} SYSTEM_MODULE_INFORMATION, * PSYSTEM_MODULE_INFORMATION;

typedef struct _IMAGE_DOS_HEADER
{
	USHORT e_magic;                                                         //0x0
	USHORT e_cblp;                                                          //0x2
	USHORT e_cp;                                                            //0x4
	USHORT e_crlc;                                                          //0x6
	USHORT e_cparhdr;                                                       //0x8
	USHORT e_minalloc;                                                      //0xa
	USHORT e_maxalloc;                                                      //0xc
	USHORT e_ss;                                                            //0xe
	USHORT e_sp;                                                            //0x10
	USHORT e_csum;                                                          //0x12
	USHORT e_ip;                                                            //0x14
	USHORT e_cs;                                                            //0x16
	USHORT e_lfarlc;                                                        //0x18
	USHORT e_ovno;                                                          //0x1a
	USHORT e_res[4];                                                        //0x1c
	USHORT e_oemid;                                                         //0x24
	USHORT e_oeminfo;                                                       //0x26
	USHORT e_res2[10];                                                      //0x28
	LONG e_lfanew;                                                          //0x3c
} IMAGE_DOS_HEADER;

struct _IMAGE_OPTIONAL_HEADER64
{
	USHORT Magic;                                                           //0x0
	UCHAR MajorLinkerVersion;                                               //0x2
	UCHAR MinorLinkerVersion;                                               //0x3
	ULONG SizeOfCode;                                                       //0x4
	ULONG SizeOfInitializedData;                                            //0x8
	ULONG SizeOfUninitializedData;                                          //0xc
	ULONG AddressOfEntryPoint;                                              //0x10
	ULONG BaseOfCode;                                                       //0x14
	ULONGLONG ImageBase;                                                    //0x18
	ULONG SectionAlignment;                                                 //0x20
	ULONG FileAlignment;                                                    //0x24
	USHORT MajorOperatingSystemVersion;                                     //0x28
	USHORT MinorOperatingSystemVersion;                                     //0x2a
	USHORT MajorImageVersion;                                               //0x2c
	USHORT MinorImageVersion;                                               //0x2e
	USHORT MajorSubsystemVersion;                                           //0x30
	USHORT MinorSubsystemVersion;                                           //0x32
	ULONG Win32VersionValue;                                                //0x34
	ULONG SizeOfImage;                                                      //0x38
	ULONG SizeOfHeaders;                                                    //0x3c
	ULONG CheckSum;                                                         //0x40
	USHORT Subsystem;                                                       //0x44
	USHORT DllCharacteristics;                                              //0x46
	ULONGLONG SizeOfStackReserve;                                           //0x48
	ULONGLONG SizeOfStackCommit;                                            //0x50
	ULONGLONG SizeOfHeapReserve;                                            //0x58
	ULONGLONG SizeOfHeapCommit;                                             //0x60
	ULONG LoaderFlags;                                                      //0x68
	ULONG NumberOfRvaAndSizes;                                              //0x6c
	void* DataDirectory[16];                         //0x70
};

struct _IMAGE_FILE_HEADER
{
	USHORT Machine;                                                         //0x0
	USHORT NumberOfSections;                                                //0x2
	ULONG TimeDateStamp;                                                    //0x4
	ULONG PointerToSymbolTable;                                             //0x8
	ULONG NumberOfSymbols;                                                  //0xc
	USHORT SizeOfOptionalHeader;                                            //0x10
	USHORT Characteristics;                                                 //0x12
};


struct _IMAGE_NT_HEADERS64
{
	ULONG Signature;                                                        //0x0
	struct _IMAGE_FILE_HEADER FileHeader;                                   //0x4
	struct _IMAGE_OPTIONAL_HEADER64 OptionalHeader;                         //0x18
};

#include <Memory.h>
#include <segmentation.h>

#include <VmxOperationPacket.h>
#include <VmxPrerequisites.h>
#include <VmxDefinitions.h>
#include <VmxUtils.h>

#include <VmxApicConfig.h>
#include <VmxIsrConfig.h>
#include <VmxVmcsConfig.h>
#include <VmxMsrConfig.h>
#include <VmxMtrrConfig.h>
#include <VmxEptControls.h>
#include <VmxVmmConfig.h>

#include <vmexit_structs.h>
#include <exit_handlers.h>