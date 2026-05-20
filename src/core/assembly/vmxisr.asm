.code

extern HvImplNmiInterrupt: proc
extern HvImplDbgInterrupt: proc

PUSHAQ MACRO
	push rax
	push rcx
	push rdx
	push rbx
	push rsp
	push rbp
	push rsi
	push rdi
	push r8
	push r9
	push r10
	push r11
	push r12
	push r13
	push r14
	push r15
	ENDM

POPAQ MACRO
	pop r15
	pop r14
	pop r13
	pop r12
	pop r11
	pop r10
	pop r9
	pop r8
	pop rdi
	pop rsi
	pop rbp
	pop rsp
	pop rbx
	pop rdx
	pop rcx
	pop rax
	ENDM

__readfsqword proc
	
	mov rax, qword ptr fs:[rcx]
	ret

__readfsqword endp

software_nmi proc
	int 2
	ret
software_nmi endp

debugbreak proc
	int 3
	ret
debugbreak endp

crash_system proc
	cli

halt_forever:
	hlt
	jmp halt_forever

crash_system endp

HvIsrNmiInterrupt proc

	PUSHAQ

	call HvImplNmiInterrupt

	POPAQ
	iretq

HvIsrNmiInterrupt endp

HvIsrDbgInterrupt proc
	
	PUSHAQ

	call HvImplDbgInterrupt

	POPAQ
	iretq

HvIsrDbgInterrupt endp

end