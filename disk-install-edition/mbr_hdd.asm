; ============================================================================
; mbr_hdd.asm - Master Boot Record de MicroDOS (secteur 0 du disque dur)
;
; 1. se recopie en 0x0600 pour liberer 0x7C00
; 2. cherche la partition active (octet 0x80) dans la table
; 3. charge son premier secteur (le VBR) en 0x7C00 et saute dessus
;    avec DL = disque et DS:SI = entree de la partition (convention DOS)
;
; Seuls les 446 premiers octets (le code) sont ecrits par l'installateur :
; la table des partitions (octets 446-509) est geree par FDISK.
; ============================================================================
[BITS 16]
[ORG 0x0600]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    ; se recopier en 0x0600
    mov si, 0x7C00
    mov di, 0x0600
    mov cx, 256
    cld
    rep movsw
    jmp 0:relocated

relocated:
    mov [boot_drive], dl
    ; chercher la partition active
    mov si, table
    mov cx, 4
.find:
    cmp byte [si], 0x80
    je .found
    add si, 16
    loop .find
    mov si, msg_no_active
    jmp fatal

.found:
    mov [part_entry], si
    ; lire le VBR (LBA de debut de la partition) en 0x7C00
    mov eax, [si+8]
    mov [dap_lba], eax
    mov di, 3
.read:
    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, dap
    int 0x13
    jnc .loaded
    xor ax, ax
    mov dl, [boot_drive]
    int 0x13
    dec di
    jnz .read
    mov si, msg_read_err
    jmp fatal

.loaded:
    cmp word [0x7C00+510], 0xAA55
    jne .no_os
    mov si, [part_entry]
    mov dl, [boot_drive]
    jmp 0:0x7C00
.no_os:
    mov si, msg_no_os
fatal:
    lodsb
    test al, al
    jz $
    mov ah, 0x0E
    mov bx, 7
    int 0x10
    jmp fatal

boot_drive  db 0x80
part_entry  dw 0
dap         db 0x10, 0
            dw 1                ; 1 secteur
            dw 0x7C00, 0        ; vers 0000:7C00
dap_lba     dd 0, 0

msg_no_active db 'MicroDOS : aucune partition active (lancez FDISK).', 0
msg_read_err  db 'MicroDOS : erreur de lecture du disque.', 0
msg_no_os     db 'MicroDOS : partition non formatee (lancez FORMAT C:).', 0

times 446-($-$$) db 0
table:
times 64 db 0
dw 0xAA55
