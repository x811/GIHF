#include "stdafx.h"

typedef ULONG(NTAPI* t_HalGetBusData)(IN BUS_DATA_TYPE 	BusDataType,
	IN ULONG 	BusNumber,
	IN ULONG 	SlotNumber,
	IN PVOID 	Buffer,
	IN ULONG 	Length
	);

//PSPLIT_PAGE FindSplitPageAndLockIt(PVIRTUAL_CPU pCpu, uint64_t BackLinkPfn)
//{
//	for (size_t i = 0; i < sizeof(pCpu->m_pSparePages->m_sSplitPages) / sizeof(SPLIT_PAGE); i++)
//	{
//		if (pCpu->m_pSparePages->m_sSplitPages[i].m_bIsTaken)
//			continue;
//
//		pCpu->m_pSparePages->m_sSplitPages[i].m_bIsTaken = true;
//		return &pCpu->m_pSparePages->m_sSplitPages[i];
//	}
//
//	return nullptr;
//}
//
//PSPARE_PAGE FindFreePageAndLockIt(PVIRTUAL_CPU pCpu)
//{
//	for (uint16_t i = 0; i < sizeof(pCpu->m_pSparePages->m_sSparePages) / sizeof(SPARE_PAGE); i++)
//	{
//		if (pCpu->m_pSparePages->m_sSparePages[i].m_bIsTaken)
//			continue;
//
//		pCpu->m_pSparePages->m_sSparePages[i].m_bIsTaken = true;
//		return &pCpu->m_pSparePages->m_sSparePages[i];
//	}
//
//	return nullptr;
//}
//
//PHOOK_PAGE FindFreeHookPage(PVIRTUAL_CPU pCpu)
//{
//	for (uint16_t i = 0; i < sizeof(pCpu->m_pSparePages->m_sHookPages) / sizeof(HOOK_PAGE); i++)
//	{
//		if (pCpu->m_pSparePages->m_sHookPages[i].m_bIsTaken)
//			continue;
//
//		pCpu->m_pSparePages->m_sHookPages[i].m_bIsTaken = true;
//		return &pCpu->m_pSparePages->m_sHookPages[i];
//	}
//}

uint8_t VmGetFixedMtrrRange(uint64_t MsrAddr, uint32_t index)
{
	uint64_t MsrValue = __readmsr(MsrAddr + (index >> 3));
	return static_cast<uint8_t>(MsrValue >> (index << 3));
}

/*
*   This applies only for Fixed MTRRs as the memory in
*   the Variable MTRRs is the dominant against the PAT
*   but PAT is required to be ANDed with MTRRs type to
*   get effective Memory Type of PAGE.
*
*   The problem here is that, with that kind of setup
*   it would be hard to deduce where is the PTE for
*   that page, nonetheless it does need to account for
*   the 4KB pages, as I only map the 2MB pages. So
*   this setup is pretty fucked and I don't think it's
*   possible to precisely setup the MTRR type right
*   way when dealing with EPT, because one approach
*   causes very much memory allocated or the other one
*   being undocumented and extremely hard to rewrite
*   this shit, which I am already tired of.
*/

/*
*   This does need a physical address of a Guest, no matter
*   if it's large page or not.
*
*   Point here, if it's a large page shift it 21 bits insted
*   of 12 bits.
*/

typedef uint64_t(__fastcall* t_MiGetPteAddress)(uint64_t Va);

