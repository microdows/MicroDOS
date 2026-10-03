[BITS 16]
[ORG 0x7C00]

start:
	cli
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov sp, 0x7C00
	sti

	mov si, msg
	call print

	mov ax, 0x1000
	mov es, ax
	xor bx, bx

	mov ah, 0x02
	mov al, 4
	mov ch, 0
	mov cl, 2
	mov dh, 0
	mov dl, 0
	int 0x13
	jc error

	jmp 0x1000:0000

	error:
	mov si, msg_error
	call print
	jmp $


	print:
	lodsb
	or al, al
	jz .done
	mov ah, 0x0E
	int 0x10
	jmp print
	.done:
	ret

	msg	db 'Chargement de MicroDOS', 13, 10, 0
	msg_error db 'Erreur de Chargement', 13, 10, 0


	times 510-($-$$) db 0
	dw 0xAA55
