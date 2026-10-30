; ============================================
; cmd_copy.inc - Copie entre A: (FAT12) et C: (FAT16)
; ============================================

copy_saved_drive db 0
copy_saved_cluster dw 0
copy_saved_path times 64 db 0
copy_source_drive db 0
copy_target_drive db 0

cmd_copy:
    push bx
    push cx
    push dx
    push si
    push di
    push es

    mov ax, [current_cluster]
    mov [copy_saved_cluster], ax
    mov al, [current_drive]
    mov [copy_saved_drive], al
    mov si, current_path
    mov di, copy_saved_path
    mov cx, 64
    rep movsb

    mov word [parse_skip], 5
    call parse_two_args
    jc .error

    ; Resolve and read the source file.
    mov si, [arg1_ptr]
    call .select_drive
    jc .error
    mov [copy_source_drive], al
    call strip_drive_prefix
    call resolve_path
    jc .error
    mov si, [path_filename]
    cmp byte [copy_source_drive], 0
    jne .read_fat16
    call read_file
    cmp ax, 0xFFFF
    je .error
    jmp .source_ready
.read_fat16:
    call build_fat_name
    call fat16_find_file
    jc .error
    mov ax, [si+28]
    mov [src_size], ax
    mov ax, [si+26]
    call fat16_read_file
    jc .error
.source_ready:

    ; Resolve the destination in its own drive and directory.
    mov ax, [copy_saved_cluster]
    mov [current_cluster], ax
    mov al, [copy_saved_drive]
    mov [current_drive], al
    mov si, copy_saved_path
    mov di, current_path
    mov cx, 64
    rep movsb
    mov si, [arg2_ptr]
    call .select_drive
    jc .error
    mov [copy_target_drive], al
    call strip_drive_prefix
    call resolve_path
    jc .error
    mov si, [path_filename]
    cmp byte [copy_target_drive], 0
    jne .write_fat16
    call create_file
    cmp ax, 0xFFFF
    je .error
    mov bx, [src_size]
    cmp bx, 0
    je .success
    call write_file
    cmp ax, 0xFFFF
    je .error
    jmp .success
.write_fat16:
    call build_fat_name
    mov bx, [src_size]
    call fat16_create_file
    jc .error
    mov bx, [src_size]
    cmp bx, 0
    je .success
    call fat16_write_file
    jc .error

.success:
    mov si, msg_copy_ok
    call print
    jmp .restore
.error:
    mov si, msg_copy_err
    call print
.restore:
    mov ax, [copy_saved_cluster]
    mov [current_cluster], ax
    mov al, [copy_saved_drive]
    mov [current_drive], al
    mov si, copy_saved_path
    mov di, current_path
    mov cx, 64
    rep movsb
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    ret

.select_drive:
    mov al, [current_drive]
    cmp byte [si+1], ':'
    jne .drive_ready
    mov al, [si]
    and al, 0xDF
    cmp al, 'A'
    je .drive_a
    cmp al, 'C'
    jne .drive_error
    mov al, 1
    jmp .set_root
.drive_a:
    xor al, al
.set_root:
    mov [current_drive], al
    mov word [current_cluster], 0
    mov byte [current_path], 'A'
    cmp al, 0
    je .set_drive_suffix
    mov byte [current_path], 'C'
.set_drive_suffix:
    mov byte [current_path+1], ':'
    mov byte [current_path+2], '\'
    mov byte [current_path+3], 0
.drive_ready:
    clc
    ret
.drive_error:
    stc
    ret

; Remove optional "A:\" or "C:\" from the selected argument.
strip_drive_prefix:
    cmp byte [si+1], ':'
    jne .done
    add si, 2
    cmp byte [si], '\'
    jne .done
    inc si
.done:
    ret
