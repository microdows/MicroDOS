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

    mov ah, 0x42
    mov dl, 0x80
    mov si, dap
    int 0x13
    jc error

    jmp 0x0000:0x7E00

error:
    jmp $

dap:
    db 0x10, 0x00
    dw 1
    dw 0x7E00, 0x0000
    dq 2048

times 446-($-$$) db 0
; Partition 1 : Bootable, Type FAT16, LBA 2048, Taille 63488
db 0x80
db 0, 0, 0
db 0x06
db 0, 0, 0
dd 2048
dd 63488
times 48 db 0
dw 0xAA55