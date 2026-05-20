#include <stdafx.h>

/*
*	If the caller does erase header, the initialization through
*	the vulnerable driver will be not possible, this is not
*	likeable as the actual production grade should pass to the
*	Main entry allocation pointer and it's size, so it does know
*	how the memory is laid out.
*/

extern "C" IMAGE_DOS_HEADER __ImageBase;
NTSTATUS GetCurrentDriverInfo(KMODULE_INFO* kMod)
{
	if (__ImageBase.e_magic != 0x5A4D)
		return STATUS_INVALID_IMAGE_FORMAT;

	const _IMAGE_NT_HEADERS64* headers = reinterpret_cast<_IMAGE_NT_HEADERS64*>(reinterpret_cast<ULONG64>(&__ImageBase) + __ImageBase.e_lfanew);
	if (headers->Signature != 0x4550)
		return STATUS_INVALID_IMAGE_FORMAT;

	kMod->m_iSize = headers->OptionalHeader.SizeOfImage;
	kMod->m_iBase = reinterpret_cast<ULONG64>(&__ImageBase);

	return STATUS_SUCCESS;
}

/*
	This will mostly copy the info from the Guest's GDT as I am not
	planning now to make every detail from scratch...

	Main interest lies in the Task Register, which ultimately points
	at the location of Task State Segment, which is of interest
	because in the our IDT NMI handler is destined to has it's own
	stack frame when switch to NMI Interrupt Service Routine occurs.

	Because of this, it needs to be the separate from the Guest as
	the Guest has it's own TSS which it uses internally, that means
	VMM needs own TSS setup in order to run it's ISRs which defined
	to have their own stack frame...
*/

void BuildHostGlobalTable(const PVIRTUAL_CPU pCpu, PVOID pGlobalTablePage)
{
	SEGMENT_DESCRIPTOR_REGISTER_64 pGlobalDescriptorRegister{ 0 };
	PTASK_STATE_SEGMENT_64 pTaskSegmentDescriptor{ 0 };
	SEGMENT_SELECTOR TaskSegmentSelector{ 0 };
	uint64_t* pGlobalTableAddr{ 0 };

	_sgdt(&pGlobalDescriptorRegister);
	pGlobalTableAddr = (uint64_t*)pGlobalTablePage;

	RtlCopyMemory(pGlobalTableAddr, (PVOID)pGlobalDescriptorRegister.BaseAddress, pGlobalDescriptorRegister.Limit + 1);

	TaskSegmentSelector.Flags = __read_tr();

	pTaskSegmentDescriptor = reinterpret_cast<PTASK_STATE_SEGMENT_64>(&pGlobalTableAddr[TaskSegmentSelector.Index]);

	uint64_t pTaskStateAddress = (uint64_t)pCpu->m_pCpuTss;

	pTaskSegmentDescriptor->bits.BaseLow = pTaskStateAddress & MAXUINT16;
	pTaskSegmentDescriptor->bits.BaseMid = (pTaskStateAddress >> 16) & MAXUINT8;
	pTaskSegmentDescriptor->bits.BaseHigh = (pTaskStateAddress >> 24) & MAXUINT8;
	pTaskSegmentDescriptor->bits.BaseHighest = (pTaskStateAddress >> 32) & MAXUINT32;

	pTaskSegmentDescriptor->bits.LimitLow = sizeof(CPU_TSS);
	pTaskSegmentDescriptor->bits.DescriptorPrivilegeLevel = 0;
	pTaskSegmentDescriptor->bits.Present = 1;
}

/*
	This will build the interrupt vectors for the host to use if needed.

	For now it will be only NMI filled...
*/

