#include <stdafx.h>

/*
*	Software must account for a different address space of the
*	program that is trying to communicate, so that parameters
*	can not be (possibly) acquired safely without switching
*	context.
*/

uint16_t VmExitHandlers::VmCommunicationHandler(PGUEST_STATE pGuestState)
{
	if (pGuestState->m_pGuestRegisters->rcx != COMM_CODE0)
	{
		pGuestState->m_bShouldInjectEvent = TRUE;
		pGuestState->m_iEventVector = InterruptVectors::INTERRUPT_TYPE_HARDWARE_EXCEPTION;
		pGuestState->m_iEventType = EventTypes::EXCEPTION_UNDEFINED_OPCODE;
		return VMCALL;
	}

	/*
	*	This issues "NMI" to all other cores and enters the same IDT NMI
	*	handler in order to perform Hiding of Pages through EPT, the EPT
	*	structs are defined per core, so it adds some layer of safety from
	*	Race Conditions that were happening when it was using a global
	*	EPT Paging structure.
	*
	*	Code duplication is bad!!!
	*/

	if (pGuestState->m_pGuestRegisters->rdx != COMM_USER_COMM_START)
	{
		pGuestState->m_bShouldInjectEvent = TRUE;
		pGuestState->m_iEventVector = InterruptVectors::INTERRUPT_TYPE_HARDWARE_EXCEPTION;
		pGuestState->m_iEventType = EventTypes::EXCEPTION_UNDEFINED_OPCODE;
		return VMCALL;
	}

	pGuestState->m_pGuestRegisters->rax = COMM_ACKNOLEDGE_GUEST; // Constant for OK respone from HV to an app

	/*
	*	This is where user supplies his own buffer to the hypervisor for
	*	further processing.
	*
	*	Using this code passed in RDX register is mandatory. R8 will
	*	supply the PID of the user process and R9 will supply the address of
	*	user page to communicate.
	* 
	*	If the type of a buffer is a string then, the string must be
	*	ANSI string, not a UNICODE one.
	*/
	PVIRTUAL_CPU pCurrentCpu{ nullptr };
	PCOMMON_PACKET pCommonPacket{ nullptr };

	pCurrentCpu = reinterpret_cast<PVIRTUAL_CPU>(__readfsqword(0x8));
	pCurrentCpu->m_pCommPage->m_sPageEntry->PageFrameNumber = VmxUtils::FindUserCommPhysicalPage(pGuestState->m_pGuestRegisters->r8, pGuestState->m_pGuestRegisters->r9) >> PAGE_SHIFT;
	__invlpg(pCurrentCpu->m_pCommPage->m_pPage);

	/* At this point user's application sees the same data as the hypervisor does. It's allowed to change memory */
	pCommonPacket = reinterpret_cast<PCOMMON_PACKET>(pCurrentCpu->m_pCommPage->m_pPage);

	/* Actual memory interaction can happen here now */

	switch (pCommonPacket->m_iMessageId)
	{
	case 0x4:
	{
		pGuestState->m_pVmmCtx->m_pNmiWorkerItem->m_bSendByHost = true;
		pGuestState->m_pVmmCtx->m_pNmiWorkerItem->m_iFuncCode = FuncCodes::FUNC_ID_HIDE_SELF;
		pGuestState->m_pVmmCtx->m_pNmiWorkerItem->m_bTlbFlush = true;
		xApicConfig::TriggerNmi();
		software_nmi();
		RtlZeroMemory(pGuestState->m_pVmmCtx->m_pNmiWorkerItem, sizeof(NMI_DATA));

		PBUFFER_PACKET pBuffer = reinterpret_cast<PBUFFER_PACKET>(pCommonPacket);

		ANSI_STRING str{ 0 };
		RtlInitAnsiString(&str, "Driver is hidden!");

		RtlCopyMemory(pBuffer->m_sMessageBuffer, str.Buffer, str.Length);

		break;
	}
	case 0x5:
	{
		pGuestState->m_pVmmCtx->m_pNmiWorkerItem->m_bSendByHost = true;
		pGuestState->m_pVmmCtx->m_pNmiWorkerItem->m_bTlbFlush = true;
		pGuestState->m_pVmmCtx->m_pNmiWorkerItem->m_iFuncCode = FuncCodes::FUNC_ID_HOOK_PAGE;
		//pGuestState->m_pVmmCtx->m_pNmiWorkerItem->m_iFuncArgs[0] = pGuestState->m_pGuestRegisters->r8;
		xApicConfig::TriggerNmi();
		software_nmi();
		RtlZeroMemory(pGuestState->m_pVmmCtx->m_pNmiWorkerItem, sizeof(NMI_DATA));
		break;
	}
	case 0x6:
	{
		PBUFFER_PACKET pBuffer = reinterpret_cast<PBUFFER_PACKET>(pCommonPacket);

		ANSI_STRING str{ 0 };
		RtlInitAnsiString(&str, "This is badass as fuck!");

		RtlCopyMemory(pBuffer->m_sMessageBuffer, str.Buffer, str.Length);
	}
		break;
	default:
		pGuestState->m_pGuestRegisters->rax = MAXUINT64;
		break;
	}

	pCurrentCpu->m_pCommPage->m_sPageEntry->PageFrameNumber = pCurrentCpu->m_pCommPage->m_iPhysicalAddress >> PAGE_SHIFT;
	__invlpg(pCurrentCpu->m_pCommPage->m_pPage);

	return VMCALL;
}