#include <stdafx.h>

/*
*	This won't make a complete watching behaviour that re-enters
*	the state where the page is constantly monitored through the
*	execute disable bit in EPT entry.
* 
*	Once the bit was cleared and guest resumed "breakpoint" is
*	gone.
* 
*	Proper implementation requires a copy of the page. One page
*	will be marked as the execute only and the other as write
*	and read. For that, I can't comprehend how it must be done.
*/

uint16_t VmExitHandlers::VmHandleEptViolation(PGUEST_STATE pGuestState)
{
	VMX_EXIT_QUALIFICATION_EPT_VIOLATION EptViolation{ 0 };
	INVEPT_DESCRIPTOR InvDescriptor{ 0 };
	PNMI_DATA pNmiItem{ 0 };
	PVIRTUAL_CPU pCurrentCpu{ 0 };

	EptViolation.Flags = pGuestState->m_iExitQualification;
	pNmiItem = reinterpret_cast<PNMI_DATA>(__readfsqword(0x0));
	pCurrentCpu = reinterpret_cast<PVIRTUAL_CPU>(__readfsqword(0x8));

	if (EptViolation.ExecuteAccess)
		__vmx_vmwrite(VMCS_CTRL_EPT_POINTER, pCurrentCpu->m_pEptGuestPaging[0]->m_pEptTranslation.Flags);
	else /*if (EptViolation.ReadAccess || EptViolation.WriteAccess)*/
		__vmx_vmwrite(VMCS_CTRL_EPT_POINTER, pCurrentCpu->m_pEptGuestPaging[1]->m_pEptTranslation.Flags);
	
	pGuestState->m_bShouldIncrementRip = FALSE;

	return EPT_VIOLATION;
}