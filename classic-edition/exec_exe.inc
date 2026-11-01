; Chargeur MZ .EXE de l'edition Classic (FAT12).
; load_exe_source charge le fichier en 3000:0000 et remplit src_size.

exec_exe:
    push bx
    push cx
    push dx
    push si
    push di
    push es
    push ds

    mov [old_ss], ss
    mov [old_sp], sp
    call load_exe_source
    cmp ax, 0xFFFF
    je .error

    mov ax, 0x3000
    mov es, ax
    cmp word [es:0], 0x5A4D
    jne .error

    mov ax, [es:0x02]
    cmp ax, 511
    ja .error
    mov dx, [es:0x04]
    test dx, dx
    jz .error
    cmp dx, 128
    ja .error
    test ax, ax
    jnz .partial_page
    cmp dx, 127
    ja .error
    mov ax, dx
    shl ax, 9
    jmp .check_file_size

.partial_page:
    mov bx, ax
    dec dx
    shl dx, 9
    add dx, bx
    jc .error
    mov ax, dx

.check_file_size:
    cmp ax, [src_size]
    ja .error
    mov [exe_file_size], ax
    mov ax, [es:0x08]
    cmp ax, 2
    jb .error
    cmp ax, 0x0FFF
    ja .error
    mov [exe_header_size], ax
    shl ax, 4
    mov [exe_header_bytes], ax
    cmp ax, [exe_file_size]
    jae .error

    mov ax, [es:0x0A]
    mov [exe_minalloc], ax
    mov ax, [es:0x0C]
    mov [exe_maxalloc], ax
    mov ax, [es:0x0E]
    mov [exe_ss], ax
    mov ax, [es:0x10]
    mov [exe_sp], ax
    mov ax, [es:0x14]
    mov [exe_ip], ax
    mov ax, [es:0x16]
    mov [exe_cs], ax
    mov ax, [es:0x06]
    mov [exe_reloc_count], ax
    mov ax, [es:0x18]
    mov [exe_reloc_offset], ax

    mov ax, [exe_reloc_count]
    test ax, ax
    jz .relocations_valid
    cmp word [exe_reloc_offset], 0x1C
    jb .error
    cmp ax, 0x3FFF
    ja .error
    shl ax, 2
    add ax, [exe_reloc_offset]
    jc .error
    cmp ax, [exe_header_bytes]
    ja .error

.relocations_valid:
    mov ax, [exe_file_size]
    sub ax, [exe_header_bytes]
    mov [exe_code_size], ax
    xor dx, dx
    mov bx, 16
    div bx
    test dx, dx
    jz .image_size_ready
    inc ax

.image_size_ready:
    mov [exe_size_para], ax
    add ax, 32
    jc .error
    mov [exe_base_para], ax
    mov dx, [next_free_segment]
    cmp dx, 0x9FFF
    ja .no_mem
    mov bx, 0x9FFF
    sub bx, dx
    sub bx, ax
    jc .no_mem

    mov ax, [exe_maxalloc]
    cmp ax, 0xFFFF
    je .use_available
    cmp ax, bx
    jbe .maxalloc_ready
    mov ax, bx
    jmp .maxalloc_ready
.use_available:
    mov ax, bx
.maxalloc_ready:
    cmp ax, [exe_minalloc]
    jae .additional_ready
    mov ax, [exe_minalloc]
.additional_ready:
    add ax, [exe_base_para]
    jc .error
    mov [exe_alloc_para], ax
    mov [exe_prev_free_segment], dx
    mov bx, ax
    mov ah, 0x48
    int 0x21
    jc .no_mem
    mov [exe_load_seg], ax

    mov cx, [exe_alloc_para]
    mov ax, [exe_load_seg]
    mov es, ax
    xor di, di
    cld
.clear_memory:
    mov bx, cx
    cmp bx, 0x1000
    jbe .clear_chunk
    mov bx, 0x1000
.clear_chunk:
    mov dx, bx
    shl dx, 3
    push cx
    mov cx, dx
    xor ax, ax
    rep stosw
    pop cx
    sub cx, bx
    jz .memory_cleared
    mov ax, es
    add ax, 0x1000
    mov es, ax
    xor di, di
    jmp .clear_memory
.memory_cleared:
    mov ax, [exe_load_seg]
    mov es, ax
    mov word [es:0], 0x20CD
    add ax, [exe_alloc_para]
    mov [es:2], ax

    mov ax, [exe_load_seg]
    add ax, 16
    mov [exe_code_seg], ax
    mov es, ax
    mov si, [exe_header_bytes]
    xor di, di
    mov cx, [exe_code_size]
    push ds
    mov ax, 0x3000
    mov ds, ax
    cld
    rep movsb
    pop ds
    mov cx, [exe_reloc_count]
    jcxz .jump_to_code
    mov si, [exe_reloc_offset]
.reloc_loop:
    mov ax, 0x3000
    mov es, ax
    mov bx, [es:si]
    mov ax, [es:si+2]
    add ax, [exe_code_seg]
    mov es, ax
    mov dx, [exe_code_seg]
    add [es:bx], dx
    add si, 4
    loop .reloc_loop
.jump_to_code:
    mov ax, [exe_code_seg]
    add ax, [exe_cs]
    mov [exe_entry_ptr+2], ax
    mov ax, [exe_ip]
    mov [exe_entry_ptr], ax
    mov ax, [exe_code_seg]
    add ax, [exe_ss]
    mov dx, ax
    mov bx, [exe_sp]
    mov ax, [exe_load_seg]
    mov ds, ax
    mov es, ax
    cli
    mov byte [cs:exe_active], 1
    mov ss, dx
    mov sp, bx
    sti
    jmp far [cs:exe_entry_ptr]
.error:
    mov si, msg_mz_err
    call print
    jmp .done
.no_mem:
    mov si, msg_no_mem
    call print
.done:
    pop ds
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    ret

msg_mz_err db 'Erreur chargement EXE.', 13, 10, 0
msg_no_mem db 'Pas assez de memoire.', 13, 10, 0