uint8_t VmxExtendedPaging::VmGetMtrrMemoryTypeForAddress(const PVMM_CONTEXT pVmm, PVIRTUAL_CPU pCpu, uint64_t PhysAddress)
{
	uint64_t MaskBase{ 0 }, MaskTarget{ 0 };
	IA32_MTRR_PHYSBASE_REGISTER Range{ 0 };
	IA32_MTRR_DEF_TYPE_REGISTER DefType{ 0 };
	IA32_PAT_REGISTER PatRegister{ 0 };
	//t_MiGetPteAddress MiGetPteAddress{ nullptr };

	//UCHAR pattern[] = "\xE8\xCC\xCC\xCC\xCC\x4C\x8B\xC0\xC7\x44\x24";

	//PatRegister.Flags = __readmsr(IA32_PAT);
	DefType.Flags = __readmsr(IA32_MTRR_DEF_TYPE);

	if (PhysAddress < PAGE_SIZE_2MB / 2)
	{
		if (!pVmm->m_bFixedMtrrs) // This actually can cause problems
			return DefType.DefaultMemoryType;

		if (PhysAddress < IA32_MTRR_FIX16K_BASE)
			return VmGetFixedMtrrRange(IA32_MTRR_FIX64K_00000, PhysAddress / (PAGE_SIZE << 4));

		if (PhysAddress < IA32_MTRR_FIX4K_BASE)
		{
			PhysAddress -= IA32_MTRR_FIX16K_BASE;
			return VmGetFixedMtrrRange(IA32_MTRR_FIX16K_80000, PhysAddress / (PAGE_SIZE << 2));
		}

		PhysAddress -= IA32_MTRR_FIX4K_BASE;
		return VmGetFixedMtrrRange(IA32_MTRR_FIX4K_C0000, PhysAddress / PAGE_SIZE);
	}
#ifndef _VM
	/*
		This only executes if we are not dealing with the Fixed MTRRs
	*/

	for (uint8_t MtrrIdx = 0; MtrrIdx < pVmm->m_iVariableMtrrsCount; MtrrIdx++)
	{
		MaskBase = pVmm->m_pVariableMtrrs[MtrrIdx].m_sMtrr0.PageFrameNumber << PAGE_SHIFT;
		MaskTarget = pVmm->m_pVariableMtrrs[MtrrIdx].m_sMtrrMask0.PageFrameNumber << PAGE_SHIFT;

		if (!pVmm->m_pVariableMtrrs[MtrrIdx].m_sMtrrMask0.Valid)
			continue;

		if ((PhysAddress & MaskTarget) == (MaskBase & MaskTarget))
			return pVmm->m_pVariableMtrrs[MtrrIdx].m_sMtrr0.Type;
	}
#endif
	return DefType.DefaultMemoryType;
}

/*
	This will ultimately return the physical address of EPT Pointer of the preallocated for the
	Guest PA to Host PA translations. This is the exact mechanism to define the translation of
	first 512GB of the real physical memory and mapping them as 2MB pages.

	This will result in EPT Violations for the entire 2MB pages, so that the pages of interest
	must be split to 4KB...
*/

void VmxExtendedPaging::VmSetupEptPagingStructures(PVIRTUAL_CPU pCpu)
{
	for (uint8_t EptIdx = 0; EptIdx < sizeof(pCpu->m_pEptGuestPaging) / sizeof(PEPT_GUEST_PAGING); EptIdx++)
	{
		EPT_POINTER Ept{ 0 };
		EPT_PML4* pEptPml4Base = reinterpret_cast<EPT_PML4*>(MemoryUtils::AllocatePages(PAGE_SIZE));
		EPDPTE(*pEptPdptBase)[512] = reinterpret_cast<EPDPTE(*)[512]>(MemoryUtils::AllocatePages(sizeof(EPDPTE) * 512));
		EPDE_2MB(*pEptPdBase)[512][512] = reinterpret_cast<EPDE_2MB(*)[512][512]>(MemoryUtils::AllocatePages(sizeof(EPDE_2MB) * 512 * 512));

		EPT_PML4* pEptPml4E = reinterpret_cast<EPT_PML4*>(&pEptPml4Base[0]);

		pEptPml4E->WriteAccess = 1;
		pEptPml4E->ExecuteAccess = 1;
		pEptPml4E->ReadAccess = 1;
		pEptPml4E->PageFrameNumber = MmGetPhysicalAddress(&pEptPdptBase[0][0]).QuadPart >> PAGE_SHIFT;

		for (uint16_t pdptIdx = 0; pdptIdx < 512; pdptIdx++)
		{
			EPDPTE* pEptPdptE = reinterpret_cast<EPDPTE*>(&pEptPdptBase[0][pdptIdx]);

			pEptPdptE->WriteAccess = 1;
			pEptPdptE->ExecuteAccess = 1;
			pEptPdptE->ReadAccess = 1;
			pEptPdptE->PageFrameNumber = MmGetPhysicalAddress(&pEptPdBase[0][pdptIdx][0]).QuadPart >> PAGE_SHIFT;

			for (uint16_t pdIdx = 0; pdIdx < 512; pdIdx++)
			{
				uint64_t translation_pfn = (pdptIdx * 512) + pdIdx;
				EPDE_2MB* pEptPdE = reinterpret_cast<EPDE_2MB*>(&pEptPdBase[0][pdptIdx][pdIdx]);

				pEptPdE->WriteAccess = 1;
				pEptPdE->ExecuteAccess = 1;
				pEptPdE->LargePage = 1;
				pEptPdE->ReadAccess = 1;
				pEptPdE->MemoryType = VmGetMtrrMemoryTypeForAddress(pCpu->m_pVmmBacklink, pCpu, translation_pfn << PAGE_SHIFT_2MB);
				pEptPdE->PageFrameNumber = translation_pfn;

			}
		}

		Ept.MemoryType = 6; //Quick constant for WriteBack;
		Ept.PageWalkLength = 3; //Quick constant for 4 Level Page Translation
		Ept.PageFrameNumber = MmGetPhysicalAddress(pEptPml4Base).QuadPart >> PAGE_SHIFT;

		pCpu->m_pEptGuestPaging[EptIdx]->m_pEptTranslation = Ept;
		pCpu->m_pEptGuestPaging[EptIdx]->m_pEptPml4 = pEptPml4Base;
		pCpu->m_pEptGuestPaging[EptIdx]->m_pEptPdpt = pEptPdptBase;
		pCpu->m_pEptGuestPaging[EptIdx]->m_pEptPd = pEptPdBase;
	}
}

