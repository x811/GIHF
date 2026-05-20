#pragma once

/*
 * VMCS Control Related Fields
 */

namespace VmcsConfig
{
	uint64_t HvEncodeFieldBits(uint64_t ControlRegisterValue, uint64_t MsrToReadFrom);
	IA32_VMX_PINBASED_CTLS_REGISTER HvSetupPinBasedControls(const PVMM_CONTEXT pVmm);
	IA32_VMX_PROCBASED_CTLS_REGISTER HvSetupPrimaryProcessorControls(const PVMM_CONTEXT pVmm);
	IA32_VMX_PROCBASED_CTLS2_REGISTER HvSetupSecondaryProcessorControls(const PVMM_CONTEXT pVmm);
	IA32_VMX_ENTRY_CTLS_REGISTER HvSetupEntryControls(const PVMM_CONTEXT pVmm);
	IA32_VMX_EXIT_CTLS_REGISTER HvSetupExitControls(const PVMM_CONTEXT pVmm);
	uint32_t HvSetupGuestArea(uint64_t guest_rip, uint64_t guest_rsp);
	uint32_t HvSetupHostArea(const PVIRTUAL_CPU pCpu, uint64_t host_rip, uint64_t host_rsp);
	uint32_t HvSetupControlArea(const PVIRTUAL_CPU pCpu);
	NTSTATUS SwitchToVmxAndSetLoadVmcs(PVIRTUAL_CPU pCpu);
	NTSTATUS HvSetupVmcsStructure(PVIRTUAL_CPU pCpu, uint64_t host_rip, uint64_t host_rsp, uint64_t guest_rip, uint64_t guest_rsp);
}