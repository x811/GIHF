#include "stdafx.h"

/*
	This function will allocate all the custom paging structs used by host,
	then it will populate them with the physical addresses of the memory itself.

	For a host to run properly with these custom paging, it needs a Windows kernel,
	hypervisor driver and all of the hypervisor memory allocations mapped to this
	custom paging hierarchy, otherwise the system will halt.
*/

//void VmmConfig::ConstructModulePaging(PVMM_CONTEXT pVmm, const char* ModuleName)
//{
//	KMODULE_INFO ntInfo{ 0 };
//
//	if (!pVmm->m_pHostPagingBase)
//		pVmm->m_pHostPagingBase = reinterpret_cast<PML4E_64*>(MemoryUtils::AllocatePages(PAGE_SIZE));
//
//	uint64_t HostPaging = MmGetPhysicalAddress(pVmm->m_pHostPagingBase).QuadPart;
//
//	if (!MemoryUtils::GetKernelModuleAddressAndSize(&ntInfo, ModuleName))
//	{
//		__debugbreak();
//		return;
//	}
//
//	CloneAddressPagingFromGuestVirtualAddress(HostPaging, ntInfo.m_iBase, ntInfo.m_iSize);
//}

void VmmConfig::BuildHostPaging(PVIRTUAL_CPU pCpu)
{
	if (!pCpu->m_pHostPagingBase)
		pCpu->m_pHostPagingBase = reinterpret_cast<PML4E_64*>(MemoryUtils::AllocatePages(PAGE_SIZE));

	PML4E_64* HostPaging = pCpu->m_pHostPagingBase;
	PML4E_64* GuestPaging = reinterpret_cast<PML4E_64*>(MmGetVirtualForPhysical({ .QuadPart = (LONGLONG)pCpu->m_sGuestPhysicalBase.Flags }));

	RtlCopyMemory(HostPaging, GuestPaging, sizeof(PML4E_64*) * 512);

	/*
	* This is actually a working code, but it's very slow and shouldn't be used
	* in the actual scenario
	*/

	/*for (size_t Pml4Idx = 0; Pml4Idx < 512; Pml4Idx++)
	{
		PML4E_64* pPml4EntryGuest = reinterpret_cast<PML4E_64*>(&GuestPaging[Pml4Idx]);
		PML4E_64* pPml4EntryHost = reinterpret_cast<PML4E_64*>(&HostPaging[Pml4Idx]);

		if (!pPml4EntryGuest->Flags || !pPml4EntryGuest->Present)
			continue;

		if (!pPml4EntryHost->Flags || !pPml4EntryHost->Present)
		{
			uint64_t Address = AllocatePagingTableFromEntry(pPml4EntryHost, Pml4Idx);
			pPml4EntryHost->Flags = pPml4EntryGuest->Flags;
			pPml4EntryHost->PageFrameNumber = MmGetPhysicalAddress((PVOID)Address).QuadPart >> PAGE_SHIFT;
		}

		PDPTE_64* pPdptBaseGuest = reinterpret_cast<PDPTE_64*>(MmGetVirtualForPhysical({ .QuadPart = (LONGLONG)pPml4EntryGuest->PageFrameNumber << PAGE_SHIFT }));
		PDPTE_64* pPdptBaseHost = reinterpret_cast<PDPTE_64*>(MmGetVirtualForPhysical({ .QuadPart = (LONGLONG)pPml4EntryHost->PageFrameNumber << PAGE_SHIFT }));

		if (!pPdptBaseGuest)
			continue;

		for (size_t PdptIdx = 0; PdptIdx < 512; PdptIdx++)
		{
			PDPTE_64* pPdptEntryGuest = reinterpret_cast<PDPTE_64*>(&pPdptBaseGuest[PdptIdx]);
			PDPTE_64* pPdptEntryHost = reinterpret_cast<PDPTE_64*>(&pPdptBaseHost[PdptIdx]);

			if (!pPdptEntryGuest->Flags || !pPdptEntryGuest->Present)
				continue;

			if (!pPdptEntryHost->Flags || !pPdptEntryHost->Present)
			{
				uint64_t Address = AllocatePagingTableFromEntry(pPdptEntryHost, PdptIdx);
				pPml4EntryHost->Flags = pPml4EntryGuest->Flags;
				pPml4EntryHost->PageFrameNumber = MmGetPhysicalAddress((PVOID)Address).QuadPart >> PAGE_SHIFT;
			}

			PDE_64* pPdBaseGuest = reinterpret_cast<PDE_64*>(MmGetVirtualForPhysical({ .QuadPart = (LONGLONG)pPdptEntryGuest->PageFrameNumber << PAGE_SHIFT }));
			PDE_64* pPdBaseHost = reinterpret_cast<PDE_64*>(MmGetVirtualForPhysical({ .QuadPart = (LONGLONG)pPdptEntryHost->PageFrameNumber << PAGE_SHIFT }));

			if (!pPdBaseGuest)
				continue;

			for (size_t PdIdx = 0; PdIdx < 512; PdIdx++)
			{
				PDE_64* pPdEntryGuest = reinterpret_cast<PDE_64*>(&pPdBaseGuest[PdIdx]);
				PDE_64* pPdEntryHost = reinterpret_cast<PDE_64*>(&pPdBaseHost[PdIdx]);

				if (!pPdEntryGuest->Flags || !pPdEntryGuest->Present)
					continue;

				if (!pPdEntryHost->Flags || !pPdEntryHost->Present)
				{
					if (pPdEntryGuest->LargePage)
					{
						__debugbreak();
						pPdEntryHost->Flags = pPdEntryGuest->Flags;
						break;
					}

					uint64_t Address = AllocatePagingTableFromEntry(pPdEntryHost, PdIdx);
					pPdEntryHost->Flags = pPdEntryGuest->Flags;
					pPdEntryHost->PageFrameNumber = MmGetPhysicalAddress((PVOID)Address).QuadPart >> PAGE_SHIFT;
				}

				PTE_64* pPtBaseGuest = reinterpret_cast<PTE_64*>(MmGetVirtualForPhysical({ .QuadPart = (LONGLONG)pPdEntryGuest->PageFrameNumber << PAGE_SHIFT }));
				PTE_64* pPtBaseHost = reinterpret_cast<PTE_64*>(MmGetVirtualForPhysical({ .QuadPart = (LONGLONG)pPdEntryHost->PageFrameNumber << PAGE_SHIFT }));
				
				if (!pPtBaseGuest)
					continue;

				for (size_t PtIdx = 0; PtIdx < 512; PtIdx++)
				{
					PTE_64* pPtEntryGuest = reinterpret_cast<PTE_64*>(&pPtBaseGuest[PtIdx]);
					PTE_64* pPtEntryHost = reinterpret_cast<PTE_64*>(&pPtBaseHost[PtIdx]);

					if (!pPtEntryGuest->Flags || !pPtEntryGuest->Present)
						continue;

					if (!pPtEntryHost->Flags || !pPtEntryHost->Present)
					{
						uint64_t Address = AllocatePagingTableFromEntry(pPtEntryHost, PtIdx);

						pPtEntryHost->Flags = pPtEntryGuest->Flags;
						pPtEntryHost->PageFrameNumber = MmGetPhysicalAddress((PVOID)Address).QuadPart >> PAGE_SHIFT;
					}
				}
			}
		}
	}
	__debugbreak();*/
}

