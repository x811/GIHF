#include <stdafx.h>

uint16_t VmExitHandlers::VmHandleGenericVmx(PGUEST_STATE pGuestState)
{
	VmInjectEvent(InterruptVectors::INTERRUPT_TYPE_HARDWARE_EXCEPTION, EventTypes::EXCEPTION_UNDEFINED_OPCODE);

	pGuestState->m_bShouldIncrementRip = FALSE;

	return 0xDEAD;
}