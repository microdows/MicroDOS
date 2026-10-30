; ============================================
; fat16_write_file.inc - Écrire un fichier FAT16
; ============================================

write_cluster_num dw 0
write_size_rem dw 0
write_buffer_seg dw 0
write_buffer_off dw 0
write_lba dd 0

fat16_write_file:

    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es
    
    mov [write_cluster_num], ax
    mov [write_size_rem], bx
    mov word [write_buffer_seg], 0x1000
    mov word [write_buffer_off], 0xA000

    cmp word [write_size_rem], 0
    jne .write_loop
    cmp word [write_cluster_num], 0
    je .success
    jmp .last_cluster

.write_loop:

    movzx eax, word [write_cluster_num]
    sub eax, 2
    movzx ebx, byte [bpb_sectors_per_cluster]
    imul eax, ebx
    add eax, [lba_data]
    mov [write_lba], eax

    movzx ax, byte [bpb_sectors_per_cluster]
    shl ax, 9
    cmp [write_size_rem], ax
    jae .full_cluster
    mov ax, [write_size_rem]
    add ax, 511
    shr ax, 9
    jmp .write_cluster

.full_cluster:
    movzx ax, byte [bpb_sectors_per_cluster]

.write_cluster:
    mov cx, ax
    mov eax, [write_lba]
    mov es, [write_buffer_seg]
    mov bx, [write_buffer_off]
    call lba_write
    jc .error

    movzx eax, byte [bpb_sectors_per_cluster]
    shl eax, 9
    movzx ebx, word [write_buffer_off]
    add eax, ebx
    mov bx, ax
    mov [write_buffer_off], bx
    shr eax, 16
    shl ax, 12
    add [write_buffer_seg], ax

    movzx ax, byte [bpb_sectors_per_cluster]
    shl ax, 9
    cmp [write_size_rem], ax
    jbe .last_cluster
    sub word [write_size_rem], ax

    call fat16_find_free
    cmp ax, 0xFFFF
    je .error

    push ax
    mov dx, ax
    mov ax, [write_cluster_num]
    call fat16_set
    pop ax
    jc .error

    mov [write_cluster_num], ax
    mov dx, 0xFFFF
    call fat16_set
    jc .error
    jmp .write_loop

.last_cluster:
    mov ax, [write_cluster_num]
    mov dx, 0xFFFF
    call fat16_set
    jc .error

.success:
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