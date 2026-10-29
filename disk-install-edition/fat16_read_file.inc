; ============================================
; fat16_read_file.inc - Lire un fichier FAT16
; ============================================

read_cluster dw 0
read_buffer_seg dw 0
read_buffer_off dw 0
read_lba dd 0

fat16_read_file:

    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es

    mov [read_cluster], ax

    mov ax, 0x1000
    mov [read_buffer_seg], ax
    mov word [read_buffer_off], 0xA000

.read_loop:

    mov ax, [read_cluster]
    cmp ax, 2
    jb .error
    cmp ax, 0xFFF0
    jae .error

    movzx eax, ax
    sub eax, 2
    movzx ebx, byte [bpb_sectors_per_cluster]
    imul eax, ebx
    add eax, [lba_data]
    mov [read_lba], eax

    mov eax, [read_lba]
    movzx cx, byte [bpb_sectors_per_cluster]
    mov es, [read_buffer_seg]
    mov bx, [read_buffer_off]
    call lba_read
    jc .error

    movzx eax, byte [bpb_sectors_per_cluster]
    shl eax, 9
    movzx ebx, word [read_buffer_off]
    add eax, ebx
    mov bx, ax
    mov [read_buffer_off], bx
    shr eax, 16
    shl ax, 12
    add [read_buffer_seg], ax

    mov ax, [read_cluster]
    call fat16_get
    jc .error
    mov [read_cluster], ax

    cmp ax, 0xFFF8
    jae .done

    cmp ax, 2
    jb .error
    cmp ax, 0xFFF0
    jae .error

    jmp .read_loop

.done:

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