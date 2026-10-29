; ============================================
; fat16_create_file.inc - Créer un fichier FAT16
; ============================================

fat16_create_file:
    push bx
    push cx
    push dx
    push si
    push di
    push es

    ; 1. Trouver un cluster libre
    call fat16_find_free
    cmp ax, 0xFFFF
    je .error
    mov [cluster_temp], ax

    ; 2. Marquer la fin de chaîne (0xFFFF) dans la FAT
    mov dx, 0xFFFF
    call fat16_set
    jc .error

    ; 3. Chercher une entrée libre dans le root directory
    mov eax, [lba_rootdir]
    mov [write_lba], eax
    mov cx, 32

.find_entry:
    mov eax, [write_lba]
    mov cx, 1
    mov bx, 0x6000
    push cs
    pop es
    call lba_read
    jc .error

    mov si, 0x6000
    mov di, 16

.check_entry:
    mov al, [si]
    cmp al, 0
    je .entry_found
    cmp al, 0xE5
    je .entry_found
    add si, 32
    dec di
    jnz .check_entry

    inc dword [write_lba]
    dec cx
    jnz .find_entry
    jmp .error

.entry_found:
    ; 4. Remplir l'entrée (32 octets)
    mov di, si
    mov si, fatname
    mov cx, 11
.copy_name:
    mov al, [si]
    mov [di], al
    inc si
    inc di
    loop .copy_name

    mov byte [di], 0x20     ; Attribut archive
    inc di
    mov cx, 10
    xor al, al
.reserved:
    mov [di], al
    inc di
    loop .reserved

    xor ax, ax
    mov [di], ax            ; Heure
    add di, 2
    mov [di], ax            ; Date
    add di, 2

    mov ax, [cluster_temp]
    mov [di], ax            ; Cluster de début
    add di, 2

    mov [di], bx            ; Taille (basse)
    add di, 2
    xor ax, ax
    mov [di], ax            ; Taille (haute)
    add di, 2

    ; 5. Réécrire le secteur modifié
    mov eax, [write_lba]
    mov cx, 1
    mov bx, 0x6000
    push cs
    pop es
    call lba_write
    jc .error

    mov ax, [cluster_temp]
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    clc
    ret

.error:
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    stc
    ret