void BuildHostInterruptTable(PVOID pInterruptTablePage)
{
	INTERRUPT_DESCRIPTOR_64 InterruptDescriptor{ 0 };
	PINTERRUPT_DESCRIPTOR_64 pInterruptTableAddr{ nullptr };

	pInterruptTableAddr = reinterpret_cast<PINTERRUPT_DESCRIPTOR_64>(pInterruptTablePage);

	uint64_t NmiHandlerAddr = (uint64_t)&IsrConfig::HvIsrNmiInterrupt;

	InterruptDescriptor.bits.OffsetLow = NmiHandlerAddr & MAXUINT16;
	InterruptDescriptor.bits.OffsetMid = (NmiHandlerAddr >> 16) & MAXUINT16;
	InterruptDescriptor.bits.OffsetHigh = (NmiHandlerAddr >> 32) & MAXUINT32;

	InterruptDescriptor.bits.DescriptorPrivilegeLevel = 0;
	InterruptDescriptor.bits.InterruptStackTable = 1;
	InterruptDescriptor.bits.Type = 0xE; // Clears the IF in RFLAGS
	InterruptDescriptor.bits.Present = 1;
	InterruptDescriptor.bits.SegmentSelector = __read_cs();

	pInterruptTableAddr[2].Flags = InterruptDescriptor.Flags;

	RtlZeroMemory(&InterruptDescriptor.Flags, sizeof(InterruptDescriptor));

	/* This must be my own handler, otherwise it will triple fault */
	uint64_t DebugHandlerAddr = (uint64_t)&IsrConfig::HvIsrDbgInterrupt;

	InterruptDescriptor.bits.OffsetLow = DebugHandlerAddr & MAXUINT16;
	InterruptDescriptor.bits.OffsetMid = (DebugHandlerAddr >> 16) & MAXUINT16;
	InterruptDescriptor.bits.OffsetHigh = (DebugHandlerAddr >> 32) & MAXUINT32;

	InterruptDescriptor.bits.DescriptorPrivilegeLevel = 0;
	InterruptDescriptor.bits.InterruptStackTable = 0;
	InterruptDescriptor.bits.Type = 0xE;
	InterruptDescriptor.bits.Present = 1;
	InterruptDescriptor.bits.SegmentSelector = __read_cs();

	pInterruptTableAddr[3].Flags = InterruptDescriptor.Flags;
}

