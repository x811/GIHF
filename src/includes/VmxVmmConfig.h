#pragma once

namespace VmmConfig
{
	void ConstructModulePaging(PVMM_CONTEXT pVmm, const char* ModuleName);
	void ConstructHostPaging(PVMM_CONTEXT pVmm);

	void BuildHostPaging(PVIRTUAL_CPU pCpu);
}