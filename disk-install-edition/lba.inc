; ============================================
; lba.inc - Lecture/écriture disque en LBA
; Pour MicroDOS Disk Install Edition
; ============================================

dap:

	db 0x10
	db 0x00
	dw 0
	dw 0
	dw 0
	dd 0
	dd 0

lba_read:
	push ax
	push bx
	push cx
	push dx
	push si
	push ds

	push cs
	pop ds

	mov word [dap + 2], cx
	mov word [dap + 4], bx
	mov word [dap + 6], es
	mov dword [dap + 8], eax

	mov ah, 0x42
	mov dl, 0x80
	mov si, dap
	int 0x13
	jc .error

	pop ds
	pop si
	pop dx
	pop cx
	pop bx
	pop ax
	clc
	ret

.error:

	pop ds
	pop si
	pop dx
	pop cx
	pop bx
	pop ax
	stc
	ret