PVIRTUAL_CPU AllocateCpu(PVMM_CONTEXT pVmm)
{
	PVIRTUAL_CPU pCpu;
	IA32_VMX_BASIC_REGISTER BasicRegister;

	pCpu = reinterpret_cast<PVIRTUAL_CPU>(MemoryUtils::Allocate(sizeof(VIRTUAL_CPU)));
	RtlZeroMemory(pCpu, sizeof(VIRTUAL_CPU));

	BasicRegister.Flags = __readmsr(IA32_VMX_BASIC);

	pCpu->m_sGuestPhysicalBase.Flags = __readcr3();
	pCpu->m_pVmxonRegion = reinterpret_cast<VMXON*>(MemoryUtils::AllocatePages(PAGE_SIZE));
	pCpu->m_pVmcsRegion = reinterpret_cast<VMCS*>(MemoryUtils::AllocatePages(PAGE_SIZE));
	pCpu->m_pApicPage = reinterpret_cast<PVOID>(MemoryUtils::AllocatePages(PAGE_SIZE));
	pCpu->m_pHostStack = reinterpret_cast<PHOST_STACK>(MemoryUtils::AllocatePages(KERNEL_STACK_SIZE));

	for (size_t i = 0; i < sizeof(pCpu->m_pEptGuestPaging) / sizeof(PEPT_GUEST_PAGING); i++)
	{
		pCpu->m_pEptGuestPaging[i] = reinterpret_cast<PEPT_GUEST_PAGING>(MemoryUtils::Allocate(sizeof(EPT_GUEST_PAGING)));
	}
	//pCpu->m_pEptGuestPaging = reinterpret_cast<PEPT_GUEST_PAGING>(MemoryUtils::Allocate(sizeof(EPT_GUEST_PAGING)));

	//============This is kind of ugly============
	pCpu->m_pCommPage = reinterpret_cast<PCOMM_PAGE>(MemoryUtils::Allocate(sizeof(COMM_PAGE)));
	
	pCpu->m_pCommPage->m_pPage = MemoryUtils::AllocatePages(PAGE_SIZE);
	pCpu->m_pCommPage->m_iPhysicalAddress = MmGetPhysicalAddress(pCpu->m_pCommPage->m_pPage).QuadPart;
	pCpu->m_pCommPage->m_sPageEntry = reinterpret_cast<PTE_64*>(VmxUtils::GetPte(pCpu->m_sGuestPhysicalBase.Flags, (uint64_t)pCpu->m_pCommPage->m_pPage));
	//============================================

	pCpu->m_pSparePages = reinterpret_cast<PSPARE_PAGES>(MemoryUtils::Allocate(sizeof(SPARE_PAGES)));

	pCpu->m_pGlobalDescriptorTable = reinterpret_cast<PVOID>(MemoryUtils::AllocatePages(PAGE_SIZE));
	pCpu->m_pInterruptTable = reinterpret_cast<PVOID>(MemoryUtils::AllocatePages(PAGE_SIZE));

	pCpu->m_pCpuTss = reinterpret_cast<PCPU_TSS>(MemoryUtils::AllocatePages(PAGE_SIZE));
	pCpu->m_pCpuTss->reserved_IOPB = sizeof(CPU_TSS);

	pCpu->m_pCpuTss->IST1_STACK = MemoryUtils::AllocatePages(KERNEL_STACK_SIZE);
	pCpu->m_pCpuTss->IST2_STACK = MemoryUtils::AllocatePages(KERNEL_STACK_SIZE);

	pCpu->m_pCpuTss->IST1 = (uint64_t) & reinterpret_cast<PHOST_STACK>(pCpu->m_pCpuTss->IST1_STACK)->StackContents.m_pVmmBacklink;
	pCpu->m_pCpuTss->IST2 = (uint64_t) & reinterpret_cast<PHOST_STACK>(pCpu->m_pCpuTss->IST2_STACK)->StackContents.m_pVmmBacklink;

	pCpu->m_pCpuFSBase = reinterpret_cast<PCPU_FS>(MemoryUtils::AllocatePages(PAGE_SIZE));

	pCpu->m_pVmxonRegion->RevisionId = BasicRegister.VmcsRevisionId;
	pCpu->m_pVmxonRegion->MustBeZero = NULL;

	pCpu->m_pVmcsRegion->RevisionId = BasicRegister.VmcsRevisionId;
	pCpu->m_pVmcsRegion->ShadowVmcsIndicator = NULL;

	pCpu->m_pVmmBacklink = pVmm;
	pCpu->m_iCoreCount = pVmm->m_iCoreCount;

	pCpu->m_pHostStack->StackContents.m_pVmmBacklink = pVmm;

	pCpu->m_pCpuFSBase->m_pNmiWorkItem = pVmm->m_pNmiWorkerItem;
	pCpu->m_pCpuFSBase->m_pCurrentCpu = pCpu;

	for (uint8_t i = 0; i < sizeof(pCpu->m_pEptGuestPaging) / sizeof(PEPT_GUEST_PAGING); i++)
	{
		pCpu->m_pCpuFSBase->m_pCurrentEpt[i] = pCpu->m_pEptGuestPaging[i];
	}

	/*
		These are a free pages, used for whatever reason in future, e.g. stealth hooking,
		hiding and other common stuff related to physical memory translation.
	*/

	for (uint16_t i = 0; i < sizeof(pCpu->m_pSparePages->m_sSparePages) / sizeof(SPARE_PAGE); i++)
	{
		pCpu->m_pSparePages->m_sSparePages[i].m_pPage = MemoryUtils::AllocatePages(PAGE_SIZE);
		pCpu->m_pSparePages->m_sSparePages[i].m_iPhysicalAddress = MmGetPhysicalAddress(pCpu->m_pSparePages->m_sSparePages[i].m_pPage).QuadPart;
		pCpu->m_pSparePages->m_sSparePages[i].m_iIndex = i;
		pCpu->m_pSparePages->m_sSparePages[i].m_bIsTaken = false;
	}

	for (uint16_t i = 0; i < sizeof(pCpu->m_pSparePages->m_sSplitPages) / sizeof(SPLIT_PAGE); i++)
	{
		pCpu->m_pSparePages->m_sSplitPages[i].m_pPage = MemoryUtils::AllocatePages(PAGE_SIZE);
		pCpu->m_pSparePages->m_sSplitPages[i].m_iPhysicalAddress = MmGetPhysicalAddress(pCpu->m_pSparePages->m_sSplitPages[i].m_pPage).QuadPart;
		pCpu->m_pSparePages->m_sSplitPages[i].m_iIndex = i;
		pCpu->m_pSparePages->m_sSplitPages[i].m_bIsTaken = false;
		pCpu->m_pSparePages->m_sSplitPages[i].m_iPageVirtualAddress = reinterpret_cast<uint64_t>(pCpu->m_pSparePages->m_sSplitPages[i].m_pPage);
	}

	for (uint16_t i = 0; i < sizeof(pCpu->m_pSparePages->m_sHookPages) / sizeof(HOOK_PAGE); i++)
	{
		pCpu->m_pSparePages->m_sHookPages[i].m_bIsTaken = false;
		pCpu->m_pSparePages->m_sHookPages[i].m_iIndex = i;
		pCpu->m_pSparePages->m_sHookPages[i].m_pModifiedPage = MemoryUtils::AllocatePages(PAGE_SIZE);
		pCpu->m_pSparePages->m_sHookPages[i].m_iModifiedPhys = MmGetPhysicalAddress(pCpu->m_pSparePages->m_sHookPages[i].m_pModifiedPage).QuadPart;
	}

	BuildHostGlobalTable(pCpu, pCpu->m_pGlobalDescriptorTable);
	BuildHostInterruptTable(pCpu->m_pInterruptTable);

	VmmConfig::BuildHostPaging(pCpu);
	VmxExtendedPaging::VmSetupEptPagingStructures(pCpu);

	/*
	*	This is essentially a shitty way of setting up first 1MB of memory
	*	to the correct type.
	* 
	*	This 1MB memory type is determined by the Fixed MTRRs, but first it
	*	needs to be split up to 4KB pages, because it uses 2MB pages from
	*	the initialization in the previous line.
	*/

	for (uint8_t i = 0; i < sizeof(pCpu->m_pEptGuestPaging) / sizeof(PEPT_GUEST_PAGING); i++)
	{
		VmxExtendedPaging::VmSplitLargePage(pCpu, &pCpu->m_pEptGuestPaging[i]->m_pEptPd[0][0][0]);
	}
	
	/*
	*	This can be uncommented and the previous two lines commented in
	*	order to use Identity Mapping, which just maps every physical
	*	Base PFN as the 4KB EPT, this causes higher usage of memory.
	* 
	*	This is not safe to use with a hot-plug of RAM in actual system
	*	as it doesn't account for the RAM that is beyond what the system
	*	already defined.
	* 
	*	The usage of this function instead of previous two is, for now,
	*	not reasonable.
	*/
	//VmxExtendedPaging::VmSetupEptIdentityPaging(pCpu);
	return pCpu;
}