void VmSetupPageTableEntry(PVIRTUAL_CPU pCpu, const EPT_POINTER& EptState, uint64_t GuestPhysical)
{
	VIRTUAL_ADDRESS pa = { .Flags = GuestPhysical };
	EPT_PML4* pPml4Base = reinterpret_cast<EPT_PML4*>(MmGetVirtualForPhysical({ .QuadPart = (LONGLONG)EptState.PageFrameNumber << PAGE_SHIFT }));

	if (!pPml4Base)
		pPml4Base = reinterpret_cast<EPT_PML4*>(MemoryUtils::AllocatePages(PAGE_SIZE));

	EPT_PML4* pPml4Entry = &pPml4Base[pa.bits.PML4E];

	EPDPTE* pPdptEntry = reinterpret_cast<EPDPTE*>(MemoryUtils::AllocatePagingTableFromEntryEPT(pPml4Entry, pa.bits.PDPTE));

	EPDE* pPdEntry = reinterpret_cast<EPDE*>(MemoryUtils::AllocatePagingTableFromEntryEPT(pPdptEntry, pa.bits.PDE));

	EPTE* pPtEntry = reinterpret_cast<EPTE*>(MemoryUtils::AllocatePagingTableFromEntryEPT(pPdEntry, pa.bits.PTE));

	pPtEntry->ReadAccess = 1;
	pPtEntry->WriteAccess = 1;
	pPtEntry->ExecuteAccess = 1;

	pPtEntry->PageFrameNumber = pa.Flags >> PAGE_SHIFT;

	pPtEntry->MemoryType = VmxExtendedPaging::VmGetMtrrMemoryTypeForAddress(pCpu->m_pVmmBacklink, pCpu, pa.Flags);
}

// UNUSED
void VmSetupPciPaging(PVIRTUAL_CPU pCpu)
{
	static t_HalGetBusData HalGetBusData{ nullptr };
	PCI_SLOT_NUMBER pciSlot{ 0 };
	PCI_COMMON_CONFIG pciConfig{ 0 };
	
	if (!HalGetBusData)
	{
		UNICODE_STRING name{ 0 };

		RtlInitUnicodeString(&name, L"HalGetBusData");
		HalGetBusData = reinterpret_cast<t_HalGetBusData>(MmGetSystemRoutineAddress(&name));
		//RtlFreeUnicodeString(&name);
	}

	for (uint8_t nBus = 0; nBus < 256; nBus++)
	{
		for (uint8_t nDevice = 0; nDevice < PCI_MAX_DEVICES; nDevice++)
		{
			pciSlot.u.bits.DeviceNumber = nDevice;

			for (uint8_t nFunction = 0; nFunction < PCI_MAX_FUNCTION; nFunction++)
			{
				pciSlot.u.bits.FunctionNumber = nFunction;

				if (HalGetBusData(PCIConfiguration, nBus, pciSlot.u.AsULONG, &pciConfig, sizeof(PCI_COMMON_CONFIG)))
				{
					for (uint8_t BARnum = 0; BARnum < PCI_TYPE0_ADDRESSES; BARnum++)
					{
						if (!(pciConfig.u.type0.BaseAddresses[BARnum]) || pciConfig.u.type0.BaseAddresses[BARnum] == MAXUINT32)
							continue;

						if ((pciConfig.u.type0.BaseAddresses[BARnum] & 1))
							continue;

						/* HalSetBusData should be used */
						uint32_t BackUpBAR = pciConfig.u.type0.BaseAddresses[BARnum];
						pciConfig.u.type0.BaseAddresses[BARnum] = MAXUINT32;
						uint32_t MaskedBits = pciConfig.u.type0.BaseAddresses[BARnum];
						MaskedBits &= ~0xF;
						uint32_t Size = ~(MaskedBits)+1;
						pciConfig.u.type0.BaseAddresses[BARnum] = BackUpBAR;
					
					}

				}
			}
		}
	}
}

