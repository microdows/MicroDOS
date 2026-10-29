; ============================================
; cmd_copy.inc - Commande COPY (A: vers C:)
; ============================================

cmd_copy:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es

    ; Sauvegarde de l'état actuel
    mov ax, [current_cluster]
    push ax
    mov al, [current_drive]
    push ax

    ; --- 1. LECTURE SUR A: ---
    mov word [current_cluster], 0
    mov byte [current_drive], 0

    mov si, [arg1_ptr]
    call strip_drive_prefix
    
    ; Si le nom est vide, on utilise celui de destination
    cmp byte [si], 0
    jne .source_ready
    mov si, [arg2_ptr]
    call strip_drive_prefix
.source_ready:
    call build_fat_name
    call find_file
    jc .error

    call read_file
    cmp ax, 0xFFFF
    je .error

    ; Restauration de l'état
    pop ax
    mov [current_drive], al
    pop ax
    mov [current_cluster], ax

    ; --- 2. ÉCRITURE SUR C: ---
    mov si, [arg2_ptr]
    call strip_drive_prefix
    call build_fat_name

    mov bx, [src_size]
    call fat16_create_file
    jc .error

    mov bx, [src_size]
    call fat16_write_file
    jc .error

    mov si, msg_copy_ok
    call print
    jmp .done

.error:
    mov si, msg_copy_err
    call print

.done:
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; --- Enlève "A:\" ou "C:\" au début de la chaîne ---
strip_drive_prefix:
    cmp byte [si+1], ':'
    jne .done
    add si, 2
    cmp byte [si], '\'
    jne .done
    inc si
.done:
    ret