EXTERN_C bool prepare_for_virtualization(PVIRTUAL_CPU pCpu, uint64_t guest_rsp, uint64_t guest_rip)
{
	if (!NT_SUCCESS(VmcsConfig::SwitchToVmxAndSetLoadVmcs(pCpu)))
	{
		Log("VMXON Entering Failure\n");
		return FALSE;
	}

	if (!NT_SUCCESS(VmcsConfig::HvSetupVmcsStructure(pCpu, (uint64_t)&VmExitStub, (uint64_t)&pCpu->m_pHostStack->StackContents.m_pVmmBacklink, guest_rip, guest_rsp)))
	{
		Log("VMCS Structure wasn't setup properly!\n");
		return FALSE;
	}

	if (__vmx_vmlaunch() != 0)
	{
		SIZE_T instruction_error_code = 0;
		__vmx_vmread(VMCS_VM_INSTRUCTION_ERROR, &instruction_error_code);
		Log("The VM launch failed with the code of: 0x%p\n", instruction_error_code);
		return FALSE;
	}
}

/*

	Stub that is used to pass data to another stub, which in turn calls a function with a
	subsequent meaning of setting up and configuring virtualization environment on all cores

*/

_Use_decl_annotations_
uint64_t setup_virtual_cpu(uint64_t Vmm)
{
	PROCESSOR_NUMBER pn = { 0 };
	PVMM_CONTEXT pVmm = (PVMM_CONTEXT)(Vmm);

	uint32_t current_cpu_number = KeGetCurrentProcessorNumberEx(&pn);
	PVIRTUAL_CPU pCpu = pVmm->m_pCpuArray[current_cpu_number];

	bool status = BeginVirtualization(pCpu);
	return status;
}

