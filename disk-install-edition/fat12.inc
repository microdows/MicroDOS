; ============================================
; fat12.inc - Fonctions FAT12 (Disquette)
; ============================================

load_current_dir:
    push ax
    push bx
    push cx
    push dx
    push es
    mov ax, [current_cluster]
    test ax, ax
    jz .root
    add ax, 31
    call lba_to_chs
    jmp .load
.root:
    mov ch, 0
    mov cl, 2
    mov dh, 1
.load:
    mov ax, 0x1000
    mov es, ax
    mov bx, 0x8000
    mov dl, 0
    mov ah, 0x02
    cmp word [current_cluster], 0
    jne .one_sector
    mov al, 14
    jmp .do_read
.one_sector:
    mov al, 1
.do_read:
    int 0x13
    pop es
    pop dx
    pop cx
    pop bx
    pop ax
    ret

build_fat_name:
    push si
    push di
    mov di, fatname
    mov cx, 8
.copy_name:
    mov al, [si]
    cmp al, '.'
    je .pad_name
    cmp al, 0
    je .pad_name
    cmp al, '\'
    je .pad_name
    cmp al, '/'
    je .pad_name
    cmp al, 'a'
    jb .store_name
    cmp al, 'z'
    ja .store_name
    sub al, 32
.store_name:
    mov [di], al
    inc di
    inc si
    dec cx
    jnz .copy_name
    cmp byte [si], '.'
    jne .copy_ext
    inc si
    jmp .copy_ext
.pad_name:
    test cx, cx
    jz .skip_dot
    mov byte [di], ' '
    inc di
    dec cx
    jmp .pad_name
.skip_dot:
    cmp byte [si], '.'
    jne .copy_ext
    inc si
.copy_ext:
    mov cx, 3
.copy_ext_loop:
    mov al, [si]
    cmp al, 0
    je .pad_ext
    cmp al, '\'
    je .pad_ext
    cmp al, '/'
    je .pad_ext
    cmp al, 'a'
    jb .store_ext
    cmp al, 'z'
    ja .store_ext
    sub al, 32
.store_ext:
    mov [di], al
    inc di
    inc si
    dec cx
    jnz .copy_ext_loop
    jmp .done
.pad_ext:
    test cx, cx
    jz .done
    mov byte [di], ' '
    inc di
    dec cx
    jmp .pad_ext
.done:
    pop di
    pop si
    ret

find_file:
    call load_current_dir
    jc .err
    mov di, 0x8000
    mov bp, 224
.loop:
    mov al, [di]
    cmp al, 0
    je .not_found
    cmp al, 0xE5
    je .next
    mov al, [di+11]
    cmp al, 0x0F
    je .next
    push si
    push di
    mov si, fatname
    mov cx, 11
.cmp:
    mov al, [si]
    mov bl, [di]
    cmp al, bl
    jne .no_match
    inc si
    inc di
    loop .cmp
    pop di
    pop si
    mov si, di
    clc
    ret
.no_match:
    pop di
    pop si
.next:
    add di, 32
    dec bp
    jnz .loop
.not_found:
    stc
    ret
.err:
    stc
    ret

read_file:
    push bx
    push cx
    push dx
    push si
    push di
    push es
    call build_fat_name
    call find_file
    jc .error
    mov ax, [si + 28]
    mov [src_size], ax
    mov ax, [si + 26]
    mov [free_temp], ax
    mov di, 0xA000
.read_loop:
    mov ax, [free_temp]
    cmp ax, 0xFF8
    jae .done
    cmp ax, 2
    jb .done
    add ax, 31
    call lba_to_chs
    push di
    mov ax, 0x1000
    mov es, ax
    mov bx, di
    mov dl, 0
    mov ah, 0x02
    mov al, 1
    int 0x13
    pop di
    jc .error
    add di, 512
    mov ax, [free_temp]
    call fat_get
    mov [free_temp], ax
    jmp .read_loop
.done:
    xor ax, ax
    jmp .exit
.error:
    mov ax, 0xFFFF
.exit:
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    ret

fat_get:
    push bx
    push cx
    push dx
    push si
    push es
    mov [cluster_temp], ax
    mov bx, ax
    mov ax, ax
    shr ax, 1
    add ax, bx
    push ax
    xor dx, dx
    mov cx, 512
    div cx
    add ax, 1
    call lba_to_chs
    mov ax, 0x1000
    mov es, ax
    mov bx, 0x6000
    mov ah, 0x02
    mov al, 1
    mov dl, 0
    int 0x13
    jc .error
    pop ax
    and ax, 0x01FF
    mov si, 0x6000
    add si, ax
    mov ax, [si]
    mov bx, [cluster_temp]
    test bx, 1
    je .even
    shr ax, 4
.even:
    and ax, 0x0FFF
    pop es
    pop si
    pop dx
    pop cx
    pop bx
    ret
.error:
    xor ax, ax
    pop ax
    pop es
    pop si
    pop dx
    pop cx
    pop bx
    ret

fat_set:
    push ax
    push bx
    push cx
    push dx
    push si
    push es
    mov [cluster_temp], ax
    mov bx, ax
    push dx
    mov ax, ax
    shr ax, 1
    add ax, bx
    push ax
    xor dx, dx
    mov cx, 512
    div cx
    add ax, 1
    call lba_to_chs
    mov ax, 0x1000
    mov es, ax
    mov bx, 0x6000
    mov ah, 0x02
    mov al, 1
    mov dl, 0
    int 0x13
    jc .error
    pop ax
    and ax, 0x01FF
    mov si, 0x6000
    add si, ax
    mov ax, [si]
    pop dx
    mov bx, [cluster_temp]
    test bx, 1
    jnz .odd
    and ax, 0xF000
    and dx, 0x0FFF
    or ax, dx
    jmp .write
.odd:
    and ax, 0x000F
    shl dx, 4
    and dx, 0xFFF0
    or ax, dx
.write:
    mov [si], ax
    mov ax, 0x1000
    mov es, ax
    mov bx, 0x6000
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, 0
    mov ah, 0x03
    mov al, 1
    int 0x13
.error:
    pop es
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

lba_to_chs:
    push ax
    push bx
    xor dx, dx
    mov bx, 18
    div bx
    mov cl, dl
    inc cl
    xor dx, dx
    mov bx, 2
    div bx
    mov ch, al
    mov dh, dl
    mov dl, 0
    pop bx
    pop ax
    ret