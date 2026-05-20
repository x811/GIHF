#include "stdafx.h"

PVOID MemoryUtils::AllocatePages(SIZE_T size)
{
	PHYSICAL_ADDRESS low, high, bound;
	low.QuadPart = bound.QuadPart = 0;
	high.QuadPart = MAXUINT64;

	PVOID pMemory = MmAllocateContiguousMemorySpecifyCacheNode(size, low, high, bound, MmNonCached, MM_ANY_NODE_OK);
	if (!pMemory)
		return NULL;

	RtlZeroMemory(pMemory, size);
	return pMemory;
}

PVOID MemoryUtils::Allocate(SIZE_T size, POOL_TYPE type)
{
	PVOID pMemory = ExAllocatePool(type, size);

	if (!pMemory)
		return nullptr;

	RtlZeroMemory(pMemory, size);
	return pMemory;
}

template <typename T>
concept GenericPageTable = requires(T t)
{
	t.Present;
	t.PageFrameNumber;
};

template<GenericPageTable T>
uint64_t AllocatePagingTableFromEntry(T* PageEntry, uint64_t index)
{
	T* pPagingBase{ nullptr };

	if (!PageEntry->Present || !PageEntry->PageFrameNumber)
	{
		pPagingBase = reinterpret_cast<T*>(MemoryUtils::AllocatePages(PAGE_SIZE));

		PageEntry->Present = 1;
		PageEntry->Write = 1;
		PageEntry->PageFrameNumber = MmGetPhysicalAddress(pPagingBase).QuadPart >> PAGE_SHIFT;

		return reinterpret_cast<uint64_t>(&pPagingBase[index]);
	}

	T* pPagingEntry{ nullptr };

	pPagingBase = reinterpret_cast<T*>(MmGetVirtualForPhysical({ .QuadPart = (LONGLONG)(PageEntry->PageFrameNumber << PAGE_SHIFT) }));
	pPagingEntry = reinterpret_cast<T*>(&pPagingBase[index]);

	return reinterpret_cast<uint64_t>(pPagingEntry);
}

/*
	Creates own paging hierarchy to map the specified virtual address from the specified already existing
	mapping from the Guest CR3 to the specified HostPagingBase which is fluent in it's destination

	This is not properly tested and may cause issues on the virtual addresses that are not 2MB or 4KB(not tested actually on 4KB) aligned..
*/

void MemoryUtils::CloneAddressPagingFromGuestVirtualAddress(uint64_t HostPagingBase, uint64_t VirtualAddress, uint64_t ModuleSize)
{
	uint32_t TranslationCount{ 0 };
	uint64_t TranslatedPagesSize{ 0 };

	CR3 HostCr3{ 0 };
	VIRTUAL_ADDRESS Va{ 0 };

	PML4E_64* pPml4Base{ nullptr };
	PDPTE_64* pPdptBase{ nullptr };
	PDE_64* pPdBase{ nullptr };
	PTE_64* pPtBase{ nullptr };

	HostCr3.Flags = HostPagingBase;
	Va.Flags = VirtualAddress & ~0xFFF;

	/* This can be very dangerous because we don't know what is passed and from where it is passed */
	if (!Va.Flags)
		return;

	do
	{
		++TranslationCount;
		uint64_t PageSize{ 0 };
		uint64_t RequestedVirtualToPhysical = MemoryUtils::VatoPa(__readcr3(), Va.Flags, &PageSize);

		pPml4Base = reinterpret_cast<PML4E_64*>(HostCr3.AddressOfPageDirectory << PAGE_SHIFT);
		PML4E_64* pPml4E = reinterpret_cast<PML4E_64*>(MmGetVirtualForPhysical({ .QuadPart = (LONGLONG)&pPml4Base[Va.bits.PML4E] }));

		PDPTE_64* pPdptE = reinterpret_cast<PDPTE_64*>(AllocatePagingTableFromEntry(pPml4E, Va.bits.PDPTE));

		PDE_64* pPdE = reinterpret_cast<PDE_64*>(AllocatePagingTableFromEntry(pPdptE, Va.bits.PDE));

		if (PageSize == 2 * 1024 * 1024)
		{
			PDE_2MB_64* pLargePde = reinterpret_cast<PDE_2MB_64*>(pPdE);

			pLargePde->LargePage = 1;
			pLargePde->Write = 1;
			pLargePde->Present = 1;
			pLargePde->PageFrameNumber = RequestedVirtualToPhysical >> PAGE_SHIFT_2MB;

			TranslatedPagesSize += 2 * 1024 * 1024;
			Va.Flags += 2 * 1024 * 1024;
			continue;
		}

		PTE_64* pPtE = reinterpret_cast<PTE_64*>(AllocatePagingTableFromEntry(pPdE, Va.bits.PTE));

		pPtE->Present = 1;
		pPtE->Write = 1;
		pPtE->PageFrameNumber = RequestedVirtualToPhysical >> PAGE_SHIFT;

		TranslatedPagesSize += 4 * 1024;
		Va.Flags += 4 * 1024;
		continue;

		
	} while (TranslatedPagesSize < ModuleSize);
}

/*
	This is general purpose function that is used to retrieve any Windows 'System' process mapped
	kernel module (e.g. ntoskrnl or user modules)
*/

