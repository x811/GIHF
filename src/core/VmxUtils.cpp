#include "stdafx.h"

struct _KPROCESS
{
	struct _DISPATCHER_HEADER Header;                                       //0x0
	struct _LIST_ENTRY ProfileListHead;                                     //0x18
	uint64_t DirectoryTableBase;                                           //0x28
};

PTE_64* Get_Pte(const ULONG64 address)
{
	VIRTUAL_ADDRESS virtualAddress;
	virtualAddress.Flags = address;

	CR3 cr3;
	cr3.Flags = __readcr3();

	PML4E_64* pml4 = reinterpret_cast<PML4E_64*>(MmGetVirtualForPhysical({.QuadPart = (LONGLONG)cr3.AddressOfPageDirectory << PAGE_SHIFT}));
	const PML4E_64* pml4e = (pml4 + virtualAddress.bits.PML4E);
	if (!pml4e->Present)
		return nullptr;

	PDPTE_64* pdpt = reinterpret_cast<PDPTE_64*>(MmGetVirtualForPhysical({.QuadPart = (LONGLONG)pml4e->PageFrameNumber << PAGE_SHIFT}));
	const PDPTE_64* pdpte = (pdpt + virtualAddress.bits.PDPTE);
	if (!pdpte->Present)
		return nullptr;

	// sanity check 1GB page
	if (pdpte->LargePage)
		return nullptr;

	PDE_64* pd = reinterpret_cast<PDE_64*>(MmGetVirtualForPhysical({.QuadPart = (LONGLONG)pdpte->PageFrameNumber << PAGE_SHIFT}));
	const PDE_64* pde = (pd + virtualAddress.bits.PDE);
	if (!pde->Present)
		return nullptr;

	// sanity check 2MB page
	if (pde->LargePage)
		return nullptr;

	PTE_64* pt = reinterpret_cast<PTE_64*>(MmGetVirtualForPhysical({.QuadPart = (LONGLONG)pde->PageFrameNumber << PAGE_SHIFT}));
	PTE_64* pte = (pt + virtualAddress.bits.PTE);
	if (!pte->Present)
		return nullptr;

	return pte;
}

uint64_t VmxUtils::GetPte(uint64_t PageDirectoryBase, uint64_t AddressToTranslate)
{
	CR3 cr3{ NULL };
	VIRTUAL_ADDRESS Va{ NULL };

	PML4E_64 Pml4E{ 0 };
	PDPTE_64 PdptE{ 0 };
	PDE_64 PdE{ 0 };
	PTE_64 PtE{ 0 };

	Va.Flags = AddressToTranslate;
	cr3.Flags = PageDirectoryBase;

	uint64_t Pml4e_addr = (uint64_t)MmGetVirtualForPhysical(PHYSICAL_ADDRESS{ .QuadPart = (LONGLONG)((cr3.AddressOfPageDirectory << PAGE_SHIFT) + (Va.bits.PML4E * 8)) });
	Pml4E.Flags = *reinterpret_cast<uint64_t*>(Pml4e_addr);//MmGetPhysicalAddress((PVOID)Pml4e_addr).QuadPart;

	uint64_t Pdpte_addr = (uint64_t)MmGetVirtualForPhysical(PHYSICAL_ADDRESS{ .QuadPart = (LONGLONG)((Pml4E.PageFrameNumber << PAGE_SHIFT) + (Va.bits.PDPTE * 8)) });
	PdptE.Flags = *reinterpret_cast<uint64_t*>(Pdpte_addr);//MmGetPhysicalAddress((PVOID)Pdpte_addr).QuadPart;

	if (PdptE.LargePage)
	{
		PDPTE_1GB_64 PDPTELarge{ 0 };
		PDPTELarge.Flags = PdptE.Flags;
		uint64_t resulting_address = (PDPTELarge.PageFrameNumber << 30) + (Va.bits.PDE << 21 | Va.bits.PTE << PAGE_SHIFT | Va.bits.PageOffset);


		return resulting_address;
	}

	uint64_t Pde_address = (uint64_t)MmGetVirtualForPhysical({ .QuadPart = (LONGLONG)((PdptE.PageFrameNumber << PAGE_SHIFT) + (Va.bits.PDE * 8)) });
	PdE.Flags = *reinterpret_cast<uint64_t*>(Pde_address);

	if (PdE.LargePage)
	{
		PDE_2MB_64 PdELarge{ 0 };
		PdELarge.Flags = PdE.Flags;
		uint64_t resulting_address = (PdELarge.PageFrameNumber << 21) + (Va.bits.PTE << PAGE_SHIFT | Va.bits.PageOffset);
		return resulting_address;
	}

	uint64_t Pte_address = (uint64_t)MmGetVirtualForPhysical({ .QuadPart = (LONGLONG)((PdE.PageFrameNumber << PAGE_SHIFT) + (Va.bits.PTE * 8)) });
	return Pte_address;
}

