#include <stdafx.h>

/*
*	Invalidates Host EPT translation cache. It does invalidate only
*	single EPT context. This is sufficient for that virtual machine
*	as it's not meant to be used as a "general use Virtual Machine".
*/

bool InvalidateHostCacheEpt(void)
{
	INVEPT_DESCRIPTOR InveptDesc{ 0 };
	PEPT_GUEST_PAGING pCurrentEptStruct{ nullptr };
	
	pCurrentEptStruct = reinterpret_cast<PEPT_GUEST_PAGING>(__readfsqword(0x10));
	InveptDesc.EptPointer = pCurrentEptStruct->m_pEptTranslation.Flags;
	
	if (__invept(1, &InveptDesc) != NULL)
		return false;

	return true;
}

/*
*	CPU cores are locked in between the actual operation to allow
*	them to enter and exit "NMI" (software or hardware) approximately
*	at the same time so that operations in the actual "NMI" (software
*	or hardware) is synchronized (or at least behaves the same way).
*/

/*
*	This is only a single threaded communication method with the user
*	application.
*/

void IsrImpl::HvImplNmiInterrupt(void)
{
	PNMI_DATA pNmiItem = reinterpret_cast<PNMI_DATA>(__readfsqword(0x0));
	PVIRTUAL_CPU pCurrentCpu = reinterpret_cast<PVIRTUAL_CPU>(__readfsqword(0x8));
	//CPKPRCB pCurrentPrcb = reinterpret_cast<CPKPRCB>(__readgsqword(0x20));

	InterlockedIncrement64((volatile LONG64*)&pNmiItem->m_iArrivedCount);

	while (pNmiItem->m_iArrivedCount != pCurrentCpu->m_iCoreCount)
		_mm_pause();

	switch (pNmiItem->m_iFuncCode)
	{
	case FuncCodes::FUNC_ID_HIDE_SELF:
		if (!pCurrentCpu->m_pVmmBacklink->m_bMappedDriver)
			break;

		VmxExtendedPaging::VmHideVmm(pCurrentCpu);
		break;
	case FuncCodes::FUNC_ID_HOOK_PAGE:
		VmxExtendedPaging::VmHookPage(pCurrentCpu, pNmiItem->m_iFuncArgs[0]);
		break;
	case FuncCodes::FUNC_ID_ESTABLISH_COMM:
	{
		//uint64_t pCommPageAddr{ 0 };
		//PCOMMON_PACKET pCommPage{ nullptr };
		//
		//pCommPageAddr = pNmiItem->m_iFuncArgs[0];

		//pCurrentCpu->m_pCommPage->m_sPageEntry->PageFrameNumber = pCommPageAddr >> PAGE_SHIFT;
		//__invlpg(pCurrentCpu->m_pCommPage->m_pPage);

		///* At this point user's application sees the same data as the hypervisor does. It's allowed to change memory */
		//pCommPage = reinterpret_cast<PCOMMON_PACKET>(pCurrentCpu->m_pCommPage->m_pPage);

		//pCommPage->m_iMessageId = 0x22; // User actually sees it

		//
		//pCurrentCpu->m_pCommPage->m_sPageEntry->PageFrameNumber = pCurrentCpu->m_pCommPage->m_iPhysicalAddress;
		//__invlpg(pCurrentCpu->m_pCommPage->m_pPage);
		//break;
	}
	default:
		break;
	}
	
	if (pNmiItem->m_bTlbFlush)
		if (!InvalidateHostCacheEpt())
			InvalidateHostCacheEpt(); // Try again ? maybe it will work

	InterlockedIncrement64((volatile LONG64*)&pNmiItem->m_iCompletedCount);

	while (pNmiItem->m_iCompletedCount != pCurrentCpu->m_iCoreCount)
		_mm_pause();
}

void IsrImpl::HvImplDbgInterrupt(void)
{

}
