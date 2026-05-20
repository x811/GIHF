#include <stdafx.h>

#include "exit_handlers.h"

bool VmIsXcrValid(uint64_t Xcr0)
{
	if (!(Xcr0 & X86_XCR0_FP))
		return false;

	if ((Xcr0 & X86_XCR0_YMM) && !(Xcr0 & X86_XCR0_SSE))
		return false;

	if ((!(Xcr0 & X86_XCR0_BNDREGS)) != (!(Xcr0 & X86_XCR0_BNDCSR)))
		return false;

	if (Xcr0 & XSTATE_MASK_AVX512) {

		// OPMASK, ZMM, and HI_ZMM require YMM.
		if (!(Xcr0 & X86_XCR0_YMM))
			return false;

		// OPMASK, ZMM, and HI_ZMM must be the same.
		if (~Xcr0 & (X86_XCR0_OPMASK | X86_XCR0_ZMM | X86_XCR0_HI_ZMM))
			return false;
	}

	return true;
}

bool VmIsXcrSupported(uint64_t Xcr0)
{
	int32_t regs[4];

	__cpuidex(regs, 0xD, 0);

	if ((Xcr0 & ~regs[0]) != 0)
		return false;

	/* This is Windows 10 supported mask */
	if ((Xcr0 & ~0x7) != 0)
		return false;

	return true;
}

/*
*	This is (probably) not prone to crashing from Rootkits.
*/

uint16_t VmExitHandlers::VmHandleXsetbv(PGUEST_STATE pGuestState)
{
	LARGE_INTEGER value_to_write{ 0 };

	value_to_write.LowPart = pGuestState->m_pGuestRegisters->rax;
	value_to_write.HighPart = pGuestState->m_pGuestRegisters->rdx;

	switch (pGuestState->m_pGuestRegisters->rcx)
	{
	case 0:
		break;
	default:
GPFAULT:
		pGuestState->m_bShouldInjectEvent = true;
		pGuestState->m_iEventType = EventTypes::EXCEPTION_GENERAL_PROTECTION_FAULT;
		pGuestState->m_iEventVector = InterruptVectors::INTERRUPT_TYPE_HARDWARE_EXCEPTION;

		pGuestState->m_bShouldIncrementRip = TRUE;
		return XSETBV;
	}

	if (!VmxUtils::VmIsCplZero())
		goto GPFAULT;

	if (!VmIsXcrSupported(value_to_write.QuadPart))
		goto GPFAULT;

	if (!VmIsXcrValid(value_to_write.QuadPart))
		goto GPFAULT;

	_xsetbv(pGuestState->m_pGuestRegisters->rcx, value_to_write.QuadPart);

	pGuestState->m_bShouldIncrementRip = TRUE;

	return XSETBV;
}