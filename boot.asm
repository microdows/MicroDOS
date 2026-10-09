[BITS 16]
[ORG 0x7C00]

	jmp short start
	nop

	OEMName		db"MicroDOS"
	BytesPerSector dw 512
	SectorsPerClust db 1
	ReservedSectors dw 1
	NumFATs		db 2
	RootEntries dw 224
	TotalSectors16 dw 2880
	MediaDescriptor db 0xF0
	SectorsPerFAT dw 9
	SectorsPerTrack dw 18
	NumHeads dw 2
	HiddenSectors dd 0
	TotalSectors32 dd 0
	DriveNumber db 0
	Reserved1 db 0
	BootSignature db 0x29
	VolumeID dd 0x12345678
	VolumeLabel db 'MICRODOS   '
	FileSystem db 'FAT12   '

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
	mov al, 9
	mov ch, 0
	mov cl, 16
	mov dh, 1
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

msg db 'Chargement de MicroDOS...', 13, 10, 0
msg_error db 'Erreur de Lecture!', 13, 10, 0

times 510-($-$$) db 0
dw 0xAA55