/*
*   This function maps memory as described by Physical Memory Block
*   This works and sufficient enough, but doesn't set anything
*   that is not a general RAM, which can be WB.
*/

void VmxExtendedPaging::VmSetupEptIdentityPaging(PVIRTUAL_CPU pCpu)
{
	EPT_POINTER Ept{ 0 };
	EPT_PML4* pEptPml4Base{ nullptr };
	EPDPTE* pEptPdptBase{ nullptr };
	EPDE* pEptPdBase{ nullptr };
	EPTE* pEptPtBase{ nullptr };

	static PPHYSICAL_MEMORY_DESCRIPTOR pPhysicalPtData{ 0 };
	UCHAR patternPhysMemBlock[] = "\x48\x0F\x44\x15\xCC\xCC\xCC\xCC\x4C\x89\x0D";

	if (!pPhysicalPtData)
		pPhysicalPtData = *reinterpret_cast<PPHYSICAL_MEMORY_DESCRIPTOR*>(MemoryUtils::GetAddrFromPattern(patternPhysMemBlock, sizeof(patternPhysMemBlock) - 1, 8, 4));

	if (!pPhysicalPtData)
		KeBugCheck(BUGCODE_ID_DRIVER);

	pEptPml4Base = reinterpret_cast<EPT_PML4*>(MemoryUtils::AllocatePages(PAGE_SIZE));

	Ept.MemoryType = 6; // Quick constant for WB
	Ept.PageWalkLength = 3; // 4-Level
	Ept.PageFrameNumber = MmGetPhysicalAddress(pEptPml4Base).QuadPart >> PAGE_SHIFT;

	for (uint16_t MemBlkIdx = 0; MemBlkIdx < pPhysicalPtData->NumberOfRuns; MemBlkIdx++)
	{
		PHYSICAL_MEMORY_RUN pPagingData = pPhysicalPtData->Run[MemBlkIdx];

		for (uint32_t PageNum = 0; PageNum < pPagingData.PageCount; PageNum++)
		{
			VmSetupPageTableEntry(pCpu, Ept, (pPagingData.BasePage + PageNum) * PAGE_SIZE);
		}
	}

	//VmSetupPciPaging(pCpu);
	for (uint8_t EptIdx = 0; EptIdx < sizeof(pCpu->m_pEptGuestPaging) / sizeof(PEPT_GUEST_PAGING); EptIdx++)
	{
		pCpu->m_pEptGuestPaging[EptIdx]->m_pEptTranslation = Ept;
	}
}

/*
	If already split will return false, otherwise splits PDE into PT and fills PTE and returns true
*/

bool VmxExtendedPaging::VmSplitLargePage(PVIRTUAL_CPU pCpu, EPDE_2MB* pPdELargeEntry)
{
	EPDE* pEptPdE{ nullptr };
	EPTE* pEptPtBase{ nullptr }, * pEptPtE{ nullptr };
	PSPLIT_PAGE pFreePage{ nullptr };
	uint64_t pEptPfnPhysical{ 0 };

	if (!pPdELargeEntry->Flags || !pPdELargeEntry->LargePage)
		return false;

	pFreePage = VmxUtils::FindSplitPageAndLockIt(pCpu, pPdELargeEntry->PageFrameNumber);

	pEptPfnPhysical = pPdELargeEntry->PageFrameNumber;
	pEptPtBase = reinterpret_cast<EPTE*>(pFreePage->m_pPage);
	pEptPdE = reinterpret_cast<EPDE*>(pPdELargeEntry);

	pEptPdE->Flags = 0;

	pEptPdE->ExecuteAccess = 1;
	pEptPdE->WriteAccess = 1;
	pEptPdE->ReadAccess = 1;

	pEptPdE->PageFrameNumber = pFreePage->m_iPhysicalAddress >> PAGE_SHIFT;

	for (uint16_t PtIdx = 0; PtIdx < 512; PtIdx++)
	{
		pEptPtE = &pEptPtBase[PtIdx];

		/* Essentially traverses range by 4KB (e.g. if address was 0x280000, then it will add 0x1000 and not 0x20000, thus mapping as 4KB) */
		pEptPtE->PageFrameNumber = ((pEptPfnPhysical << 21) + (PtIdx * PAGE_SIZE)) >> PAGE_SHIFT;

		pEptPtE->ReadAccess = 1;
		pEptPtE->WriteAccess = 1;
		pEptPtE->ExecuteAccess = 1;
		pEptPtE->MemoryType = VmGetMtrrMemoryTypeForAddress(pCpu->m_pVmmBacklink, pCpu, pEptPtE->PageFrameNumber << PAGE_SHIFT);
	}

	return true;
}

