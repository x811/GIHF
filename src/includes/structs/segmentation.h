#pragma once
#include <cstdint>
#include <ia32.h>

typedef struct _VMX_SEGMENT_DESCRIPTOR
{
	SEGMENT_SELECTOR m_sSelector;
	VMX_SEGMENT_ACCESS_RIGHTS m_sAccessRights;
	uint64_t m_iBase;
	uint32_t m_iLimit;
} VMX_SEGMENT_DESCRIPTOR, *PVMX_SEGMENT_DESCRIPTOR;