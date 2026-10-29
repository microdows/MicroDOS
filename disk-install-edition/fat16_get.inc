; ============================================
; fat16_get.inc - Lire une entrée FAT16
; Pour MicroDOS Disk Install Edition
; ============================================

fat16_get:

	push bx
	push cx
	push dx
	push si
	push es

	movzx eax, ax
	shl eax, 1
	xor edx, edx
	mov ecx, 512
	div ecx

	add eax, [lba_fat1]

	mov cx, 1
	mov bx, 0x6000
	push cs
	pop es
	call lba_read
	jc .error

	mov si, 0x6000
	add si, dx
	mov ax, [si]

	pop es
	pop si
	pop dx
	pop cx
	pop bx
	clc
	ret

.error:

	 pop es
         pop si
         pop dx
         pop cx
         pop bx
         stc
         ret
