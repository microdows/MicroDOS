; ============================================
; fat16_ren_file.inc - Renommage FAT16
; ============================================

fat16_old_name times 11 db 0
fat16_ren_lba dd 0
fat16_ren_offset dw 0

fat16_ren_file:
    mov word [parse_skip], 4
    call parse_two_args
    jc .error
    mov si, [arg1_ptr]
    call build_fat_name
    call fat16_find_file
    jc .error

    mov al, [si+11]
    test al, 0x05
    jnz .protected
    mov eax, [fat16_sector_lba]
    mov [fat16_ren_lba], eax
    mov ax, si
    sub ax, 0x6000
    mov [fat16_ren_offset], ax
    mov di, fat16_old_name
    mov cx, 11
    rep movsb

    mov si, [arg2_ptr]
    call build_fat_name
    mov si, fat16_old_name
    mov di, fatname
    mov cx, 11
    cld
.same_name_loop:
    mov al, [si]
    cmp al, [di]
    jne .check_collision
    inc si
    inc di
    loop .same_name_loop
    jmp .same_name

.check_collision:
    call fat16_find_file
    jnc .error
.same_name:
    mov eax, [fat16_ren_lba]
    mov [fat16_sector_lba], eax
    mov cx, 1
    mov bx, 0x6000
    push cs
    pop es
    call lba_read
    jc .error

    mov di, 0x6000
    add di, [fat16_ren_offset]
    mov si, fatname
    mov cx, 11
    rep movsb

    mov eax, [fat16_ren_lba]
    mov cx, 1
    mov bx, 0x6000
    push cs
    pop es
    call lba_write
    jc .error
    xor ax, ax
    ret
.protected:
    mov ax, 0xFFFE
    ret
.error:
    mov ax, 0xFFFF
    ret
