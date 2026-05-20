#pragma once

namespace VmxExtendedPaging
{
	uint8_t VmGetMtrrMemoryTypeForAddress(const PVMM_CONTEXT pVmm, PVIRTUAL_CPU pCpu, uint64_t PhysAddress);
	uint64_t VmFindEptEntryByAddress(PEPT_GUEST_PAGING pGuestEptPaging, uint64_t GuestPhysicalBase, uint64_t VirtualAddress, bool* IsLargeEntry = nullptr);

	void VmSetupEptPagingStructures(PVIRTUAL_CPU pCpu);
	void VmSetupEptIdentityPaging(PVIRTUAL_CPU pCpu);

	bool VmSplitLargePage(PVIRTUAL_CPU pCpu, EPDE_2MB* pPdLargeEntry);
	bool VmAssembleLargePage(PVIRTUAL_CPU pCpu, EPDE* pPdEntry);
	
	bool VmHookPage(PVIRTUAL_CPU pCpu, uint64_t VirtualAddress);

	bool VmHideMemory(PVIRTUAL_CPU pCpu, uint64_t PhysicalAddress);
	bool VmHideMemoryRange(PVIRTUAL_CPU pCpu, uint64_t Start, uint64_t Size);
	bool VmHideVmm(PVIRTUAL_CPU pCpu);
}