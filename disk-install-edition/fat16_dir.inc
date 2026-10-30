; ============================================
; fat16_dir.inc - Affichage du répertoire FAT16 courant
; ============================================

fat16_list_cluster dw 0
fat16_list_sector db 0
fat16_list_lba dd 0
fat16_list_left dw 0
fat16_list_dir:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push es

    mov ax, [current_cluster]
    mov [fat16_list_cluster], ax
    cmp ax, 0
    jne .cluster_dir

    mov ax, [bpb_root_entries]
    add ax, 15
    shr ax, 4
    mov [fat16_list_left], ax
    mov eax, [lba_rootdir]
    mov [fat16_list_lba], eax
    mov byte [fat16_list_sector], 0
    jmp .read_sector

.cluster_dir:
    movzx ax, byte [bpb_sectors_per_cluster]
    mov [fat16_list_left], ax
    mov byte [fat16_list_sector], 0
    call .cluster_to_lba
    mov [fat16_list_lba], eax

.read_sector:
    mov eax, [fat16_list_lba]
    mov cx, 1
    mov bx, 0x6000
    push cs
    pop es
    call lba_read
    jc .error

    mov si, 0x6000
    mov bp, 16
.entry_loop:
    cmp byte [si], 0
    je .finished
    cmp byte [si], 0xE5
    je .next_entry
    mov al, [si+11]
    test al, 0x08
    jnz .next_entry
    test al, 0x06
    jnz .next_entry
    cmp byte [si], '.'
    je .next_entry

    mov di, si
    mov cx, 8
.print_name:
    mov al, [di]
    cmp al, ' '
    je .name_done
    mov ah, 0x0E
    int 0x10
    inc di
    loop .print_name
.name_done:
    cmp byte [si+8], ' '
    je .extension_done
    mov ah, 0x0E
    mov al, '.'
    int 0x10
    mov di, si
    add di, 8
    mov cx, 3
.print_extension:
    mov al, [di]
    cmp al, ' '
    je .extension_done
    mov ah, 0x0E
    int 0x10
    inc di
    loop .print_extension
.extension_done:
    test byte [si+11], 0x10
    jz .newline
    push si
    mov si, msg_dir_tag
    call print
    pop si
.newline:
    mov ah, 0x0E
    mov al, 13
    int 0x10
    mov al, 10
    int 0x10
.next_entry:
    add si, 32
    dec bp
    jnz .entry_loop

    inc dword [fat16_list_lba]
    dec word [fat16_list_left]
    jz .finished
    cmp word [fat16_list_cluster], 0
    je .read_sector
    inc byte [fat16_list_sector]
    mov al, [fat16_list_sector]
    cmp al, [bpb_sectors_per_cluster]
    jb .read_sector

    mov ax, [fat16_list_cluster]
    call fat16_get
    jc .error
    cmp ax, 0xFFF8
    jae .finished
    cmp ax, 2
    jb .error
    cmp ax, 0xFFF0
    jae .error
    mov [fat16_list_cluster], ax
    movzx eax, ax
    sub eax, 2
    movzx ebx, byte [bpb_sectors_per_cluster]
    imul eax, ebx
    add eax, [lba_data]
    mov [fat16_list_lba], eax
    mov byte [fat16_list_sector], 0
    movzx ax, byte [bpb_sectors_per_cluster]
    mov [fat16_list_left], ax
    jmp .read_sector

.finished:
    pop es
    pop bp
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
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret

.cluster_to_lba:
    movzx eax, word [fat16_list_cluster]
    sub eax, 2
    movzx ebx, byte [bpb_sectors_per_cluster]
    imul eax, ebx
    add eax, [lba_data]
    ret