bool MemoryUtils::GetKernelModuleAddressAndSize(KMODULE_INFO* pKmodInfo, const char* ModuleName)
{
	NTSTATUS status{ STATUS_SUCCESS };
	PSYSTEM_MODULE_INFORMATION pSystemModules{ nullptr };
	PSYSTEM_MODULE_ENTRY pModEntry{ nullptr };
	ULONG SizeNeeded{ 0 };

	status = ZwQuerySystemInformation(SystemModuleInformation, 0, SizeNeeded, &SizeNeeded);

	pSystemModules = reinterpret_cast<PSYSTEM_MODULE_INFORMATION>(MemoryUtils::Allocate(SizeNeeded));

	status = ZwQuerySystemInformation(SystemModuleInformation, pSystemModules, SizeNeeded, &SizeNeeded);

	pModEntry = reinterpret_cast<PSYSTEM_MODULE_ENTRY>(&pSystemModules->Module);

	STRING targetModuleName;
	STRING currModuleName;

	RtlInitAnsiString(&targetModuleName, ModuleName);
	for (uint32_t i = 0; i < pSystemModules->Count; i++)
	{
		RtlInitAnsiString(&currModuleName, (PCSZ)pModEntry[i].FullPathName);

		if (strstr(currModuleName.Buffer, targetModuleName.Buffer) != NULL)
		{
			pKmodInfo->m_iBase = (uint64_t)pModEntry[i].ImageBase;
			pKmodInfo->m_iSize = (uint64_t)pModEntry[i].ImageSize;

			ExFreePool(pSystemModules);
			return true;
		}
	}

	ExFreePool(pSystemModules);
	return false;
}

NTSTATUS MemoryUtils::BBSearchPattern(IN PCUCHAR pattern, IN UCHAR wildcard, IN ULONG_PTR len, IN const VOID* base, IN ULONG_PTR size, OUT PVOID* ppFound)
{
	ASSERT(ppFound != NULL && pattern != NULL && base != NULL);
	if (ppFound == NULL || pattern == NULL || base == NULL)
		return STATUS_INVALID_PARAMETER;

	for (ULONG_PTR i = 0; i < size - len; i++)
	{
		BOOLEAN found = TRUE;
		for (ULONG_PTR j = 0; j < len; j++)
		{
			if (pattern[j] != wildcard && pattern[j] != ((PCUCHAR)base)[i + j])
			{
				found = FALSE;
				break;
			}
		}

		if (found != FALSE)
		{
			*ppFound = (PUCHAR)base + i;
			return STATUS_SUCCESS;
		}
	}

	return STATUS_NOT_FOUND;
}

uint64_t MemoryUtils::GetAddrFromPattern(PCUCHAR pattern, ULONG_PTR len, int32_t NextRipOffset, int32_t MnemonicOffset)
{
	KMODULE_INFO kMod{ 0 };
	uint64_t pLoc{ 0 };
	uint64_t RipAfterCall{ 0 };
	uint64_t pFuncAddr{ 0 };
	int32_t pFuncOffset{ 0 };

	GetKernelModuleAddressAndSize(&kMod, "ntoskrnl.exe");
	BBSearchPattern(pattern, 0xCC, len, (PVOID)kMod.m_iBase, kMod.m_iSize, (PVOID*)&pLoc);

	RipAfterCall = pLoc + NextRipOffset;
	RtlCopyMemory(&pFuncOffset, (PVOID)((uint64_t)pLoc + MnemonicOffset), NextRipOffset - MnemonicOffset);

	pFuncAddr = pFuncOffset + RipAfterCall;
	return pFuncAddr;
}


/*
	This function is designed to be used on a system process context, otherwise
	it will crash as I only explicitly take the system process context memory addresses

	It is designed to translate virtual to physical addresses (mostly for Guest)
*/

uint64_t MemoryUtils::VatoPa(uint64_t PageDirectoryBase, uint64_t AddressToTranslate, uint64_t* PageSize)
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

		if (PageSize != nullptr)
			*PageSize = 1024 * 1024 * 1024;

		return resulting_address;
	}

	uint64_t Pde_address = (uint64_t)MmGetVirtualForPhysical({ .QuadPart = (LONGLONG)((PdptE.PageFrameNumber << PAGE_SHIFT) + (Va.bits.PDE * 8)) });
	PdE.Flags = *reinterpret_cast<uint64_t*>(Pde_address);

	if (PdE.LargePage)
	{
		PDE_2MB_64 PdELarge{ 0 };
		PdELarge.Flags = PdE.Flags;
		uint64_t resulting_address = (PdELarge.PageFrameNumber << 21) + (Va.bits.PTE << PAGE_SHIFT | Va.bits.PageOffset);

		if (PageSize != nullptr)
			*PageSize = 2 * 1024 * 1024;

		return resulting_address;
	}

	uint64_t Pte_address = (uint64_t)MmGetVirtualForPhysical({ .QuadPart = (LONGLONG)((PdE.PageFrameNumber << PAGE_SHIFT) + (Va.bits.PTE * 8)) });
	PtE.Flags = *reinterpret_cast<uint64_t*>(Pte_address);

	if (!PtE.Present)
		return NULL;

	uint64_t resulting_address = (PtE.PageFrameNumber << PAGE_SHIFT) + Va.bits.PageOffset;

	if (PageSize != nullptr)
		*PageSize = 4 * 1024;

	return resulting_address;
}