/*
	This will return true if page was constructed back to the 2MB one,
	otherwise it will fail and return false.
*/

bool VmxExtendedPaging::VmAssembleLargePage(PVIRTUAL_CPU pCpu, EPDE* pPdEntry)
{
	if (pPdEntry->LargePage)
		return false;
	
	EPDE_2MB* pEptPdLargeEntry{ nullptr };
	EPTE* pEptPtBase{ nullptr }, * pEptPtEntry{ nullptr };
	uint64_t BasePfn{ 0 };

	pEptPdLargeEntry = reinterpret_cast<EPDE_2MB*>(pPdEntry);

	pEptPtBase = reinterpret_cast<EPTE*>(MmGetVirtualForPhysical({ .QuadPart = (LONGLONG)pPdEntry->PageFrameNumber << PAGE_SHIFT }));
	pEptPtEntry = &pEptPtBase[0]; // The first Entry is the original one, that was split.

	BasePfn = pEptPtEntry->PageFrameNumber << PAGE_SHIFT;

	pEptPdLargeEntry->Flags = 0; // This is a must

	pEptPdLargeEntry->ReadAccess = 1;
	pEptPdLargeEntry->ExecuteAccess = 1;
	pEptPdLargeEntry->WriteAccess = 1;
	pEptPdLargeEntry->LargePage = 1;

	pEptPdLargeEntry->PageFrameNumber = BasePfn >> PAGE_SHIFT_2MB;

	for (uint16_t HvPageIdx = 0; HvPageIdx < sizeof(pCpu->m_pSparePages->m_sSplitPages) / sizeof(SPLIT_PAGE); HvPageIdx++)
	{
		if (pCpu->m_pSparePages->m_sSplitPages[HvPageIdx].m_iPageVirtualAddress != (uint64_t)pEptPtBase)
			continue;
		
		RtlZeroMemory(pCpu->m_pSparePages->m_sSplitPages[HvPageIdx].m_pPage, PAGE_SIZE);
		pCpu->m_pSparePages->m_sSplitPages[HvPageIdx].m_bIsTaken = false;
	}

	return true;
}

/*
*	Will return true if Page is successfully hooked,
*	otherwise it will return false if something went wrong.
*/


bool VmxExtendedPaging::VmHookPage(PVIRTUAL_CPU pCpu, uint64_t VirtualAddress)
{

	EPTE* pEptEntry{ nullptr };
	PHOOK_PAGE pFreeHookedPage;
	bool IsLargeEptPage{ false };

	pFreeHookedPage = VmxUtils::FindFreeHookPage(pCpu);

	for (uint8_t EptpIdx = 0; EptpIdx < sizeof(pCpu->m_pEptGuestPaging) / sizeof(PEPT_GUEST_PAGING); EptpIdx++)
	{
		pEptEntry = reinterpret_cast<EPTE*>(VmFindEptEntryByAddress(pCpu->m_pEptGuestPaging[EptpIdx], pCpu->m_sGuestPhysicalBase.Flags, VirtualAddress, &IsLargeEptPage));

		if (IsLargeEptPage)
		{
			if (!VmSplitLargePage(pCpu, reinterpret_cast<EPDE_2MB*>(pEptEntry)))
				return false;

			pEptEntry = reinterpret_cast<EPTE*>(VmFindEptEntryByAddress(pCpu->m_pEptGuestPaging[EptpIdx], pCpu->m_sGuestPhysicalBase.Flags, VirtualAddress, &IsLargeEptPage));
		}

		pFreeHookedPage->m_iOriginalPhysAddr = pEptEntry->PageFrameNumber << PAGE_SHIFT;

		pEptEntry->Flags = 0;

		pEptEntry->ExecuteAccess = EptpIdx > 0 ? 0 : 1;
		pEptEntry->ReadAccess = EptpIdx;
		pEptEntry->WriteAccess = EptpIdx;

		pEptEntry->PageFrameNumber = pFreeHookedPage->m_iOriginalPhysAddr;

		if (EptpIdx == 0)
		{
			/*
			*	This is general page copying. It's designed to copy the contents of the page that
			*	is being hooked. But for an actual hooking the least significant 12 bits are
			*	required, so we hit the actual point of code execution when the function is really
			*	executing.
			* 
			*	At that point of execution we can modify memory in EPT[0] and detour it to our
			*	own defined function. To be user friendly it does need to take the bytes generated
			*	from the JIT assembler that is passed in a (future made) special buffer of 4KB.
			*	
			*	Only then, it must place that "byte code" from that buffer to a real pre-allocated
			*	page. The actual pages, not including the buffer page must be pre-allocated in
			*	a struct like "SPARE_PAGE" for each of hooking page. I suggest the number of 12
			*	structs alike as it must suffice most of debugging goals.
			*/
			RtlCopyMemory(pFreeHookedPage->m_pModifiedPage, (PVOID)(VirtualAddress & ~0xFFF), PAGE_SIZE);
			pEptEntry->PageFrameNumber = pFreeHookedPage->m_iModifiedPhys >> PAGE_SHIFT;
		}

		pEptEntry->MemoryType = VmGetMtrrMemoryTypeForAddress(pCpu->m_pVmmBacklink, pCpu, pFreeHookedPage->m_iOriginalPhysAddr);
	}

	return true;
}

