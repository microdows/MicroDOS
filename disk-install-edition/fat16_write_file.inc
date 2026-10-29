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
    mov [write_buffer_seg], bx
    mov ax, 0x1000
    mov [write_buffer_off], ax
    mov word [write_buffer_off], 0xA000

.write_loop:

    movzx eax, word [write_cluster_num]
    sub eax, 2
    movzx ebx, byte [bpb_sectors_per_cluster]
    imul eax, ebx
    add eax, [lba_data]
    mov [write_lba], eax

    mov eax, [write_lba]
    movzx cx, byte [bpb_sectors_per_cluster]
    mov es, [write_buffer_seg]
    mov bx, [write_buffer_off]
    call lba_write
    jc .error

    movzx ax, byte [bpb_sectors_per_cluster]
    shl ax, 9
    add [write_buffer_off], ax


    movzx ax, byte [bpb_sectors_per_cluster]
    shl ax, 9
    cmp word [write_size_rem], ax
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

    mov [write_cluster_num], ax
    jmp .write_loop

.last_cluster:

mov ax, [write_cluster_num]
mov dx, 0xFFFF
call fat16_get
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