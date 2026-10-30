[BITS 16]
[ORG 0x7E00]

%ifndef KERNEL_SECTORS
%define KERNEL_SECTORS 18
%endif

jmp short start
nop

OEMName           db "MSDOS5.0"
BytesPerSector    dw 512
SectorsPerCluster db 4
ReservedSectors   dw 4
NumFATs           db 2
RootEntries       dw 512
TotalSectors16    dw 0
MediaDescriptor   db 0xF8
SectorsPerFAT     dw 64
SectorsPerTrack   dw 18
NumHeads          dw 2
HiddenSectors     dd 2048
TotalSectors32    dd 63488
DriveNumber       db 0x80
Reserved1         db 0
BootSignature     db 0x29
VolumeID          dd 0x12345678
VolumeLabel       db "MICRODOS   "
FileSystem        db "FAT16   "

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    ; Calcul direct du data_lba (2048 + 4 + 128 + 32 = 2212)
    mov dword [data_lba], 2212

    ; Lire le noyau compilé depuis le début de la zone de données FAT16
    mov word [dap+2], KERNEL_SECTORS
    mov word [dap+4], 0x0000
    mov word [dap+6], 0x1000
    mov eax, [data_lba]
    mov dword [dap+8], eax
    mov ah, 0x42
    mov dl, 0x80
    mov si, dap
    int 0x13
    jc error

    jmp 0x1000:0000

error:
    jmp $

dap:
    db 0x10, 0x00
    dw 1
    dw 0, 0
    dq 0

data_lba dd 0

times 510-($-$$) db 0
dw 0xAA55