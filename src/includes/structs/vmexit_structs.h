#pragma once

enum EXIT_REASONS
{
	NMI = 0,
	EXTERNAL_INTERRUPT = 1,
	CPUID = 10,
	VMCALL = 18,
	VMCLEAR = 19,
	VMLAUNCH = 20,
	VMPTRLD = 21,
	VMPTRST = 22,
	VMREAD = 23,
	VMRESUME = 24,
	VMWRITE = 25,
	VMOFF = 26,
	VMON = 27,
	CR_ACCESS = 28,
	RDMSR = 31,
	WRMSR = 32,
	EPT_VIOLATION = 48,
	XSETBV = 55,
};

typedef struct _GUEST_REGISTERS
{
	//__m128 xmm_array[6];
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t r11;
	uint64_t r10;
	uint64_t r9;
	uint64_t r8;
	uint64_t rdi;
	uint64_t rsi;
	uint64_t rsp;
	uint64_t rbp;
	uint64_t rbx;
	uint64_t rdx;
	uint64_t rcx;
	uint64_t rax;
} GUEST_REGISTERS, * PGUEST_REGISTERS;

typedef struct _GUEST_STATE
{
	bool m_bShouldIncrementRip;
	bool m_bShouldInjectEvent;
	bool m_bShouldExit;
	size_t m_iExitReason;
	size_t m_iExitQualification;
	size_t m_iEventType;
	size_t m_iEventVector;

	PVMM_CONTEXT m_pVmmCtx;
	CPKPRCB m_pKprcb;
	PGUEST_REGISTERS m_pGuestRegisters;
} GUEST_STATE, *PGUEST_STATE;