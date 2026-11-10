; ============================================================================
; boot_hdd.asm - Secteur de demarrage de la partition MicroDOS (VBR)
;
; Le BPB est rempli par FORMAT C:. Ce code calcule ou commence la zone de
; donnees, puis charge KERNEL.BIN (le premier fichier copie par
; l'installateur) en 0x1000:0000 et lui passe DL = disque.
; ============================================================================
[BITS 16]
[ORG 0x7C00]

KERNEL_SECTORS equ 56           ; 28 Ko (voir KERNEL_MAX dans fs.inc)

    jmp short start
    nop

    OEMName           db 'MICRODOS'
    BytesPerSector    dw 512
    SectorsPerCluster db 4
    ReservedSectors   dw 1
    NumFATs           db 2
    RootEntries       dw 512
    TotalSectors16    dw 0
    MediaDescriptor   db 0xF8
    SectorsPerFAT     dw 64
    SectorsPerTrack   dw 63
    NumHeads          dw 255
    HiddenSectors     dd 2048
    TotalSectors32    dd 0
    DriveNumber       db 0x80
    Reserved1         db 0
    BootSignature     db 0x29
    VolumeID          dd 0x12345678
    VolumeLabel       db 'MICRODOS   '
    FileSystem        db 'FAT16   '

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    mov [DriveNumber], dl

    mov si, msg
    call print

    ; debut des donnees = cache + reserves + FATs + racine
    movzx eax, byte [NumFATs]
    movzx ecx, word [SectorsPerFAT]
    mul ecx
    movzx ecx, word [ReservedSectors]
    add eax, ecx
    movzx ecx, word [RootEntries]
    add ecx, 15
    shr ecx, 4
    add eax, ecx
    add eax, [HiddenSectors]
    mov [dap_lba], eax

    mov bp, KERNEL_SECTORS
.read:
    mov di, 3
.try:
    mov ah, 0x42
    mov dl, [DriveNumber]
    mov si, dap
    int 0x13
    jnc .ok
    xor ax, ax
    mov dl, [DriveNumber]
    int 0x13
    dec di
    jnz .try
    mov si, msg_error
    call print
    jmp $
.ok:
    inc dword [dap_lba]
    add word [dap_seg], 0x20    ; secteur suivant : +512 octets
    dec bp
    jnz .read

    mov dl, [DriveNumber]
    jmp 0x1000:0000

print:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    mov bx, 7
    int 0x10
    jmp print
.done:
    ret

dap     db 0x10, 0
        dw 1
        dw 0
dap_seg dw 0x1000
dap_lba dd 0, 0

msg       db 'Chargement de MicroDOS...', 13, 10, 0
msg_error db 'Erreur de lecture du disque dur !', 13, 10, 0

times 510-($-$$) db 0
dw 0xAA55
