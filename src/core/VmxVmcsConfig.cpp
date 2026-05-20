#include "stdafx.h"

#define VmxVmwriteFieldFromRegister(_FIELD_DEFINE_, _REGISTER_VAR_) \
	VmStatus |= __vmx_vmwrite(_FIELD_DEFINE_, _REGISTER_VAR_.Flags);

#define VmxVmwriteFieldFromImmediate(_FIELD_DEFINE_, _IMM_VAL_) \
	VmStatus |= __vmx_vmwrite(_FIELD_DEFINE_, _IMM_VAL_)

#define VMCS_HOST_SEGMENT_NO_BASE(_VMX_SEGMENT_NAME_, _VMX_DESCRIPTOR_STRUCTURE_VALUE_) \
	CaptureAllSegmentInfo(gdt.BaseAddress, _VMX_DESCRIPTOR_STRUCTURE_VALUE_, &VmxDescriptor, true); \
	VmxVmwriteFieldFromImmediate(VMCS_HOST_##_VMX_SEGMENT_NAME_##_SELECTOR, VmxDescriptor.m_sSelector.Flags);

#define VMCS_HOST_SEGMENT(_VMX_SEGMENT_NAME_, _VMX_DESCRIPTOR_STRUCTURE_VALUE_) \
	CaptureAllSegmentInfo(gdt.BaseAddress, _VMX_DESCRIPTOR_STRUCTURE_VALUE_, &VmxDescriptor, true); \
	VmxVmwriteFieldFromImmediate(VMCS_HOST_##_VMX_SEGMENT_NAME_##_SELECTOR, VmxDescriptor.m_sSelector.Flags); \
	VmxVmwriteFieldFromImmediate(VMCS_HOST_##_VMX_SEGMENT_NAME_##_BASE, VmxDescriptor.m_iBase);

#define VMCS_GUEST_SEGMENT(VMX_SEGMENT_NAME_, _VMX_DESCRIPTOR_STRUCTURE_VALUE_) \
	CaptureAllSegmentInfo(gdt.BaseAddress, _VMX_DESCRIPTOR_STRUCTURE_VALUE_, &VmxDescriptor, false); \
	VmxVmwriteFieldFromImmediate(VMCS_GUEST_##VMX_SEGMENT_NAME_##_SELECTOR, VmxDescriptor.m_sSelector.Flags); \
	VmxVmwriteFieldFromImmediate(VMCS_GUEST_##VMX_SEGMENT_NAME_##_BASE, VmxDescriptor.m_iBase); \
	VmxVmwriteFieldFromImmediate(VMCS_GUEST_##VMX_SEGMENT_NAME_##_LIMIT, VmxDescriptor.m_iLimit); \
	VmxVmwriteFieldFromImmediate(VMCS_GUEST_##VMX_SEGMENT_NAME_##_ACCESS_RIGHTS, VmxDescriptor.m_sAccessRights.Flags);

/*
* Segmentation Related
*/

void CaptureAllSegmentInfo(const uint64_t gdtr_base, uint16_t selector, VMX_SEGMENT_DESCRIPTOR* pVmxDescriptor, bool is_host_segment)
{
	uint64_t BaseAddress;
	uint32_t Limit;

	SEGMENT_DESCRIPTOR_32* SegmentDescriptor{ 0 };
	SEGMENT_DESCRIPTOR_32* GlobalDescriptorTable{ 0 };

	RtlSecureZeroMemory(pVmxDescriptor, sizeof(VMX_SEGMENT_DESCRIPTOR));

	pVmxDescriptor->m_sSelector.Flags = selector;

	if (pVmxDescriptor->m_sSelector.Flags == 0 || pVmxDescriptor->m_sSelector.Table != 0)
	{
		pVmxDescriptor->m_sAccessRights.Unusable = 1;
		return;
	}

	GlobalDescriptorTable = reinterpret_cast<SEGMENT_DESCRIPTOR_32*>(gdtr_base);
	SegmentDescriptor = &GlobalDescriptorTable[pVmxDescriptor->m_sSelector.Index];

	pVmxDescriptor->m_iBase = (SegmentDescriptor->BaseAddressLow |
		SegmentDescriptor->BaseAddressMiddle << 16 |
		SegmentDescriptor->BaseAddressHigh << 24) & MAXUINT32;

	if (SegmentDescriptor->DescriptorType == 0)
	{
		SEGMENT_DESCRIPTOR_64* pExtendedDescriptor = reinterpret_cast<SEGMENT_DESCRIPTOR_64*>(SegmentDescriptor);
		pVmxDescriptor->m_iBase |= (uint64_t)pExtendedDescriptor->BaseAddressUpper << 32;
	}

	pVmxDescriptor->m_iLimit = __segmentlimit(selector);

	pVmxDescriptor->m_sAccessRights.Flags = (__load_ar(selector) >> 8);

	if (is_host_segment)
	{
		pVmxDescriptor->m_sSelector.RequestPrivilegeLevel = 0;
		pVmxDescriptor->m_sSelector.Table = 0;
	}

	pVmxDescriptor->m_sAccessRights.Unusable = 0;
	pVmxDescriptor->m_sAccessRights.Reserved1 = 0;
	pVmxDescriptor->m_sAccessRights.Reserved2 = 0;
}

/*
 * VMCS Control Related Fields
 */

uint64_t VmcsConfig::HvEncodeFieldBits(uint64_t ControlRegisterValue, uint64_t MsrToReadFrom)
{
	LARGE_INTEGER MsrToReadFromValue;

	MsrToReadFromValue.QuadPart = MsrToReadFrom;

	ControlRegisterValue |= MsrToReadFromValue.LowPart;
	ControlRegisterValue &= MsrToReadFromValue.HighPart;

	return ControlRegisterValue;
}

IA32_VMX_PINBASED_CTLS_REGISTER VmcsConfig::HvSetupPinBasedControls(const PVMM_CONTEXT pVmm)
{
	uint64_t Msr;
	IA32_VMX_PINBASED_CTLS_REGISTER PinBasedRegister;

	PinBasedRegister.Flags = 0;
	PinBasedRegister.NmiExiting = 1;
	//PinBasedRegister.VirtualNmi = 1;

	if (pVmm->m_bTrueControls)
	{
		Msr = __readmsr(IA32_VMX_TRUE_PINBASED_CTLS);
	}
	else
	{
		Msr = __readmsr(IA32_VMX_PINBASED_CTLS);
	}

	PinBasedRegister.Flags = HvEncodeFieldBits(PinBasedRegister.Flags, Msr);
	return PinBasedRegister;
}

IA32_VMX_PROCBASED_CTLS_REGISTER VmcsConfig::HvSetupPrimaryProcessorControls(const PVMM_CONTEXT pVmm)
{
	uint64_t Msr;
	IA32_VMX_PROCBASED_CTLS_REGISTER ProcBasedRegister;

	ProcBasedRegister.Flags = 0;

	ProcBasedRegister.ActivateSecondaryControls = 1;
	ProcBasedRegister.UseMsrBitmaps = 1;
	//ProcBasedRegister.NmiWindowExiting = 1;

	if (pVmm->m_bTrueControls)
	{
		Msr = __readmsr(IA32_VMX_TRUE_PROCBASED_CTLS);
	}
	else
	{
		Msr = __readmsr(IA32_VMX_PROCBASED_CTLS);
	}

	ProcBasedRegister.Flags = HvEncodeFieldBits(ProcBasedRegister.Flags, Msr);
	return ProcBasedRegister;
}

IA32_VMX_PROCBASED_CTLS2_REGISTER VmcsConfig::HvSetupSecondaryProcessorControls(const PVMM_CONTEXT pVmm)
{
	uint64_t MsrId;
	IA32_VMX_PROCBASED_CTLS2_REGISTER SecondaryBasedRegister;

	SecondaryBasedRegister.Flags = 0;

	SecondaryBasedRegister.ConcealVmxFromPt = 1;
	SecondaryBasedRegister.EnableRdtscp = 1;
	SecondaryBasedRegister.EnableXsaves = 1;
	SecondaryBasedRegister.EnableInvpcid = 1;
	SecondaryBasedRegister.EnableVpid = 1;
	SecondaryBasedRegister.EnableEpt = 1;

	/*SecondaryBasedRegister.ApicRegisterVirtualization = 1;
	SecondaryBasedRegister.VirtualInterruptDelivery = 1;
	SecondaryBasedRegister.VirtualizeX2ApicMode = 1;*/

	SecondaryBasedRegister.Flags = HvEncodeFieldBits(SecondaryBasedRegister.Flags, __readmsr(IA32_VMX_PROCBASED_CTLS2));
	return SecondaryBasedRegister;
}

IA32_VMX_ENTRY_CTLS_REGISTER VmcsConfig::HvSetupEntryControls(const PVMM_CONTEXT pVmm)
{
	uint64_t Msr;
	IA32_VMX_ENTRY_CTLS_REGISTER EntryBasedRegister;

	EntryBasedRegister.Flags = 0;

	EntryBasedRegister.ConcealVmxFromPt = 1;
	EntryBasedRegister.Ia32EModeGuest = 1;

	if (pVmm->m_bTrueControls)
	{
		Msr = __readmsr(IA32_VMX_TRUE_ENTRY_CTLS);
	}
	else
	{
		Msr = __readmsr(IA32_VMX_ENTRY_CTLS);
	}

	EntryBasedRegister.Flags = HvEncodeFieldBits(EntryBasedRegister.Flags, Msr);
	return EntryBasedRegister;
}

IA32_VMX_EXIT_CTLS_REGISTER VmcsConfig::HvSetupExitControls(const PVMM_CONTEXT pVmm)
{
	uint64_t MsrId;
	IA32_VMX_EXIT_CTLS_REGISTER ExitBasedRegister;

	ExitBasedRegister.Flags = 0;

	ExitBasedRegister.ConcealVmxFromPt = 1;
	ExitBasedRegister.HostAddressSpaceSize = 1;

	if (pVmm->m_bTrueControls)
	{
		MsrId = __readmsr(IA32_VMX_TRUE_EXIT_CTLS);
	}
	else
	{
		MsrId = __readmsr(IA32_VMX_EXIT_CTLS);
	}

	ExitBasedRegister.Flags = HvEncodeFieldBits(ExitBasedRegister.Flags, MsrId);
	return ExitBasedRegister;
}

uint32_t VmcsConfig::HvSetupGuestArea(uint64_t guest_rip, uint64_t guest_rsp)
{
	uint32_t VmStatus = 0;

	SEGMENT_DESCRIPTOR_REGISTER_64 gdt{ 0 }, idt{ 0 };
	_sgdt(&gdt);
	__sidt(&idt);

	VMX_SEGMENT_DESCRIPTOR VmxDescriptor{ 0 };

	VmxVmwriteFieldFromImmediate(VMCS_GUEST_CR0, __readcr0());
	VmxVmwriteFieldFromImmediate(VMCS_GUEST_CR3, __readcr3());
	VmxVmwriteFieldFromImmediate(VMCS_GUEST_CR4, __readcr4());

	VmxVmwriteFieldFromImmediate(VMCS_GUEST_DR7, __readdr(7));

	VmxVmwriteFieldFromImmediate(VMCS_GUEST_RFLAGS, __readeflags());
	
	VmxVmwriteFieldFromImmediate(VMCS_GUEST_RIP, guest_rip);
	VmxVmwriteFieldFromImmediate(VMCS_GUEST_RSP, guest_rsp);

	VMCS_GUEST_SEGMENT(CS, __read_cs());
	VMCS_GUEST_SEGMENT(ES, __read_es());
	VMCS_GUEST_SEGMENT(DS, __read_ds());
	VMCS_GUEST_SEGMENT(SS, __read_ss());
	VMCS_GUEST_SEGMENT(FS, __read_fs());
	VMCS_GUEST_SEGMENT(GS, __read_gs());
	VMCS_GUEST_SEGMENT(TR, __read_tr());
	VMCS_GUEST_SEGMENT(LDTR, __read_ldtr());

	VmxVmwriteFieldFromImmediate(VMCS_GUEST_FS_BASE, __readmsr(IA32_FS_BASE));
	VmxVmwriteFieldFromImmediate(VMCS_GUEST_GS_BASE, __readmsr(IA32_GS_BASE));

	VmxVmwriteFieldFromImmediate(VMCS_GUEST_GDTR_BASE, gdt.BaseAddress);
	VmxVmwriteFieldFromImmediate(VMCS_GUEST_IDTR_BASE, idt.BaseAddress);

	VmxVmwriteFieldFromImmediate(VMCS_GUEST_GDTR_LIMIT, gdt.Limit);
	VmxVmwriteFieldFromImmediate(VMCS_GUEST_IDTR_LIMIT, idt.Limit);

	VmxVmwriteFieldFromImmediate(VMCS_GUEST_DEBUGCTL, __readmsr(IA32_DEBUGCTL));
	VmxVmwriteFieldFromImmediate(VMCS_GUEST_SYSENTER_CS, __readmsr(IA32_SYSENTER_CS));
	VmxVmwriteFieldFromImmediate(VMCS_GUEST_SYSENTER_EIP, __readmsr(IA32_SYSENTER_EIP));
	VmxVmwriteFieldFromImmediate(VMCS_GUEST_SYSENTER_ESP, __readmsr(IA32_SYSENTER_ESP));

	VmxVmwriteFieldFromImmediate(VMCS_GUEST_VMCS_LINK_POINTER, MAXUINT64);
	VmxVmwriteFieldFromImmediate(VMCS_GUEST_ACTIVITY_STATE, 0);
	VmxVmwriteFieldFromImmediate(VMCS_GUEST_INTERRUPTIBILITY_STATE, 0);
	VmxVmwriteFieldFromImmediate(VMCS_GUEST_PENDING_DEBUG_EXCEPTIONS, 0);

	return VmStatus;
}

/*
	The problem in the host VMCS configuration lies in the separation of guest from host
	on already running virtual machine, if we make our own paging it needs all kinds of
	VA to PA mapping (e.g. IDT table and GDT table to use interrupts in host, for example
	'int 3' instruction which is interrupt of a vector 3 in IDT) which is hard to track
	because Windows OS has a ton of runtime allocations that are (possibly) not available
	to us to track and map...

	This CR3 suffices the bare minimum to separate guest paging from the host paging so
	that guest won't crash on VMExit unless we use Windows API Functions or interrupt
	instructions in our VMExitHandler.

	We can add support for the IDT and GDT but I doubt that we can use it normally, as
	internally it calls other functions which need to be mapped to a host physical memory.
*/

uint32_t VmcsConfig::HvSetupHostArea(const PVIRTUAL_CPU pCpu, uint64_t host_rip, uint64_t host_rsp)
{
	uint32_t VmStatus = 0;

	SEGMENT_DESCRIPTOR_REGISTER_64 gdt{ 0 }, idt{ 0 };
	_sgdt(&gdt);
	__sidt(&idt);

	VMX_SEGMENT_DESCRIPTOR VmxDescriptor{ 0 };

	VmxVmwriteFieldFromImmediate(VMCS_HOST_CR0, __readcr0());
	VmxVmwriteFieldFromImmediate(VMCS_HOST_CR3, MmGetPhysicalAddress(pCpu->m_pHostPagingBase).QuadPart/*__readcr3()*/);
	VmxVmwriteFieldFromImmediate(VMCS_HOST_CR4, __readcr4());

	/* This is also heavy task to resolve those addresses that map to physical addresses from entries */
	VMCS_HOST_SEGMENT_NO_BASE(CS, __read_cs());
	VMCS_HOST_SEGMENT_NO_BASE(ES, __read_es());
	VMCS_HOST_SEGMENT_NO_BASE(DS, __read_ds());
	VMCS_HOST_SEGMENT_NO_BASE(SS, __read_ss());
	VMCS_HOST_SEGMENT(FS, __read_fs());
	VMCS_HOST_SEGMENT(GS, __read_gs());
	VMCS_HOST_SEGMENT(TR, __read_tr());
	
	VmxVmwriteFieldFromImmediate(VMCS_HOST_TR_BASE, (uint64_t)pCpu->m_pCpuTss);

	VmxVmwriteFieldFromImmediate(VMCS_HOST_RIP, host_rip);
	VmxVmwriteFieldFromImmediate(VMCS_HOST_RSP, host_rsp);
	
	/*
		FS base is actually 0 on Windows. Anyway, because it's completely isolates guest from host
		I can use almost any register for storing my own information as I want to if Intel doesn't
		state that otherwise...
	*/

	VmxVmwriteFieldFromImmediate(VMCS_HOST_FS_BASE, (uint64_t)pCpu->m_pCpuFSBase);
	VmxVmwriteFieldFromImmediate(VMCS_HOST_GS_BASE, __readmsr(IA32_GS_BASE));

	/*
	*	This maps own hand-crafted GDT and IDT to be used by the software in Root Mode
	*/
	VmxVmwriteFieldFromImmediate(VMCS_HOST_GDTR_BASE, (uint64_t)pCpu->m_pGlobalDescriptorTable);
	VmxVmwriteFieldFromImmediate(VMCS_HOST_IDTR_BASE, (uint64_t)pCpu->m_pInterruptTable);

	VmxVmwriteFieldFromImmediate(VMCS_HOST_SYSENTER_CS, __readmsr(IA32_SYSENTER_CS));
	VmxVmwriteFieldFromImmediate(VMCS_HOST_SYSENTER_EIP, __readmsr(IA32_SYSENTER_EIP));
	VmxVmwriteFieldFromImmediate(VMCS_HOST_SYSENTER_ESP, __readmsr(IA32_SYSENTER_ESP));
	
	return VmStatus;
}

uint32_t VmcsConfig::HvSetupControlArea(const PVIRTUAL_CPU pCpu)
{
	uint32_t VmStatus = 0;

	//__debugbreak();
	VmxVmwriteFieldFromRegister(VMCS_CTRL_PIN_BASED_VM_EXECUTION_CONTROLS, HvSetupPinBasedControls(pCpu->m_pVmmBacklink));
	VmxVmwriteFieldFromRegister(VMCS_CTRL_PROCESSOR_BASED_VM_EXECUTION_CONTROLS, HvSetupPrimaryProcessorControls(pCpu->m_pVmmBacklink));
	VmxVmwriteFieldFromRegister(VMCS_CTRL_VMEXIT_CONTROLS, HvSetupExitControls(pCpu->m_pVmmBacklink));
	VmxVmwriteFieldFromRegister(VMCS_CTRL_VMENTRY_CONTROLS, HvSetupEntryControls(pCpu->m_pVmmBacklink));
	VmxVmwriteFieldFromRegister(VMCS_CTRL_SECONDARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS, HvSetupSecondaryProcessorControls(pCpu->m_pVmmBacklink));

	VmxVmwriteFieldFromImmediate(VMCS_CTRL_MSR_BITMAP_ADDRESS, MmGetPhysicalAddress(pCpu->m_pVmmBacklink->m_MsrBitmap.m_pMsrBitmap).QuadPart);
	
	/*
	*	EPTP is explicitly set to the one that allows code modification with Execute Only bit.
	*/
	VmxVmwriteFieldFromImmediate(VMCS_CTRL_EPT_POINTER, pCpu->m_pEptGuestPaging[0]->m_pEptTranslation.Flags);

	/*
	*	This is future work, I can't comprehend the effects of full virtualization of CRs
	*/

	/*VmxVmwriteFieldFromImmediate(VMCS_CTRL_CR0_GUEST_HOST_MASK, __readcr0());
	VmxVmwriteFieldFromImmediate(VMCS_CTRL_CR0_READ_SHADOW, __readcr0());

	VmxVmwriteFieldFromImmediate(VMCS_CTRL_CR4_GUEST_HOST_MASK, __readcr4());
	VmxVmwriteFieldFromImmediate(VMCS_CTRL_CR4_READ_SHADOW, __readcr4() & ~(1 << CR4_VMX_ENABLE_BIT));*/
	
	//VmxVmwriteFieldFromImmediate(VMCS_CTRL_VIRTUAL_APIC_ADDRESS, MmGetPhysicalAddress(pCpu->m_pApicPage).QuadPart);
	
	VmxVmwriteFieldFromImmediate(VMCS_CTRL_VIRTUAL_PROCESSOR_IDENTIFIER, 1);

	return VmStatus;
}

NTSTATUS VmcsConfig::SwitchToVmxAndSetLoadVmcs(PVIRTUAL_CPU pCpu)
{
	uint64_t VmxonPhysical = MmGetPhysicalAddress(pCpu->m_pVmxonRegion).QuadPart;
	uint64_t VmcsPhysical = MmGetPhysicalAddress(pCpu->m_pVmcsRegion).QuadPart;

	if (!VmxDeps::set_vmxe())
		return STATUS_UNSUCCESSFUL;

	if (!VmxDeps::set_fixed_bits())
		return STATUS_UNSUCCESSFUL;

	if(!VmxDeps::IsExtendedApic())
		return STATUS_UNSUCCESSFUL;

	if (__vmx_on(&VmxonPhysical) != NULL)
		return STATUS_UNSUCCESSFUL;

	if (__vmx_vmclear(&VmcsPhysical) != NULL)
		return STATUS_UNSUCCESSFUL;

	if (__vmx_vmptrld(&VmcsPhysical) != NULL)
		return STATUS_UNSUCCESSFUL;

	return STATUS_SUCCESS;
}

NTSTATUS VmcsConfig::HvSetupVmcsStructure(PVIRTUAL_CPU pCpu, uint64_t host_rip, uint64_t host_rsp, uint64_t guest_rip, uint64_t guest_rsp)
{
	uint32_t VmStatus = 0;

	VmStatus |= HvSetupGuestArea(guest_rip, guest_rsp);

	if (VmStatus != 0)
	{
		Log("Failed to Setup VMCS Guest Area\n");
		return STATUS_UNSUCCESSFUL;
	}

	VmStatus |= HvSetupHostArea(pCpu, host_rip, host_rsp);

	if (VmStatus != 0)
	{
		Log("Failed to Setup VMCS Host Area\n");
		return STATUS_UNSUCCESSFUL;
	}

	VmStatus |= HvSetupControlArea(pCpu);

	if (VmStatus != 0)
	{
		Log("Failed To Setup VMCS Control Area\n");
		return STATUS_UNSUCCESSFUL;
	}

	return STATUS_SUCCESS;
}
