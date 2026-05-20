#include <stdafx.h>

#include "exit_handlers.h"

__inline void advance_guest_rip(void)
{
	uint64_t current_instruction{ 0 }, size_of_exiting_instruction{ 0 };
	__vmx_vmread(VMCS_GUEST_RIP, &current_instruction);
	__vmx_vmread(VMCS_VMEXIT_INSTRUCTION_LENGTH, &size_of_exiting_instruction);
	current_instruction += size_of_exiting_instruction;
	__vmx_vmwrite(VMCS_GUEST_RIP, current_instruction);
}

void VmBuildGuestContext(PGUEST_STATE pState, PGUEST_REGISTERS pGuestRegisters)
{
	uint64_t* pTopStackAddress = reinterpret_cast<uint64_t*>(pGuestRegisters);

	pState->m_bShouldIncrementRip = TRUE;
	pState->m_bShouldInjectEvent = FALSE;
	pState->m_bShouldExit = FALSE;
	
	pState->m_pVmmCtx = *reinterpret_cast<PVMM_CONTEXT*>(pTopStackAddress + sizeof(GUEST_REGISTERS) / sizeof(uint64_t));
	pState->m_pKprcb = reinterpret_cast<CPKPRCB>(__readgsqword(0x20));
	pState->m_pGuestRegisters = pGuestRegisters;

	__vmx_vmread(VMCS_EXIT_REASON, &pState->m_iExitReason);
	__vmx_vmread(VMCS_EXIT_QUALIFICATION, &pState->m_iExitQualification);
}

/*
*	I think the only way to run games in VM using this software
*	is to make every VM Exit that causes some type of exception
*	that the hardware generates to emulate properly and others
*	that are just to detect a VM can be (probably) ignored as
*	the software can do a myriad of ways to detect that it runs
*	in a virtualized environment, but unless it doesn't cause a
*	Guest to BSOD because of these exceptions it's fine.
*/

EXTERN_C uint16_t VmExitHandler(PGUEST_REGISTERS pGuestRegisters)
{
	uint16_t status{ 0 };

	GUEST_STATE GuestState{ 0 };
	
	VmBuildGuestContext(&GuestState, pGuestRegisters);

	uint64_t rip{0}, rflags{ 0 };
	__vmx_vmread(VMCS_GUEST_RIP, &rip);
	__vmx_vmread(VMCS_GUEST_RFLAGS, &rflags);

	HvLog("Exit Reason is: %i, RIP: %llu, RFLAGS: %i", GuestState.m_iExitReason, rip, rflags);

	switch (GuestState.m_iExitReason)
	{
	case EXIT_REASONS::NMI:
		status = VmExitHandlers::VmNmiHandler(&GuestState);
		break;
	case EXIT_REASONS::EXTERNAL_INTERRUPT:
		status = VmExitHandlers::VmExternalInterrupt(&GuestState);
		break;
	case EXIT_REASONS::CPUID:
		status = VmExitHandlers::VmHandleCpuid(&GuestState);
		break;
	case EXIT_REASONS::VMCLEAR:
	case EXIT_REASONS::VMLAUNCH:
	case EXIT_REASONS::VMOFF:
	case EXIT_REASONS::VMON:
	case EXIT_REASONS::VMPTRLD:
	case EXIT_REASONS::VMPTRST:
	case EXIT_REASONS::VMREAD:
	case EXIT_REASONS::VMRESUME:
	case EXIT_REASONS::VMWRITE:
		status = 0xDE;//VmExitHandlers::VmHandleGenericVmx(&GuestState);
		GuestState.m_bShouldInjectEvent = TRUE;
		GuestState.m_iEventType = EventTypes::EXCEPTION_UNDEFINED_OPCODE;
		GuestState.m_iEventVector = InterruptVectors::INTERRUPT_TYPE_HARDWARE_EXCEPTION;
		break;
	case EXIT_REASONS::VMCALL:
		status = VmExitHandlers::VmCommunicationHandler(&GuestState);
		break;
	case EXIT_REASONS::CR_ACCESS:
		status = VmExitHandlers::VmHandleControlRegister(&GuestState);
		break;
	case EXIT_REASONS::RDMSR:
		status = VmExitHandlers::VmHandleRdmsr(&GuestState);
		break;
	case EXIT_REASONS::WRMSR:
		status = VmExitHandlers::VmHandleWrmsr(&GuestState);
		break;
	case EXIT_REASONS::EPT_VIOLATION:
		break;
	case EXIT_REASONS::XSETBV:
		status = VmExitHandlers::VmHandleXsetbv(&GuestState);
		break;
	default:
		// TODO: Dump stack instead of 'int 3'
		HvLog("Something went wrong, reason: %i", GuestState.m_iExitReason);
		status = MAXUINT64;
		break;
	}

	if(GuestState.m_bShouldIncrementRip)
		advance_guest_rip();

	if (GuestState.m_bShouldInjectEvent)
		VmExitHandlers::VmInjectEvent(GuestState.m_iEventVector, GuestState.m_iEventType);

	return status;
}