#include <stdafx.h>

uint16_t VmExitHandlers::VmHandleCpuid(PGUEST_STATE pGuestState)
{
	int32_t regs[4];

	HvLog("CPUID Handler Called Leaf: %i SubLeaf: %i\n", pGuestState->m_pGuestRegisters->rax, pGuestState->m_pGuestRegisters->rcx);

	if (pGuestState->m_pGuestRegisters->rax >= 0x40000000 && pGuestState->m_pGuestRegisters->rax <= 0x4000000A)
	{
		HvLog("CPUID Handler stumbled upon Microsoft HV");
		VmInjectEvent(InterruptVectors::INTERRUPT_TYPE_HARDWARE_EXCEPTION, EventTypes::EXCEPTION_GENERAL_PROTECTION_FAULT);
		pGuestState->m_bShouldIncrementRip = TRUE;
		return CPUID;
	}

	__cpuidex(regs, pGuestState->m_pGuestRegisters->rax, pGuestState->m_pGuestRegisters->rcx);

	if (pGuestState->m_pGuestRegisters->rax == 1)
		regs[2] &= ~(1 << 31);

	pGuestState->m_pGuestRegisters->rax = regs[0];
	pGuestState->m_pGuestRegisters->rbx = regs[1];
	pGuestState->m_pGuestRegisters->rcx = regs[2];
	pGuestState->m_pGuestRegisters->rdx = regs[3];

	pGuestState->m_bShouldIncrementRip = TRUE;

	return CPUID;
}