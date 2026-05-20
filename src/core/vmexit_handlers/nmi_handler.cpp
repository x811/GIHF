#include <stdafx.h>

uint16_t VmExitHandlers::VmNmiHandler(PGUEST_STATE pGuestState)
{
	if (pGuestState->m_pVmmCtx->m_pNmiWorkerItem->m_bSendByHost)
		software_nmi();
	else
		VmInjectEvent(InterruptVectors::INTERRUPT_TYPE_NMI, EventTypes::EXCEPTION_NMI_INTERRUPT);

	pGuestState->m_bShouldIncrementRip = FALSE;

	return 0x80;
}