uint64_t VmxExtendedPaging::VmFindEptEntryByAddress(PEPT_GUEST_PAGING pGuestEptPaging, uint64_t GuestPhysicalBase, uint64_t VirtualAddress, bool* IsLargeEntry)
{
	uint64_t GuestPhysicalAddress{ 0 }, GuestPageSize{ 0 };
	uint64_t GuestPfn{ 0 }, GuestPhysicalNormalized{ 0 };
	VIRTUAL_ADDRESS GuestPhysNormalized{ 0 }; //This is not actually virtual, it happens that definition is the same

	GuestPhysicalAddress = MemoryUtils::VatoPa(GuestPhysicalBase, VirtualAddress, &GuestPageSize);

	if (!GuestPhysicalAddress)
		return MAXUINT64;

	GuestPhysicalNormalized = GuestPageSize == PAGE_SIZE_2MB ? GuestPfn << PAGE_SHIFT_2MB : GuestPfn << PAGE_SHIFT;
	GuestPhysNormalized.Flags = GuestPhysicalNormalized;

	/*
		This is kind of code duplication, doesn't look nice.

		EPT GUEST PAGING must take a real value not that [0], it's placeholder for now.
	*/

	EPT_PML4* pEptPml4Base = reinterpret_cast<EPT_PML4*>(MmGetVirtualForPhysical({ .QuadPart = (LONGLONG)pGuestEptPaging->m_pEptTranslation.PageFrameNumber << PAGE_SHIFT}));
	EPT_PML4* pEptPml4Entry = &pEptPml4Base[GuestPhysNormalized.bits.PML4E];

	EPDPTE* pEptPdptBase = reinterpret_cast<EPDPTE*>(MmGetVirtualForPhysical({ .QuadPart = (LONGLONG)pEptPml4Entry->PageFrameNumber << PAGE_SHIFT }));
	EPDPTE* pEptPdptEntry = &pEptPdptBase[GuestPhysNormalized.bits.PDPTE];

	EPDE_2MB* pEptPdBase = reinterpret_cast<EPDE_2MB*>(MmGetVirtualForPhysical({ .QuadPart = (LONGLONG)pEptPdptEntry->PageFrameNumber << PAGE_SHIFT }));
	EPDE_2MB* pEptPdEntry = &pEptPdBase[GuestPhysNormalized.bits.PDE];

	if (pEptPdEntry->LargePage)
	{
		if(IsLargeEntry != nullptr)
			*IsLargeEntry = true;
		return (uint64_t)pEptPdEntry;
	}

	EPTE* pEptPtBase = reinterpret_cast<EPTE*>(MmGetVirtualForPhysical({ .QuadPart = static_cast<LONGLONG>(reinterpret_cast<EPDE*>(pEptPdEntry)->PageFrameNumber << PAGE_SHIFT) }));
	EPTE* pEptPtEntry = &pEptPtBase[GuestPhysNormalized.bits.PTE];

	if(IsLargeEntry != nullptr)
		*IsLargeEntry = false;

	return (uint64_t)pEptPtEntry;
}

