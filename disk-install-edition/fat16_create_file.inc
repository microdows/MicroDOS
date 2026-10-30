; ============================================
; fat16_create_file.inc - Création d'entrées FAT16
; ============================================

fat16_entry_lba dd 0
fat16_entry_cluster dw 0
fat16_entry_size dw 0
fat16_entry_attr db 0
fat16_entry_dir_cluster dw 0
fat16_entry_sector_index db 0
fat16_entry_count dw 0

fat16_create_file:
    mov [fat16_entry_size], bx
    cmp word [fat16_entry_size], 0
    je .empty_file
    call fat16_find_free
    cmp ax, 0xFFFF
    je .error
    mov [fat16_entry_cluster], ax
    mov dx, 0xFFFF
    call fat16_set
    jc .error

.create_entry:
    mov ax, [fat16_entry_cluster]
    mov bx, [fat16_entry_size]
    mov dl, 0x20
    call fat16_create_entry
    jc .error
    mov ax, [fat16_entry_cluster]
    clc
    ret
.empty_file:
    mov word [fat16_entry_cluster], 0
    jmp .create_entry
.error:
    mov ax, 0xFFFF
    stc
    ret

fat16_create_entry:
    mov [fat16_entry_cluster], ax
    mov [fat16_entry_size], bx
    mov [fat16_entry_attr], dl
    call fat16_find_free_entry
    jc .error

    push cs
    pop es
    mov di, si
    mov si, fatname
    mov cx, 11
    rep movsb
    mov al, [fat16_entry_attr]
    stosb
    mov cx, 10
    xor al, al
    rep stosb
    xor ax, ax
    stosw
    stosw
    mov ax, [fat16_entry_cluster]
    stosw
    mov ax, [fat16_entry_size]
    stosw
    xor ax, ax
    stosw

    mov eax, [fat16_entry_lba]
    mov cx, 1
    mov bx, 0x6000
    push cs
    pop es
    call lba_write
    ret
.error:
    stc
    ret

fat16_find_free_entry:
    mov ax, [current_cluster]
    mov [fat16_entry_dir_cluster], ax
    cmp ax, 0
    jne .cluster_dir

    mov ax, [bpb_root_entries]
    mov [fat16_entry_count], ax
    mov eax, [lba_rootdir]
    mov [fat16_entry_lba], eax
    mov byte [fat16_entry_sector_index], 0
    jmp .read_sector

.cluster_dir:
    mov byte [fat16_entry_sector_index], 0
    movzx eax, word [fat16_entry_dir_cluster]
    sub eax, 2
    movzx ebx, byte [bpb_sectors_per_cluster]
    imul eax, ebx
    add eax, [lba_data]
    mov [fat16_entry_lba], eax
    movzx ax, byte [bpb_sectors_per_cluster]
    shl ax, 4
    mov [fat16_entry_count], ax

.read_sector:
    mov eax, [fat16_entry_lba]
    mov cx, 1
    mov bx, 0x6000
    push cs
    pop es
    call lba_read
    jc .error

    mov si, 0x6000
    mov cx, 16
.entry_loop:
    cmp byte [si], 0
    je .found
    cmp byte [si], 0xE5
    je .found
    add si, 32
    cmp word [fat16_entry_dir_cluster], 0
    jne .continue_entries
    dec word [fat16_entry_count]
    jz .no_space
.continue_entries:
    loop .entry_loop

    inc dword [fat16_entry_lba]
    inc byte [fat16_entry_sector_index]
    cmp word [fat16_entry_dir_cluster], 0
    je .root_next
    mov al, [fat16_entry_sector_index]
    cmp al, [bpb_sectors_per_cluster]
    jb .read_sector

    mov ax, [fat16_entry_dir_cluster]
    call fat16_get
    jc .error
    cmp ax, 0xFFF8
    jae .no_space
    cmp ax, 2
    jb .error
    cmp ax, 0xFFF0
    jae .error
    mov [fat16_entry_dir_cluster], ax
    mov byte [fat16_entry_sector_index], 0
    movzx eax, ax
    sub eax, 2
    movzx ebx, byte [bpb_sectors_per_cluster]
    imul eax, ebx
    add eax, [lba_data]
    mov [fat16_entry_lba], eax
    jmp .read_sector

.root_next:
    cmp word [fat16_entry_count], 0
    jne .read_sector
.no_space:
    stc
    ret
.found:
    clc
    ret
.error:
    stc
    ret
