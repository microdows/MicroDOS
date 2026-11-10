; ============================================================================
; boot.asm - Secteur de demarrage de MicroDOS (disquette)
;
; Le BPB est rempli par mkfs.fat (quick.sh n'ecrit que le code). On calcule
; ou commence la zone de donnees avec ce BPB, puis on charge KERNEL.BIN
; (le premier fichier copie sur la disquette) en 0x1000:0000.
; Marche pour toutes les tailles de disquette : 360 Ko, 720 Ko, 1,2 Mo,
; 1,44 Mo et 2,88 Mo.
; ============================================================================
[BITS 16]
[ORG 0x7C00]

KERNEL_SECTORS equ 56           ; 28 Ko (voir KERNEL_MAX dans fs.inc)

    jmp short start
    nop

    OEMName         db 'MicroDOS'
    BytesPerSector  dw 512
    SectorsPerClust db 1
    ReservedSectors dw 1
    NumFATs         db 2
    RootEntries     dw 224
    TotalSectors16  dw 2880
    MediaDescriptor db 0xF0
    SectorsPerFAT   dw 9
    SectorsPerTrack dw 18
    NumHeads        dw 2
    HiddenSectors   dd 0
    TotalSectors32  dd 0
    DriveNumber     db 0
    Reserved1       db 0
    BootSignature   db 0x29
    VolumeID        dd 0x12345678
    VolumeLabel     db 'MICRODOS   '
    FileSystem      db 'FAT12   '

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    mov [DriveNumber], dl       ; lecteur donne par le BIOS

    mov si, msg
    call print

    ; debut des donnees = reserves + FATs + racine
    mov al, [NumFATs]
    cbw
    mul word [SectorsPerFAT]
    add ax, [ReservedSectors]
    mov bx, ax
    mov ax, [RootEntries]
    add ax, 15
    mov cl, 4
    shr ax, cl
    add ax, bx                  ; AX = premier secteur de donnees (LBA)

    mov bx, 0x1000
    mov es, bx
    xor bx, bx
    mov bp, KERNEL_SECTORS
.read:
    push ax
    ; LBA -> CHS
    xor dx, dx
    div word [SectorsPerTrack]
    mov cl, dl
    inc cl                      ; secteur
    xor dx, dx
    div word [NumHeads]
    mov dh, dl                  ; tete
    mov ch, al                  ; cylindre
    mov dl, [DriveNumber]
    mov di, 3                   ; 3 essais
.try:
    mov ax, 0x0201
    int 0x13
    jnc .ok
    xor ax, ax
    int 0x13
    dec di
    jnz .try
    jmp error
.ok:
    pop ax
    inc ax
    add bx, 512
    dec bp
    jnz .read

    mov dl, [DriveNumber]
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

msg       db 'Chargement de MicroDOS...', 13, 10, 0
msg_error db 'Erreur de Lecture!', 13, 10, 0

times 510-($-$$) db 0
dw 0xAA55
