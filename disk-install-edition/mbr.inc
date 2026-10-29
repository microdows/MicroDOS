; ============================================
; mbr.inc - Parseur de table de partitions MBR
; Pour MicroDOS Disk Install Edition
; ============================================

mbr_find_partition:

	push bx
	push si
	push ds

	push es
	pop ds

	mov si, bx
	add si, 446

	mov cx, 4

.loop:

	mov al, [si+4]
	cmp al, 0
	je .not_found
	cmp al, 0x06
	je .found
	cmp al, 0x0E
	je .found

	add si, 16
	loop .loop

.not_found:

	pop ds
	pop si
	pop bx
	stc
	ret

.found:

	mov eax, [si+8]
	mov ecx, [si+12]

	pop ds
	pop si
	pop bx
	clc
	ret
