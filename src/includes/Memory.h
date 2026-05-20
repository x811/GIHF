#pragma once

#define FillHostPaging(_DATA_TO_FILL_, ...) \
	CloneAddressPagingFromGuestVirtualAddress(HostPaging, (uint64_t)_DATA_TO_FILL_, __VA_ARGS__)

template <typename T>
concept GenericEPTPageTable = requires(T t)
{
	t.ReadAccess;
	t.WriteAccess;
	t.ExecuteAccess;
	t.PageFrameNumber;
};

namespace MemoryUtils
{
	PVOID AllocatePages(SIZE_T size);
	PVOID Allocate(SIZE_T size, POOL_TYPE type = NonPagedPool);

	template<GenericEPTPageTable T>
	uint64_t AllocatePagingTableFromEntryEPT(T* PageEntry, uint64_t index)
	{
		T* pPagingBase{ nullptr };

		if (!PageEntry->PageFrameNumber)
		{
			pPagingBase = reinterpret_cast<T*>(MemoryUtils::AllocatePages(PAGE_SIZE));

			PageEntry->ReadAccess = 1;
			PageEntry->WriteAccess = 1;
			PageEntry->ExecuteAccess = 1;
			PageEntry->PageFrameNumber = MmGetPhysicalAddress(pPagingBase).QuadPart >> PAGE_SHIFT;

			return reinterpret_cast<uint64_t>(&pPagingBase[index]);
		}

		T* pPagingEntry{ nullptr };

		pPagingBase = reinterpret_cast<T*>(MmGetVirtualForPhysical({ .QuadPart = (LONGLONG)(PageEntry->PageFrameNumber << PAGE_SHIFT) }));
		pPagingEntry = reinterpret_cast<T*>(&pPagingBase[index]);

		return reinterpret_cast<uint64_t>(pPagingEntry);
	}

	void CloneAddressPagingFromGuestVirtualAddress(uint64_t HostPagingBase, uint64_t VirtualAddress, uint64_t ModuleSize = PAGE_SIZE);

	bool GetKernelModuleAddressAndSize(KMODULE_INFO* pKmodInfo, const char* ModuleName);
	NTSTATUS BBSearchPattern(IN PCUCHAR pattern, UCHAR wildcard, IN ULONG_PTR len, IN const VOID* base, IN ULONG_PTR size, OUT PVOID* ppFound);
	uint64_t GetAddrFromPattern(PCUCHAR pattern, ULONG_PTR len, int32_t NextRipOffset, int32_t MnemonicOffset);

	uint64_t VatoPa(uint64_t PageDirectoryBase, uint64_t AddressToTranslate, uint64_t* PageSize = nullptr);
}