#pragma once

enum FuncCodes
{
	FUNC_ID_ESTABLISH_COMM = 1,
	FUNC_ID_HIDE_SELF,
	FUNC_ID_WATCH_PAGE,
	FUNC_ID_HOOK_PAGE,
};

namespace IsrImpl
{
	EXTERN_C void HvImplNmiInterrupt(void);
	EXTERN_C void HvImplDbgInterrupt(void);
}

namespace IsrConfig
{
	EXTERN_C void HvIsrNmiInterrupt(void);
	EXTERN_C void HvIsrDbgInterrupt(void);
}