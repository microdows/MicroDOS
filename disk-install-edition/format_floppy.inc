; ============================================
; format_floppy.inc - Formater la disquette A: en FAT12
; ============================================

floppy_format_lba dw 0
floppy_format_fat_sectors dw 0

format_floppy:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es

    push cs
    pop es
    mov bx, 0x7000
    xor ax, ax
    mov cx, 1
    mov dx, 0
    mov ah, 0x02
    mov al, 1
    mov cl, 1
    int 0x13
    jc .error

    mov di, 0x7000
    mov si, floppy_oem
    add di, 3
    mov cx, 8
    rep movsb
    mov di, 0x7000
    mov word [di+11], 512
    mov byte [di+13], 1
    mov word [di+14], 1
    mov byte [di+16], 2
    mov word [di+17], 224
    mov word [di+19], 2880
    mov byte [di+21], 0xF0
    mov word [di+22], 9
    mov word [di+24], 18
    mov word [di+26], 2
    mov dword [di+28], 0
    mov dword [di+32], 0
    mov byte [di+36], 0
    mov byte [di+37], 0
    mov byte [di+38], 0x29
    mov dword [di+39], 0x12345678
    mov si, floppy_label
    mov di, 0x7000+43
    mov cx, 11
    rep movsb
    mov si, floppy_fs_type
    mov di, 0x7000+54
    mov cx, 8
    rep movsb
    mov word [es:0x7000+510], 0xAA55

    xor ax, ax
    mov [floppy_format_lba], ax
    call .write_sector
    jc .error

    mov word [floppy_format_lba], 1
    mov word [floppy_format_fat_sectors], 18
.fat_loop:
    push cs
    pop es
    mov di, 0x7000
    xor ax, ax
    mov cx, 256
    rep stosw
    cmp word [floppy_format_lba], 1
    je .fat_header
    cmp word [floppy_format_lba], 10
    jne .write_fat
.fat_header:
    mov byte [es:0x7000], 0xF0
    mov word [es:0x7001], 0xFFFF
.write_fat:
    call .write_sector
    jc .error
    inc word [floppy_format_lba]
    dec word [floppy_format_fat_sectors]
    jnz .fat_loop

    mov word [floppy_format_lba], 19
    mov word [floppy_format_fat_sectors], 14
.root_loop:
    push cs
    pop es
    mov di, 0x7000
    xor ax, ax
    mov cx, 256
    rep stosw
    call .write_sector
    jc .error
    inc word [floppy_format_lba]
    dec word [floppy_format_fat_sectors]
    jnz .root_loop

    push cs
    pop es
    mov di, 0x7000
    xor ax, ax
    mov cx, 256
    rep stosw
    mov si, floppy_label
    mov di, 0x7000
    mov cx, 11
    rep movsb
    mov byte [es:0x7000+11], 0x08
    mov word [floppy_format_lba], 19
    call .write_sector
    jc .error

    xor ax, ax
    mov dx, 0
    int 0x13
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret

.write_sector:
    push ax
    push bx
    push cx
    push dx
    push es
    mov ax, [floppy_format_lba]
    call lba_to_chs
    mov ah, 0x03
    mov al, 1
    mov bx, 0x7000
    push cs
    pop es
    int 0x13
    pop es
    pop dx
    pop cx
    pop bx
    pop ax
    ret

.error:
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret

floppy_oem db 'MICRODOS '
floppy_label db 'MICRODOS   '
floppy_fs_type db 'FAT12   '
