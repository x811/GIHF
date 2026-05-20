#include "stdafx.h"

void MsrConfig::ActivateMsrBit(PMSR_BITMAP pBitMap, uint64_t MsrId, bool WriteAccess)
{
	uint32_t msr_bit_in_bitmap = MsrId & 0x1FFF;;
	bool is_high = MsrId - 0xC0000000 > 0;

	switch (is_high)
	{
	case false:
		if (!WriteAccess)
			RtlSetBit(&pBitMap->m_pBitMapHeader, msr_bit_in_bitmap);
		else
			RtlSetBit(&pBitMap->m_pBitMapHeader, msr_bit_in_bitmap + (2048 * 8));
		break;
	case true:
		if (!WriteAccess)
			RtlSetBit(&pBitMap->m_pBitMapHeader, msr_bit_in_bitmap + (1024 * 8));
		else
			RtlSetBit(&pBitMap->m_pBitMapHeader, msr_bit_in_bitmap + (3072 * 8));
		break;
	default:
		break;
	}
}
