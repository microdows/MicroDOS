; ============================================
; format.inc - Formater un disque FAT16
; ============================================

format_hdd:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es

    ; Recompute the fixed FAT16 layout, even if the existing BPB is invalid.
    mov word [bpb_bytes_per_sector], 512
    mov byte [bpb_sectors_per_cluster], 4
    mov word [bpb_reserved_sectors], 4
    mov byte [bpb_num_fats], 2
    mov word [bpb_root_entries], 512
    mov word [bpb_sectors_per_fat], 64
    mov eax, 2048
    call fat16_init

    ; Read the existing boot sector so formatting preserves its boot code.
    mov ax, 0x1000
    mov es, ax
    mov bx, 0x7000
    mov eax, 2048
    mov cx, 1
    call lba_read
    jc .error

    ; Update only BPB fields; keep the boot code and its jump instruction.
    mov di, 0x7000
    mov si, oem_name
    add di, 3
    mov cx, 8
    rep movsb
    mov di, 0x7000
    mov word [es:di+11], 512
    mov byte [es:di+13], 4
    mov word [es:di+14], 4
    mov byte [es:di+16], 2
    mov word [es:di+17], 512
    mov word [es:di+19], 0
    mov byte [es:di+21], 0xF8
    mov word [es:di+22], 64
    mov word [es:di+24], 18
    mov word [es:di+26], 2
    mov dword [es:di+28], 2048
    mov dword [es:di+32], 63488
    mov byte [es:di+36], 0x80
    mov byte [es:di+37], 0
    mov byte [es:di+38], 0x29
    mov dword [es:di+39], 0x12345678
    mov si, vol_label
    mov di, 0x7000 + 43
    mov cx, 11
    rep movsb
    mov si, fs_type
    mov di, 0x7000 + 54
    mov cx, 8
    rep movsb
    mov word [es:0x7000+510], 0xAA55

    mov eax, 2048
    mov cx, 1
    mov bx, 0x7000
    call lba_write
    jc .error

    ; --- 2. Écrire la FAT1 ---
    mov eax, [lba_fat1]
    mov [current_format_lba], eax
    call .write_fat_at_lba
    jc .error

    ; --- 3. Écrire la FAT2 ---
    mov eax, [lba_fat1]
    movzx ebx, word [bpb_sectors_per_fat]
    add eax, ebx
    mov [current_format_lba], eax
    call .write_fat_at_lba
    jc .error

    ; --- 4. Effacer le Root Directory ---
    mov eax, [lba_rootdir]
    mov [current_format_lba], eax
    mov cx, 32
.clear_root:
    push cx
    mov eax, [current_format_lba]
    mov cx, 1
    mov bx, 0x7000
    mov dx, 0x1000
    mov es, dx
    call lba_write
    pop cx              ; <-- FIX : pop cx AVANT le jc
    jc .error
    inc dword [current_format_lba]
    loop .clear_root

    mov ax, 0x1000
    mov es, ax
    mov di, 0x7000
    xor ax, ax
    mov cx, 256
    rep stosw
    mov si, vol_label
    mov di, 0x7000
    mov cx, 11
    rep movsb
    mov byte [es:0x7000+11], 0x08
    mov eax, [lba_rootdir]
    mov cx, 1
    mov bx, 0x7000
    call lba_write
    jc .error

    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    clc
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

.write_fat_at_lba:
    mov ax, 0x1000
    mov es, ax
    mov di, 0x7000
    mov cx, 256
    xor ax, ax
    rep stosw

    mov ax, 0x1000
    mov es, ax
    mov byte [es:0x7000], 0xF8
    mov byte [es:0x7001], 0xFF
    mov byte [es:0x7002], 0xFF
    mov byte [es:0x7003], 0xFF

    mov eax, [current_format_lba]
    mov cx, 1
    mov bx, 0x7000
    mov dx, 0x1000
    mov es, dx
    call lba_write
    jc .error
    inc dword [current_format_lba]

    mov ax, 0x1000
    mov es, ax
    mov di, 0x7000
    mov cx, 256
    xor ax, ax
    rep stosw

    mov cx, 63
.write_fat_sector:
    push cx
    mov eax, [current_format_lba]
    mov cx, 1
    mov bx, 0x7000
    mov dx, 0x1000
    mov es, dx
    call lba_write
    pop cx              ; <-- FIX : pop cx AVANT le jc
    jc .error
    inc dword [current_format_lba]
    loop .write_fat_sector
    
    mov ax, 0x1000
    mov es, ax
    mov di, 0x7000
    mov cx, 256
    xor ax, ax
    rep stosw
    
    clc
    ret
current_format_lba dd 0
oem_name db 'MSDOS5.0'
vol_label db 'MICRODOS   '
fs_type db 'FAT16   '