PSPLIT_PAGE VmxUtils::FindSplitPageAndLockIt(PVIRTUAL_CPU pCpu, uint64_t BackLinkPfn)
{
	for (size_t i = 0; i < sizeof(pCpu->m_pSparePages->m_sSplitPages) / sizeof(SPLIT_PAGE); i++)
	{
		if (pCpu->m_pSparePages->m_sSplitPages[i].m_bIsTaken)
			continue;

		pCpu->m_pSparePages->m_sSplitPages[i].m_bIsTaken = true;
		return &pCpu->m_pSparePages->m_sSplitPages[i];
	}

	return nullptr;
}

PSPARE_PAGE VmxUtils::FindFreePageAndLockIt(PVIRTUAL_CPU pCpu)
{
	for (uint16_t i = 0; i < sizeof(pCpu->m_pSparePages->m_sSparePages) / sizeof(SPARE_PAGE); i++)
	{
		if (pCpu->m_pSparePages->m_sSparePages[i].m_bIsTaken)
			continue;

		pCpu->m_pSparePages->m_sSparePages[i].m_bIsTaken = true;
		return &pCpu->m_pSparePages->m_sSparePages[i];
	}

	return nullptr;
}

PHOOK_PAGE VmxUtils::FindFreeHookPage(PVIRTUAL_CPU pCpu)
{
	for (uint16_t i = 0; i < sizeof(pCpu->m_pSparePages->m_sHookPages) / sizeof(HOOK_PAGE); i++)
	{
		if (pCpu->m_pSparePages->m_sHookPages[i].m_bIsTaken)
			continue;

		pCpu->m_pSparePages->m_sHookPages[i].m_bIsTaken = true;
		return &pCpu->m_pSparePages->m_sHookPages[i];
	}
}

bool VmxUtils::ReleaseSparePage(PVIRTUAL_CPU pCpu, uint16_t PageId)
{
	pCpu->m_pSparePages->m_sSparePages[PageId].m_bIsTaken = false;
	RtlZeroMemory(pCpu->m_pSparePages->m_sSparePages[PageId].m_pPage, PAGE_SIZE);
	return true;
}

PEPROCESS GetNextProcess(PEPROCESS input)
{
	const PLIST_ENTRY currentListEntry = reinterpret_cast<PLIST_ENTRY>(reinterpret_cast<ULONG64>(input) + 0x448);
	PLIST_ENTRY nextListEntry = currentListEntry->Flink;
	return reinterpret_cast<PEPROCESS>(reinterpret_cast<ULONG64>(nextListEntry) - 0x448);
}

PEPROCESS FindProcess(const HANDLE processId)
{
	for (PEPROCESS current = PsInitialSystemProcess; current != nullptr; current = GetNextProcess(current))
	{
		const HANDLE currentId = PsGetProcessId(current);
		if (currentId == processId)
			return current;
	}

	return nullptr;
}

/*
*	Must be called on VMExit only
*/

uint64_t VmxUtils::FindUserCommPhysicalPage(uint32_t ProcessId, uint64_t Address)
{
	_KPROCESS* kProcess{ nullptr };
	PVIRTUAL_CPU pCurrentCpu{ nullptr };
	PSPARE_PAGE pCurrentPage{ nullptr };

	uint64_t DirectoryAddress{ 0 };

	if (!Address)
		KeBugCheck(KMODE_EXCEPTION_NOT_HANDLED);

	pCurrentCpu = reinterpret_cast<PVIRTUAL_CPU>(__readfsqword(0x8));

	kProcess = FindProcess((const HANDLE)ProcessId);

	if (!kProcess->DirectoryTableBase)
		KeBugCheck(KMODE_EXCEPTION_NOT_HANDLED);

	uint64_t original_cr3 = __readcr3();
	__writecr3(kProcess->DirectoryTableBase);

	DirectoryAddress = /*(uint64_t)Get_Pte(Address);*/MmGetPhysicalAddress((PVOID)Address).QuadPart;
	
	__writecr3(original_cr3);

	/*if (!DirectoryAddress)
		KeBugCheck(KMODE_EXCEPTION_NOT_HANDLED);*/

	return DirectoryAddress;
}

bool VmxUtils::VmIsCplZero(void)
{
	uint64_t CSValue{ 0 };
	SEGMENT_SELECTOR CSSelector{ 0 };

	__vmx_vmread(VMCS_GUEST_CS_SELECTOR, &CSValue);

	CSSelector.Flags = CSValue;

	/* That means that the code is executing with CPL0 */
	if (CSSelector.Index == 0 && CSSelector.RequestPrivilegeLevel == 0)
		return true;

	return false;
}
