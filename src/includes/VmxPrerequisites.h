#pragma once

namespace VmxDeps
{
	bool set_vmxe(void);
	bool set_fixed_bits(void);

	bool IsExtendedApic(void);

	uint32_t RetrieveMaxPhyWidth(void);
}