bool VmxExtendedPaging::VmHideMemory(PVIRTUAL_CPU pCpu, uint64_t PhysicalAddress)
{
	static PSPARE_PAGE pEmptyPage{ nullptr };

	uint64_t BkpRealAddress{ 0 };
	VIRTUAL_ADDRESS PhysAddrNormalized{ 0 }; //This is not actually virtual, it happens that definition is the same

	PhysAddrNormalized.Flags = PhysicalAddress;

	if (!pEmptyPage)
		pEmptyPage = VmxUtils::FindFreePageAndLockIt(pCpu);

	/*
		Code duplication right here.
		EPT GUEST PAGING must take a real value not that [0], it's placeholder for now.
	*/

	EPT_PML4* pEptPml4Base = reinterpret_cast<EPT_PML4*>(MmGetVirtualForPhysical({ .QuadPart = (LONGLONG)pCpu->m_pEptGuestPaging[0]->m_pEptTranslation.PageFrameNumber << PAGE_SHIFT}));
	EPT_PML4* pEptPml4Entry = reinterpret_cast<EPT_PML4*>(&pEptPml4Base[PhysAddrNormalized.bits.PML4E]);

	EPDPTE* pEptPdptBase = reinterpret_cast<EPDPTE*>(MmGetVirtualForPhysical({ .QuadPart = (LONGLONG)pEptPml4Entry->PageFrameNumber << PAGE_SHIFT }));
	EPDPTE* pEptPdptEntry = reinterpret_cast<EPDPTE*>(&pEptPdptBase[PhysAddrNormalized.bits.PDPTE]);

	EPDE* pEptPdBase = reinterpret_cast<EPDE*>(MmGetVirtualForPhysical({ .QuadPart = (LONGLONG)pEptPdptEntry->PageFrameNumber << PAGE_SHIFT }));
	EPDE* pEptPdEntry = reinterpret_cast<EPDE*>(&pEptPdBase[PhysAddrNormalized.bits.PDE]);

	if (pEptPdEntry->LargePage)
		if (!VmSplitLargePage(pCpu, (EPDE_2MB*)pEptPdEntry))
			return false;

	EPTE* pEptPtBase = reinterpret_cast<EPTE*>(MmGetVirtualForPhysical({ .QuadPart = (LONGLONG)pEptPdEntry->PageFrameNumber << PAGE_SHIFT }));
	EPTE* pEptPtEntry = reinterpret_cast<EPTE*>(&pEptPtBase[PhysAddrNormalized.bits.PTE]);
	
	BkpRealAddress = pEptPtEntry->PageFrameNumber << PAGE_SHIFT;

	pEptPtEntry->Flags = 0;

	pEptPtEntry->PageFrameNumber = pEmptyPage->m_iPhysicalAddress >> PAGE_SHIFT;

	pEptPtEntry->ExecuteAccess = 1;
	pEptPtEntry->ReadAccess = 1;
	pEptPtEntry->WriteAccess = 1;
	pEptPtEntry->MemoryType = VmGetMtrrMemoryTypeForAddress(pCpu->m_pVmmBacklink, pCpu, BkpRealAddress);
	
	return true;
}

bool VmxExtendedPaging::VmHideMemoryRange(PVIRTUAL_CPU pCpu, uint64_t StartVa, uint64_t Size)
{
	uint64_t MemStartVa{ 0 }, MemEndVa{ 0 };
	uint64_t MemStartPa{ 0 }, MemEndPa{ 0 };
	uint32_t ModuleGranularity{ 0 };

	MemStartVa = StartVa;
	MemEndVa = MemStartVa + Size;

	MemStartPa = MemoryUtils::VatoPa(pCpu->m_sGuestPhysicalBase.Flags, MemStartVa, (uint64_t*)&ModuleGranularity);
	MemEndPa = MemStartPa + Size;

	/*if (ModuleGranularity != PAGE_SIZE)
		KeBugCheck(BUGCODE_ID_DRIVER);*/

	for (uint32_t MemPageSizeIdx = 0; MemPageSizeIdx < Size / PAGE_SIZE; MemPageSizeIdx++)
	{
		VmHideMemory(pCpu, MemStartPa);
	}

	return true;
}

/*
*   This is designed to hide any VMM allocation trace in Guest's
*   view of memory and it must be called in the VMCALL handler
*   and issued to all vCPUs, otherwise it won't hide anything
*   as the VMM allocates for every core it's own memory mapping.
*/

