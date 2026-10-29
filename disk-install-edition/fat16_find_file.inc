; ============================================
; fat16_find_file.inc - Chercher un fichier FAT16
; ============================================

fat16_sector_lba   dd 0
fat16_sectors_left dw 0

fat16_find_file:
    push bx
    push cx
    push dx
    push si
    push di
    push ds

    push ds
    pop es

    ; Nombre de secteurs du root directory (root_entries / 16)
    mov ax, [bpb_root_entries]
    shr ax, 4
    mov [fat16_sectors_left], ax

    ; LBA de départ du root directory
    mov eax, [lba_rootdir]
    mov [fat16_sector_lba], eax

.sector_loop:
    ; Lire 1 secteur du répertoire dans le buffer 0x6000
    mov eax, [fat16_sector_lba]
    mov cx, 1
    mov bx, 0x6000
    call lba_read
    jc .not_found

    mov si, 0x6000
    mov cx, 16

.entry_loop:
    ; Entrée vide = fin du répertoire
    cmp byte [si], 0
    je .not_found

    ; Entrée supprimée = passer à la suivante
    cmp byte [si], 0xE5
    je .next_entry

    ; Comparaison des 11 caractères
    push cx
    push si
    mov di, fatname
    mov cx, 11

.cmp_loop:
    mov al, [si]
    cmp al, [di]
    jne .no_match
    inc si
    inc di
    loop .cmp_loop

    ; --- TROUVÉ ---
    pop si      ; Ignore le SI de la boucle
    pop cx      ; Restaure CX de la boucle
    pop ds      ; Restaure DS
    pop di      ; Restaure DI
    add sp, 2   ; Saute le push si (on veut GARDER SI pointant sur l'entrée !)
    pop dx      ; Restaure DX
    pop cx      ; Restaure CX
    pop bx      ; Restaure BX
    clc
    ret

.no_match:
    pop si
    pop cx

.next_entry:
    add si, 32
    loop .entry_loop

    ; Secteur suivant
    mov eax, [fat16_sector_lba]
    inc eax
    mov [fat16_sector_lba], eax
    dec word [fat16_sectors_left]
    jnz .sector_loop

.not_found:
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    stc
    ret