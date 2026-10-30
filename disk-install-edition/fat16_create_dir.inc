; ============================================
; fat16_create_dir.inc - Création d'un répertoire FAT16
; ============================================

fat16_new_dir_cluster dw 0
fat16_new_dir_parent dw 0
fat16_new_dir_lba dd 0
fat16_new_dir_sector db 0

fat16_create_dir:
    mov ax, [current_cluster]
    mov [fat16_new_dir_parent], ax
    call fat16_find_free
    cmp ax, 0xFFFF
    je .error
    mov [fat16_new_dir_cluster], ax
    mov dx, 0xFFFF
    call fat16_set
    jc .error

    mov ax, [fat16_new_dir_cluster]
    xor bx, bx
    mov dl, 0x10
    call fat16_create_entry
    jc .error

    movzx eax, word [fat16_new_dir_cluster]
    sub eax, 2
    movzx ebx, byte [bpb_sectors_per_cluster]
    imul eax, ebx
    add eax, [lba_data]
    mov [fat16_new_dir_lba], eax

    push cs
    pop es
    mov di, 0x7000
    xor ax, ax
    mov cx, 256
    rep stosw

    mov di, 0x7000
    mov al, '.'
    stosb
    mov al, ' '
    mov cx, 10
    rep stosb
    mov byte [di], 0x10
    mov ax, [fat16_new_dir_cluster]
    mov [0x7000+26], ax

    mov di, 0x7020
    mov al, '.'
    stosb
    stosb
    mov al, ' '
    mov cx, 9
    rep stosb
    mov byte [di], 0x10
    mov ax, [fat16_new_dir_parent]
    mov [0x7020+26], ax

    mov byte [fat16_new_dir_sector], 0
.write_sector:
    mov eax, [fat16_new_dir_lba]
    mov cx, 1
    mov bx, 0x7000
    push cs
    pop es
    call lba_write
    jc .error

    inc dword [fat16_new_dir_lba]
    inc byte [fat16_new_dir_sector]
    mov al, [fat16_new_dir_sector]
    cmp al, [bpb_sectors_per_cluster]
    jae .done

    push cs
    pop es
    mov di, 0x7000
    xor ax, ax
    mov cx, 256
    rep stosw
    jmp .write_sector

.done:
    xor ax, ax
    clc
    ret
.error:
    mov ax, 0xFFFF
    stc
    ret
