.code 

extern prepare_for_virtualization: PROC

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

BeginVirtualization proc
	
	pushfq
	PUSHAQ

	mov rdx, rsp
	mov r8, guest_resumes_here

	sub rsp, 20h
	call prepare_for_virtualization
	add rsp, 20h

	POPAQ
	popfq

	mov rax, 0
	ret

guest_resumes_here:
	POPAQ
	popfq

	mov rax, 1
	ret

BeginVirtualization endp

__read_cs proc
	mov ax, cs
	ret
__read_cs endp

__read_es proc
	mov ax, es
	ret
__read_es endp

__read_ds proc
	mov ax, ds
	ret
__read_ds endp

__read_ss proc
	mov ax, ss
	ret
__read_ss endp

__read_fs proc
	mov ax, fs
	ret
__read_fs endp

__read_gs proc
	mov ax, gs
	ret
__read_gs endp

__read_tr proc
	str ax
	ret
__read_tr endp

__read_ldtr proc
	sldt ax
	ret
__read_ldtr endp

__load_ar proc
	lar rax, rcx
	jz success
	xor rax, rax
success:
	ret
__load_ar endp

end