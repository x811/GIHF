.code

extern VmExitHandler: PROC

PUSHAQ MACRO
	push rax
	push rcx
	push rdx
	push rbx
	push -1
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
	pop rbx
	pop rbx
	pop rdx
	pop rcx
	pop rax
	ENDM

__invept proc
	invept rcx, oword ptr[rdx]
	jz fail
	jc fail_with_code
	mov rax, 0
	ret

fail_with_code:
	hlt
	mov rax, 1
	ret

fail:
	hlt
	mov rax, 2
	ret
	
__invept endp

__pause proc
	pause
	ret
__pause endp

VmExitStub proc
	PUSHAQ

	;sub rsp, 68h
	;movaps xmmword ptr [rsp + 0], xmm0
	;movaps xmmword ptr [rsp + 10h], xmm1
	;movaps xmmword ptr [rsp + 20h], xmm2
	;movaps xmmword ptr [rsp + 30h], xmm3
	;movaps xmmword ptr [rsp + 40h], xmm4
	;movaps xmmword ptr [rsp + 50h], xmm5

	mov rcx, rsp
	sub rsp, 20h
	call VmExitHandler
	add rsp, 20h

	;movaps xmm5, xmmword ptr [rsp + 50h]
	;movaps xmm4, xmmword ptr [rsp + 40h]
	;movaps xmm3, xmmword ptr [rsp + 30h]
	;movaps xmm2, xmmword ptr [rsp + 20h]
	;movaps xmm1, xmmword ptr [rsp + 10h]
	;movaps xmm0, xmmword ptr [rsp + 0]
	;add rsp, 68h

	test al, al
	jz exit
	POPAQ
	vmresume
	jmp resume_error

exit:
	int 3

resume_error:
	int 3

VmExitStub endp

end