#include <stdafx.h>

#define ISSUED_GPR_TO_GUEST_GPR_IDX(_ISSUED_IDX_) \
	~(_ISSUED_IDX_ - 15) + 1

#define CR0_RESERVED_BITS                                               \
	(~(unsigned long)(CR0_PAGING_ENABLE_FLAG | CR0_MONITOR_COPROCESSOR_FLAG | CR0_EMULATE_FPU_FLAG | CR0_TASK_SWITCHED_FLAG \
			  | CR0_EXTENSION_TYPE_FLAG | CR0_NUMERIC_ERROR_FLAG | CR0_WRITE_PROTECT_FLAG | CR0_ALIGNMENT_MASK_FLAG \
			  | CR0_NOT_WRITE_THROUGH_FLAG | CR0_CACHE_DISABLE_FLAG | CR0_PAGING_ENABLE_FLAG))

#define X86_CR0_PDPTR_BITS    (CR0_CACHE_DISABLE_FLAG | CR0_NOT_WRITE_THROUGH_FLAG | CR0_PAGING_ENABLE_FLAG)
#define X86_CR4_TLBFLUSH_BITS (CR4_PAGE_GLOBAL_ENABLE_FLAG | CR4_PCID_ENABLE_FLAG | CR4_PHYSICAL_ADDRESS_EXTENSION_FLAG | CR4_SMEP_ENABLE_FLAG)
#define X86_CR4_PDPTR_BITS    (CR4_PAGE_GLOBAL_ENABLE_FLAG | CR4_PAGE_SIZE_EXTENSIONS_FLAG | CR4_PHYSICAL_ADDRESS_EXTENSION_FLAG | CR4_SMEP_ENABLE_FLAG)

/* EFER bits: */
#define _EFER_SCE		0  /* SYSCALL/SYSRET */
#define _EFER_LME		8  /* Long mode enable */
#define _EFER_LMA		10 /* Long mode active (read-only) */
#define _EFER_NX		11 /* No execute enable */
#define _EFER_SVME		12 /* Enable virtualization */
#define _EFER_LMSLE		13 /* Long Mode Segment Limit Enable */
#define _EFER_FFXSR		14 /* Enable Fast FXSAVE/FXRSTOR */
#define _EFER_TCE		15 /* Enable Translation Cache Extensions */
#define _EFER_AUTOIBRS		21 /* Enable Automatic IBRS */

#define EFER_SCE		(1<<_EFER_SCE)
#define EFER_LME		(1<<_EFER_LME)
#define EFER_LMA		(1<<_EFER_LMA)
#define EFER_NX			(1<<_EFER_NX)
#define EFER_SVME		(1<<_EFER_SVME)
#define EFER_LMSLE		(1<<_EFER_LMSLE)
#define EFER_FFXSR		(1<<_EFER_FFXSR)
#define EFER_TCE		(1<<_EFER_TCE)
#define EFER_AUTOIBRS		(1<<_EFER_AUTOIBRS)

enum MOV_CR_REGISTERS
{
	RAX = 0,
	RCX,
	RDX,
	RBX,
	RSP,
	RBP,
	RSI,
	RDI,
	R8,
	R9,
	R10,
	R11,
	R12,
	R13,
	R14,
	R15,
};

enum CR_ACCESS_TYPE
{
	MOV_TO_CR = 0,
	MOV_FROM_CR,
	CLTS,
	LMSW,
};

__inline uint64_t VmRegisterReadRaw(PGUEST_REGISTERS pRegisters, int32_t Register)
{
	uint64_t* GuestRegisterArray{ nullptr };
	uint64_t ValueInRegister{ 0 };

	GuestRegisterArray = reinterpret_cast<uint64_t*>(pRegisters);
	ValueInRegister = GuestRegisterArray[ISSUED_GPR_TO_GUEST_GPR_IDX(Register)];

	return ValueInRegister;
}

/*
	This is not complete, if the Guest executes CLTS or LMSW, it will hit the breakpoint
	or it will crash if the custom CR3 is used...

	This must be done according to DrewGPF post on 'howtohypervise.blogspot.com' or maybe
	it's possible to do it the same way as KVM does it (lookup KVM 'handle_cr' in vmx.c)
*/

uint16_t VmExitHandlers::VmHandleControlRegister(PGUEST_STATE pGuestState)
{
	VMX_EXIT_QUALIFICATION_MOV_CR ExitQualification{ 0 };
	uint8_t RegisterIndex{ 0 }, CrIndex{ 0 };

	uint64_t CurrentGuestCr{ 0 };
	uint64_t Value{ 0 };

	uint64_t crx_fixed0{ 0 }, crx_fixed1{ 0 };
	uint64_t crx_required{ 0 };

	__vmx_vmread(VMCS_EXIT_QUALIFICATION, &ExitQualification.Flags);

	RegisterIndex = ExitQualification.GeneralPurposeRegister;
	CrIndex = ExitQualification.ControlRegister;

	switch (VMX_EXIT_QUALIFICATION_MOV_CR_ACCESS_TYPE(ExitQualification.Flags))
	{
	case VMX_EXIT_QUALIFICATION_ACCESS_MOV_TO_CR:
		Value = VmRegisterReadRaw(pGuestState->m_pGuestRegisters, CrIndex);

		switch (CrIndex)
		{
		case 0:

			break;
		case 4:
			
			break;
		default:
			break;
		}
		break;
	case CR_ACCESS_TYPE::CLTS:
	case CR_ACCESS_TYPE::LMSW:
		
		break;
	/*
		This needs proper handlers, otherwise it will crash
	*/
	default:
		
		break;
	}

	return CR_ACCESS;
}