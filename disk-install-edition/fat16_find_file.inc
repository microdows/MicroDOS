; ============================================
; fat16_find_file.inc - Chercher dans le répertoire FAT16 courant
; ============================================

fat16_sector_lba   dd 0
fat16_cluster      dw 0
fat16_sector_index db 0
fat16_entries_left dw 0

fat16_find_file:
    push bx
    push cx
    push dx
    push di
    push es

    mov ax, [current_cluster]
    mov [fat16_cluster], ax
    cmp ax, 0
    jne .cluster_dir

    mov ax, [bpb_root_entries]
    mov [fat16_entries_left], ax
    mov eax, [lba_rootdir]
    mov [fat16_sector_lba], eax
    mov byte [fat16_sector_index], 0
    jmp .read_sector

.cluster_dir:
    mov byte [fat16_sector_index], 0
    call .cluster_to_lba
    mov [fat16_sector_lba], eax
    movzx ax, byte [bpb_sectors_per_cluster]
    shl ax, 4
    mov [fat16_entries_left], ax

.read_sector:
    mov eax, [fat16_sector_lba]
    mov cx, 1
    mov bx, 0x6000
    push cs
    pop es
    call lba_read
    jc .not_found

    mov si, 0x6000
    mov cx, 16
.entry_loop:
    cmp byte [si], 0
    je .not_found
    cmp byte [si], 0xE5
    je .next_entry
    cmp byte [si+11], 0x0F
    je .next_entry
    test byte [si+11], 0x08
    jnz .next_entry

    push cx
    push si
    mov di, fatname
    mov cx, 11
.compare:
    mov al, [si]
    cmp al, [di]
    jne .no_match
    inc si
    inc di
    loop .compare
    pop si
    pop cx
    pop es
    pop di
    pop dx
    pop cx
    pop bx
    clc
    ret

.no_match:
    pop si
    pop cx
.next_entry:
    add si, 32
    cmp word [fat16_cluster], 0
    jne .continue_entries
    dec word [fat16_entries_left]
    jz .not_found
.continue_entries:
    loop .entry_loop

    inc dword [fat16_sector_lba]
    inc byte [fat16_sector_index]
    cmp word [fat16_cluster], 0
    je .root_sector_limit
    mov al, [fat16_sector_index]
    cmp al, [bpb_sectors_per_cluster]
    jb .read_sector

    mov ax, [fat16_cluster]
    call fat16_get
    jc .not_found
    cmp ax, 0xFFF8
    jae .not_found
    cmp ax, 2
    jb .not_found
    cmp ax, 0xFFF0
    jae .not_found
    mov [fat16_cluster], ax
    mov byte [fat16_sector_index], 0
    call .cluster_to_lba
    mov [fat16_sector_lba], eax
    movzx ax, byte [bpb_sectors_per_cluster]
    shl ax, 4
    mov [fat16_entries_left], ax
    jmp .read_sector

.root_sector_limit:
    cmp word [fat16_entries_left], 0
    jne .read_sector

.not_found:
    pop es
    pop di
    pop dx
    pop cx
    pop bx
    stc
    ret

.cluster_to_lba:
    movzx eax, word [fat16_cluster]
    sub eax, 2
    movzx ebx, byte [bpb_sectors_per_cluster]
    imul eax, ebx
    add eax, [lba_data]
    ret
