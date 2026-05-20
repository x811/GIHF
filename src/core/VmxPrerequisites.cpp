#include "stdafx.h"

bool VmxDeps::set_vmxe(void)
{
	CR4 cr4;

	cr4.Flags = __readcr4();

	cr4.VmxEnable = 1;
	cr4.OsXsave = 1;

	__writecr4(cr4.Flags);
	return TRUE;
}

bool VmxDeps::set_fixed_bits(void)
{
	CR0 cr0;
	CR4 cr4;
	IA32_FEATURE_CONTROL_REGISTER feature_ctrl;

	cr0.Flags = __readcr0();
	cr4.Flags = __readcr4();

	cr0.Flags |= __readmsr(IA32_VMX_CR0_FIXED0);
	cr0.Flags &= __readmsr(IA32_VMX_CR0_FIXED1);

	cr4.Flags |= __readmsr(IA32_VMX_CR4_FIXED0);
	cr4.Flags &= __readmsr(IA32_VMX_CR4_FIXED1);

	__writecr0(cr0.Flags);
	__writecr4(cr4.Flags);

	feature_ctrl.Flags = __readmsr(IA32_FEATURE_CONTROL);
	if (feature_ctrl.LockBit == 0)
	{
		feature_ctrl.LockBit = 1;
		feature_ctrl.EnableVmxOutsideSmx = 1;

		__writemsr(IA32_FEATURE_CONTROL, feature_ctrl.Flags);
		return TRUE;
	}

	return TRUE;
}

/*
*	This project doesn't support a regular APIC (at least now)
*/

bool VmxDeps::IsExtendedApic(void)
{
	IA32_APIC_BASE_REGISTER ApicRegister{ 0 };

	ApicRegister.Flags = __readmsr(IA32_APIC_BASE);

	if (!ApicRegister.EnableX2ApicMode)
		return false;

	return true;
}

uint32_t VmxDeps::RetrieveMaxPhyWidth(void)
{
	int32_t regs[4]{ 0 };

	__cpuid(regs, 0x80000008);
	
	return regs[0] & 0xFF;
}
