; ============================================
; fat16_set.inc - Écrire dans la FAT16
; ============================================

fat_value         dw 0
fat_lba_offset    dd 0
fat_byte_offset   dw 0
fat_current_lba   dd 0

fat16_set:
    push ax
    push bx
    push cx
    push dx
    push si
    push es

    mov [cluster_temp], ax
    mov [fat_value], dx

    ; 1. Calcul de l'offset = cluster * 2
    movzx eax, ax
    shl eax, 1

    ; 2. Division par 512
    xor edx, edx
    mov ecx, 512
    div ecx
    ; EAX = LBA offset (secteur), EDX = byte offset (dans le secteur)

    mov [fat_lba_offset], eax
    mov [fat_byte_offset], dx

    ; 3. Écriture dans la FAT1
    mov eax, [fat_lba_offset]
    add eax, [lba_fat1]
    mov [fat_current_lba], eax
    call .write_fat_sector
    jc .error

    ; 4. Écriture dans la FAT2
    mov eax, [fat_lba_offset]
    movzx ebx, word [bpb_sectors_per_fat]
    add eax, ebx
    add eax, [lba_fat1]
    mov [fat_current_lba], eax
    call .write_fat_sector
    jc .error

    pop es
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret

.error:
    pop es
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret

.write_fat_sector:
    ; Lire le secteur dans 0x6000
    mov eax, [fat_current_lba]
    mov cx, 1
    mov bx, 0x6000
    push cs
    pop es
    call lba_read
    jc .err

    ; Modifier la valeur
    mov si, 0x6000
    add si, [fat_byte_offset]
    mov dx, [fat_value]
    mov [si], dx

    ; Réécrire le secteur
    mov eax, [fat_current_lba]
    mov cx, 1
    mov bx, 0x6000
    push cs
    pop es
    call lba_write
    jc .err

    clc
    ret
.err:
    stc
    ret