/*

	Mostly related to setting up the Virtual Monitor e.g. virtual processors,
	MSR bitmaps and so on, notably m_iCoreCount doesn't carry any meaning
	for now, but it should be used to track cores, or get cores in subsequent
	functions used...

*/

NTSTATUS MainEntry()
{
	RTL_BITMAP pBitMap;
	PVMM_CONTEXT pVmm;
	IA32_VMX_BASIC_REGISTER BasicRegister;
	IA32_MTRR_CAPABILITIES_REGISTER MtrrCap;
	KMODULE_INFO HvModule;
	
	if (!NT_SUCCESS(TraceLoggingRegister(provider)))
	{
		Log("Shitty provider not registered");
		return STATUS_UNSUCCESSFUL;
	}

	BasicRegister.Flags = __readmsr(IA32_VMX_BASIC);
	MtrrCap.Flags = __readmsr(IA32_MTRR_CAPABILITIES);

	/*
	*	This is not a production grade, if this is mapped by Vulnerable Driver
	*	It's pretty much game over as it won't have any record in Windows
	*	that it's a driver. It will essentially be just an unsigned code
	*	executing in the kernel.
	*
	*	That is the task for a kernel driver mapper to properly pass an address
	*	of the allocation and size.
	*/

	pVmm = reinterpret_cast<PVMM_CONTEXT>(MemoryUtils::Allocate(sizeof(VMM_CONTEXT)));

#ifdef _PRODUCTION
	GetCurrentDriverInfo(&HvModule);
	pVmm->m_bMappedDriver = true;
#else
	if (!MemoryUtils::GetKernelModuleAddressAndSize(&HvModule, "GIHF.sys"))
		return STATUS_UNSUCCESSFUL;

	pVmm->m_bMappedDriver = false;
#endif

	pVmm->m_bTrueControls = BasicRegister.VmxControls;
	pVmm->m_bFixedMtrrs = MtrrCap.FixedRangeSupported;

	pVmm->m_iCoreCount = KeQueryMaximumProcessorCountEx(ALL_PROCESSOR_GROUPS);
	pVmm->m_iPhyWidthBits = VmxDeps::RetrieveMaxPhyWidth();

	pVmm->m_iVariableMtrrsCount = MtrrCap.VariableRangeCount;

	pVmm->m_iHvBase = HvModule.m_iBase;
	pVmm->m_iHvSize = HvModule.m_iSize;

	Log("Base: %p\nSize: %p\n", HvModule.m_iBase, HvModule.m_iSize);

	pVmm->m_pCpuArray = reinterpret_cast<PVIRTUAL_CPU*>(MemoryUtils::Allocate(sizeof(PVIRTUAL_CPU) * pVmm->m_iCoreCount));
	pVmm->m_pNmiWorkerItem = reinterpret_cast<PNMI_DATA>(MemoryUtils::AllocatePages(PAGE_SIZE));
	pVmm->m_pVariableMtrrs = reinterpret_cast<PVARIABLE_MTRR>(MemoryUtils::Allocate(sizeof(VARIABLE_MTRR) * MtrrCap.VariableRangeCount));

	for (uint8_t i = 0; i < MtrrCap.VariableRangeCount; i++)
	{
		pVmm->m_pVariableMtrrs[i].m_sMtrr0.Flags = __readmsr(IA32_MTRR_PHYSBASE0 + (i * 2));
		pVmm->m_pVariableMtrrs[i].m_sMtrrMask0.Flags = __readmsr(IA32_MTRR_PHYSMASK0 + (i * 2));
	}

	/*
		This is mostly an initialization of MSR Bitmap, so that RDMSR and WRMSR
		cause VMExit on a particular MSRs of interest
	*/

	pVmm->m_MsrBitmap.m_pMsrBitmap = reinterpret_cast<PVMX_MSR_BITMAP>(MemoryUtils::AllocatePages(PAGE_SIZE));
	RtlInitializeBitMap(&pVmm->m_MsrBitmap.m_pBitMapHeader, (PULONG)pVmm->m_MsrBitmap.m_pMsrBitmap, PAGE_SIZE);
	/*MsrConfig::ActivateMsrBit(&pVmm->m_MsrBitmap, IA32_TIME_STAMP_COUNTER, false);
	MsrConfig::ActivateMsrBit(&pVmm->m_MsrBitmap, IA32_APERF, false);
	MsrConfig::ActivateMsrBit(&pVmm->m_MsrBitmap, IA32_EFER, false);
	MsrConfig::ActivateMsrBit(&pVmm->m_MsrBitmap, IA32_LSTAR, true);*/

	for (uint32_t i = 0; i < pVmm->m_iCoreCount; i++)
	{
		pVmm->m_pCpuArray[i] = AllocateCpu(pVmm);
	}

	/*
		This is related to copying all the modules, so the host can execute itself
		and to use some of self-containing functions which are defined in the
		ntoskrnl.exe. Calling most of the functions from the host with custom
		paging set will cause the VMM to crash.

		It's not specifically designed to allow usage of the all Windows OS capabilities
		in the host, but rather to copy some translations in the kernel to the VMM...

		The VMM translations are copied in the ConstructHostPaging function call. This
		is crucial, because the driver of the VMM contains it's VM Exit Stub and Handlers.
		It should be enough to handle basic handling of Exiting unless any API of the
		system is used. Though, it should not be used anyway...

		This is the main limitation of type 2 hypervisor, as if UEFI was used It would've
		been possible to allocate some memory for the VMM in physical memory, define own
		GDT and IDT, all of the ISR's and make it as a standalone not dependent on any
		subsequent OS Kernel Virtual Monitor.
	*/

	/*VmmConfig::ConstructModulePaging(pVmm, "ntoskrnl.exe");
	VmmConfig::ConstructModulePaging(pVmm, "GIHF.sys");
	VmmConfig::ConstructHostPaging(pVmm);*/
	/*VmxExtendedPaging::VmHideVmm(pVmm->m_pCpuArray[0]);
	__debugbreak();*/
	KeIpiGenericCall(static_cast<PKIPI_BROADCAST_WORKER>(setup_virtual_cpu), (ULONG_PTR)pVmm);
	return STATUS_SUCCESS;
}

EXTERN_C NTSTATUS DriverEntry(uint64_t ImageBase)
{
	HANDLE hThread{ 0 };
	if (!NT_SUCCESS(PsCreateSystemThread(&hThread, GENERIC_ALL, nullptr, nullptr, nullptr, (PKSTART_ROUTINE)MainEntry, nullptr)))
		return STATUS_UNSUCCESSFUL;

	return STATUS_SUCCESS;
}