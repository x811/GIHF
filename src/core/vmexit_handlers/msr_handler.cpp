#include <stdafx.h>

#include <Log.h>
#include "exit_handlers.h"

/*
*	This two handlers assume, that an MSR ID is safe,
*	but if the guest tries to pass the MSR that isn't
*	real and the VMM tries to set it instead of injecting
*	#GP Guest will know that it executes in a VM.
*/

uint16_t VmExitHandlers::VmHandleWrmsr(PGUEST_STATE pGuestState)
{
	LARGE_INTEGER value_to_write{ 0 };

	//Log("The MSR requested is: 0x%p \n", pGuestRegisters->rcx);

	value_to_write.LowPart = pGuestState->m_pGuestRegisters->rax;
	value_to_write.HighPart = pGuestState->m_pGuestRegisters->rdx;

	__writemsr(pGuestState->m_pGuestRegisters->rcx, value_to_write.QuadPart);

	pGuestState->m_bShouldIncrementRip = TRUE;

	return WRMSR;
}

uint16_t VmExitHandlers::VmHandleRdmsr(PGUEST_STATE pGuestState)
{
	LARGE_INTEGER output{ 0 };

	output.QuadPart = __readmsr(pGuestState->m_pGuestRegisters->rcx);

	//if(pGuestRegisters->rcx <= 0x1FFF || pGuestRegisters->rcx >= 0xC0000000)
	//Log("The MSR requested is: 0x%p \n", pGuestRegisters->rcx);

	pGuestState->m_pGuestRegisters->rax = output.LowPart;
	pGuestState->m_pGuestRegisters->rdx = output.HighPart;

	pGuestState->m_bShouldIncrementRip = TRUE;

	return RDMSR;
}