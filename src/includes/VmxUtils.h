#pragma once

namespace VmxUtils
{
	uint64_t GetPte(uint64_t PageDirectoryBase, uint64_t AddressToTranslate);

	PSPLIT_PAGE FindSplitPageAndLockIt(PVIRTUAL_CPU pCpu, uint64_t BackLinkPfn);
	PSPARE_PAGE FindFreePageAndLockIt(PVIRTUAL_CPU pCpu);
	PHOOK_PAGE FindFreeHookPage(PVIRTUAL_CPU pCpu);

	bool ReleaseSparePage(PVIRTUAL_CPU pCpu, uint16_t PageId);

	uint64_t FindUserCommPhysicalPage(uint32_t ProcessId, uint64_t Address);

	bool VmIsCplZero(void);
}