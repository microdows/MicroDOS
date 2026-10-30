; ============================================
; fat16_del_file.inc - Suppression FAT16
; ============================================

fat16_del_cluster dw 0
fat16_del_next dw 0
fat16_chain_guard dw 0
fat16_del_parent_lba dd 0
fat16_del_parent_offset dw 0
fat16_del_parent_cluster dw 0

fat16_free_cluster_chain:
    mov [fat16_del_cluster], ax
    mov word [fat16_chain_guard], 0xFFF0
.free_loop:
    mov ax, [fat16_del_cluster]
    cmp ax, 2
    jb .done
    cmp ax, 0xFFF0
    jae .done
    cmp word [fat16_chain_guard], 0
    je .error
    dec word [fat16_chain_guard]

    call fat16_get
    jc .error
    mov [fat16_del_next], ax
    mov ax, [fat16_del_cluster]
    xor dx, dx
    call fat16_set
    jc .error
    mov ax, [fat16_del_next]
    mov [fat16_del_cluster], ax
    jmp .free_loop
.done:
    clc
    ret
.error:
    stc
    ret

fat16_delete_current_entry:
    mov [fat16_del_cluster], ax
    mov byte [si], 0xE5
    mov eax, [fat16_sector_lba]
    mov cx, 1
    mov bx, 0x6000
    push cs
    pop es
    call lba_write
    jc .error

    mov ax, [fat16_del_cluster]
    cmp ax, 2
    jb .done
    call fat16_free_cluster_chain
    ret
.done:
    clc
    ret
.error:
    stc
    ret

fat16_del_file:
    call fat16_find_file
    jc .error
    mov al, [si+11]
    test al, 0x10
    jnz .is_dir
    test al, 0x05
    jnz .protected
    mov ax, [si+26]
    call fat16_delete_current_entry
    jc .error
    xor ax, ax
    ret
.is_dir:
    mov ax, 0xFFFD
    ret
.protected:
    mov ax, 0xFFFE
    ret
.error:
    mov ax, 0xFFFF
    ret

fat16_del_dir:
    call fat16_find_file
    jc .error
    test byte [si+11], 0x10
    jz .error
    mov ax, [si+26]
    mov [fat16_del_cluster], ax
    mov [fat16_del_parent_cluster], ax
    mov eax, [fat16_sector_lba]
    mov [fat16_del_parent_lba], eax
    mov ax, si
    sub ax, 0x6000
    mov [fat16_del_parent_offset], ax

.check_cluster:
    mov ax, [fat16_del_cluster]
    cmp ax, 2
    jb .error
    movzx eax, ax
    sub eax, 2
    movzx ebx, byte [bpb_sectors_per_cluster]
    imul eax, ebx
    add eax, [lba_data]
    mov [fat16_sector_lba], eax
    movzx cx, byte [bpb_sectors_per_cluster]
.check_sector:
    push cx
    mov eax, [fat16_sector_lba]
    mov cx, 1
    mov bx, 0x6000
    push cs
    pop es
    call lba_read
    jc .error_pop_cx
    mov di, 0x6000
    mov cx, 16
.check_entry:
    mov al, [di]
    cmp al, 0
    je .directory_empty
    cmp al, 0xE5
    je .next_entry
    cmp al, '.'
    jne .not_empty
    cmp byte [di+1], ' '
    je .next_entry
    cmp byte [di+1], '.'
    jne .not_empty
    cmp byte [di+2], ' '
    je .next_entry
.not_empty:
    jmp .error_pop_cx
.next_entry:
    add di, 32
    loop .check_entry
.next_sector:
    pop cx
    inc dword [fat16_sector_lba]
    loop .check_sector

    mov ax, [fat16_del_cluster]
    call fat16_get
    jc .error
    cmp ax, 0xFFF8
    jae .delete_dir
    cmp ax, 2
    jb .error
    mov [fat16_del_cluster], ax
    jmp .check_cluster

.directory_empty:
    pop cx
.delete_dir:
    mov eax, [fat16_del_parent_lba]
    mov [fat16_sector_lba], eax
    mov cx, 1
    mov bx, 0x6000
    push cs
    pop es
    call lba_read
    jc .error
    mov si, 0x6000
    add si, [fat16_del_parent_offset]
    mov ax, [fat16_del_parent_cluster]
    call fat16_delete_current_entry
    jc .error
    xor ax, ax
    ret
.error_pop_cx:
    pop cx
.error:
    mov ax, 0xFFFF
    ret