/*
	This must be called prior to the first launch of VM
*/

//void VmmConfig::ConstructHostPaging(PVMM_CONTEXT pVmm)
//{
//	GROUP_AFFINITY ga{ 0 }, o_ga{ 0 };
//	PROCESSOR_NUMBER pn{ 0 };
//
//	if (!pVmm->m_pHostPagingBase)
//		pVmm->m_pHostPagingBase = reinterpret_cast<PML4E_64*>(MemoryUtils::AllocatePages(PAGE_SIZE));
//
//	uint64_t HostPaging = MmGetPhysicalAddress(pVmm->m_pHostPagingBase).QuadPart;
//
//	SEGMENT_DESCRIPTOR_REGISTER_64 gdt{ 0 }, idt{ 0 };
//	_sgdt(&gdt);
//	__sidt(&idt);
//	
//	FillHostPaging(pVmm);
//	FillHostPaging(pVmm->m_pCpuArray);
//	FillHostPaging(pVmm->m_MsrBitmap.m_pMsrBitmap);
//	FillHostPaging(pVmm->m_pNmiWorkerItem);
//
//	/* This should also map the VA's from those entries to PA, but it's enough for now */
//
//	FillHostPaging(gdt.BaseAddress);
//	FillHostPaging(idt.BaseAddress);
//
//	/* Maps the EPT Paging structures in the host PA */
//
//	FillHostPaging(pVmm->m_pEptGuestPaging);
//	FillHostPaging(pVmm->m_pEptGuestPaging->m_pEptPml4);
//	FillHostPaging(pVmm->m_pEptGuestPaging->m_pEptPdpt);
//	FillHostPaging(pVmm->m_pEptGuestPaging->m_pEptPd, sizeof(EPDE_2MB) * 512 * 512);
//
//	/*
//		Map the Pre Allocated Pages in order to not crash when handling VM Exit
//	*/
//
//	FillHostPaging(pVmm->m_pSparePages);
//
//	for (uint16_t i = 0; i < sizeof(pVmm->m_pSparePages->m_sSparePages) / sizeof(SPARE_PAGE); i++)
//	{
//		FillHostPaging(pVmm->m_pSparePages->m_sSparePages[i].m_pPage);
//	}
//
//	for (size_t i = 0; i < sizeof(pVmm->m_pSparePages->m_sSplitPages) / sizeof(SPLIT_PAGE); i++)
//	{
//		FillHostPaging(pVmm->m_pSparePages->m_sSplitPages[i].m_pPage);
//	}
//
//	/*
//		Each core get's it's own mapping in the translation process
//	*/
//
//	for (uint32_t i = 0; i < pVmm->m_iCoreCount; i++)
//	{
//		KeGetProcessorNumberFromIndex(i, &pn);
//		RtlZeroMemory(&ga, sizeof(GROUP_AFFINITY));
//
//		ga.Group = pn.Group;
//		ga.Mask = (KAFFINITY)1 << pn.Number;
//
//		KeSetSystemGroupAffinityThread(&ga, &o_ga);
//		
//		FillHostPaging(pVmm->m_pCpuArray[i]);
//		FillHostPaging(pVmm->m_pCpuArray[i]->m_pHostStack, KERNEL_STACK_SIZE);
//		FillHostPaging(pVmm->m_pCpuArray[i]->m_pVmcsRegion);
//		FillHostPaging(pVmm->m_pCpuArray[i]->m_pVmxonRegion);
//		FillHostPaging(pVmm->m_pCpuArray[i]->m_pApicPage);
//		FillHostPaging(pVmm->m_pCpuArray[i]->m_pGlobalDescriptorTable);
//		FillHostPaging(pVmm->m_pCpuArray[i]->m_pInterruptTable);
//		FillHostPaging(pVmm->m_pCpuArray[i]->m_pCpuTss);
//		FillHostPaging(pVmm->m_pCpuArray[i]->m_pCpuTss->IST1_STACK, KERNEL_STACK_SIZE);
//		FillHostPaging(pVmm->m_pCpuArray[i]->m_pCpuTss->IST2_STACK, KERNEL_STACK_SIZE);
//		FillHostPaging(pVmm->m_pCpuArray[i]->m_pCpuFSBase);
//
//		FillHostPaging(__readgsqword(0x20), PAGE_SIZE * 3);
//		FillHostPaging(__readmsr(IA32_GS_BASE));
//		//FillHostPaging(__readmsr(IA32_FS_BASE));
//		
//		KeRevertToUserGroupAffinityThread(&o_ga);
//	}
//
//
//}