bool VmxExtendedPaging::VmHideVmm(PVIRTUAL_CPU pCpu)
{
	for (uint8_t EptIdx = 0; EptIdx < sizeof(pCpu->m_pEptGuestPaging) / sizeof(PEPT_GUEST_PAGING); EptIdx++)
	{
		if (pCpu->m_pEptGuestPaging[EptIdx]->m_pEptPml4 != nullptr && pCpu->m_pEptGuestPaging[EptIdx]->m_pEptPdpt != nullptr && pCpu->m_pEptGuestPaging[EptIdx]->m_pEptPd)
		{
			VmHideMemoryRange(pCpu, (uint64_t)pCpu->m_pEptGuestPaging[EptIdx]->m_pEptPml4, sizeof(PAGE_SIZE));
			VmHideMemoryRange(pCpu, (uint64_t)pCpu->m_pEptGuestPaging[EptIdx]->m_pEptPdpt, sizeof(*pCpu->m_pEptGuestPaging[EptIdx]->m_pEptPdpt));
			VmHideMemoryRange(pCpu, (uint64_t)pCpu->m_pEptGuestPaging[EptIdx]->m_pEptPd, sizeof(*pCpu->m_pEptGuestPaging[EptIdx]->m_pEptPd));
		}

		VmHideMemoryRange(pCpu, (uint64_t)pCpu->m_pSparePages, sizeof(SPARE_PAGES));

		for (uint16_t i = 0; i < sizeof(pCpu->m_pSparePages->m_sSplitPages) / sizeof(SPLIT_PAGE); i++)
			VmHideMemoryRange(pCpu, (uint64_t)pCpu->m_pSparePages->m_sSplitPages[i].m_pPage, PAGE_SIZE);

		for (uint16_t i = 0; i < sizeof(pCpu->m_pSparePages->m_sSparePages) / sizeof(SPARE_PAGE); i++)
			VmHideMemoryRange(pCpu, (uint64_t)pCpu->m_pSparePages->m_sSparePages[i].m_pPage, PAGE_SIZE);

		for (uint16_t CoreIdx = 0; CoreIdx < pCpu->m_pVmmBacklink->m_iCoreCount; CoreIdx++)
		{
			VmHideMemoryRange(pCpu, (uint64_t)pCpu->m_pVmmBacklink->m_pCpuArray[CoreIdx], sizeof(VIRTUAL_CPU));
			VmHideMemoryRange(pCpu, (uint64_t)pCpu->m_pVmmBacklink->m_pCpuArray[CoreIdx]->m_pHostStack, KERNEL_STACK_SIZE);
			VmHideMemoryRange(pCpu, (uint64_t)pCpu->m_pVmmBacklink->m_pCpuArray[CoreIdx]->m_pVmcsRegion, PAGE_SIZE);
			VmHideMemoryRange(pCpu, (uint64_t)pCpu->m_pVmmBacklink->m_pCpuArray[CoreIdx]->m_pVmxonRegion, PAGE_SIZE);
			VmHideMemoryRange(pCpu, (uint64_t)pCpu->m_pVmmBacklink->m_pCpuArray[CoreIdx]->m_pApicPage, PAGE_SIZE);
			VmHideMemoryRange(pCpu, (uint64_t)pCpu->m_pVmmBacklink->m_pCpuArray[CoreIdx]->m_pGlobalDescriptorTable, PAGE_SIZE);
			VmHideMemoryRange(pCpu, (uint64_t)pCpu->m_pVmmBacklink->m_pCpuArray[CoreIdx]->m_pInterruptTable, PAGE_SIZE);
			VmHideMemoryRange(pCpu, (uint64_t)pCpu->m_pVmmBacklink->m_pCpuArray[CoreIdx]->m_pCpuTss, PAGE_SIZE);
			VmHideMemoryRange(pCpu, (uint64_t)pCpu->m_pVmmBacklink->m_pCpuArray[CoreIdx]->m_pCpuTss->IST1_STACK, KERNEL_STACK_SIZE);
			VmHideMemoryRange(pCpu, (uint64_t)pCpu->m_pVmmBacklink->m_pCpuArray[CoreIdx]->m_pCpuTss->IST2_STACK, KERNEL_STACK_SIZE);
			VmHideMemoryRange(pCpu, (uint64_t)pCpu->m_pVmmBacklink->m_pCpuArray[CoreIdx]->m_pCpuFSBase, PAGE_SIZE);
			//VmHideMemoryRange(pCpu, (uint64_t)pCpu->m_pVmmBacklink->m_pCpuArray[CoreIdx]->m_pKprсb, PAGE_SIZE);
		}

		VmHideMemoryRange(pCpu, (uint64_t)pCpu->m_pVmmBacklink->m_iHvBase, pCpu->m_pVmmBacklink->m_iHvSize);

		VmHideMemoryRange(pCpu, (uint64_t)pCpu->m_pEptGuestPaging, sizeof(EPT_GUEST_PAGING));

		VmHideMemoryRange(pCpu, (uint64_t)pCpu->m_pVmmBacklink->m_MsrBitmap.m_pMsrBitmap, PAGE_SIZE);
		VmHideMemoryRange(pCpu, (uint64_t)pCpu->m_pVmmBacklink->m_pNmiWorkerItem, PAGE_SIZE);
		VmHideMemoryRange(pCpu, (uint64_t)pCpu->m_pVmmBacklink->m_pCpuArray, sizeof(PVIRTUAL_CPU) * pCpu->m_pVmmBacklink->m_iCoreCount);
		VmHideMemoryRange(pCpu, (uint64_t)pCpu->m_pVmmBacklink, sizeof(VMM_CONTEXT));
	}

	pCpu->m_pVmmBacklink->m_pNmiWorkerItem->m_bTlbFlush = true;
	return true;
}
