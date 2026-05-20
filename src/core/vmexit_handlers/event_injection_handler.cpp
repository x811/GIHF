#include <stdafx.h>

void VmExitHandlers::VmInjectEvent(uint16_t InterruptType, uint16_t EventType)
{
	VMEXIT_INTERRUPT_INFORMATION interrupt_info{ 0 };

	interrupt_info.InterruptionType = InterruptType;
	interrupt_info.Vector = EventType;
	interrupt_info.Valid = TRUE;
	
	__vmx_vmwrite(VMCS_CTRL_VMENTRY_INTERRUPTION_INFORMATION_FIELD, interrupt_info.Flags);
}