#include <stdafx.h>

uint16_t VmExitHandlers::VmExternalInterrupt(PGUEST_STATE pGuestState)
{
	VMEXIT_INTERRUPT_INFORMATION VmxExternalInterrupt{ 0 };

	
	__vmx_vmread(VMCS_VMEXIT_INTERRUPTION_INFORMATION, (size_t*) & VmxExternalInterrupt.Flags);

	pGuestState->m_bShouldIncrementRip = FALSE;

	return EXIT_REASONS::EXTERNAL_INTERRUPT;
}