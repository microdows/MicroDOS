; Services INT 21h de l'edition Classic (disquette FAT12 uniquement).

%macro DOS_SET_CARRY 0
    push dx
    pushf
    pop dx
    mov bp, sp
    test dx, 1
    jz %%clear
    or word [ss:bp+0x14], 1
    jmp %%done
%%clear:
    and word [ss:bp+0x14], 0xFFFE
%%done:
    pop dx
%endmacro

dos_ah06:
    cmp dl, 0xFF
    je .input
    mov al, dl
    mov ah, 0x0E
    int 0x10
    mov ah, 0x06
    jmp int21_handler.done
.input:
    mov ah, 0x01
    int 0x16
    jz .no_key
    mov ah, 0x00
    int 0x16
    mov bp, sp
    and word [ss:bp+0x14], 0xFFBF
    mov ah, 0x06
    jmp int21_handler.done
.no_key:
    xor al, al
    mov bp, sp
    or word [ss:bp+0x14], 0x0040
    mov ah, 0x06
    jmp int21_handler.done

dos_ah07:
    mov ah, 0x00
    int 0x16
    mov ah, 0x07
    jmp int21_handler.done

dos_ah25:
    push ds
    mov bl, al
    xor bh, bh
    shl bx, 1
    shl bx, 1
    xor ax, ax
    mov ds, ax
    mov [bx], dx
    mov ax, [cs:saved_user_ds]
    mov [bx+2], ax
    pop ds
    jmp int21_handler.done

dos_ah3f:
    mov [dos_io_count], cx
    mov [dos_io_remaining], cx
    mov [dos_io_buffer], dx
    mov ax, [saved_user_ds]
    mov [dos_io_user_ds], ax
    mov ax, bx
    call find_handle
    jc .error
    mov [dos_io_handle], di
    mov ax, [di+4]
    cmp [dos_io_remaining], ax
    jbe .count_ready
    mov [dos_io_remaining], ax
    mov [dos_io_count], ax
.count_ready:
    mov ax, [di]
    mov [dos_io_cluster], ax
    mov word [dos_io_sector_index], 0
.read_loop:
    cmp word [dos_io_remaining], 0
    je .done
    mov ax, [dos_io_cluster]
    call dos_read_data_sector
    jc .error
    mov cx, 512
    cmp [dos_io_remaining], cx
    jae .copy_chunk
    mov cx, [dos_io_remaining]
.copy_chunk:
    mov [dos_io_chunk], cx
    mov ax, [dos_io_user_ds]
    mov es, ax
    mov si, 0xA000
    mov di, [dos_io_buffer]
    rep movsb
    mov [dos_io_buffer], di
    push cs
    pop es
    mov cx, [dos_io_chunk]
    sub [dos_io_remaining], cx
    cmp word [dos_io_remaining], 0
    je .done
    call dos_advance_data_cluster
    jc .error
    jmp .read_loop
.done:
    mov ax, [dos_io_count]
    clc
    DOS_SET_CARRY
    jmp int21_handler.done
.error:
    mov ax, 0x0006
    stc
    DOS_SET_CARRY
    jmp int21_handler.done

dos_ah40:
    mov [dos_io_count], cx
    mov [dos_io_remaining], cx
    mov [dos_io_buffer], dx
    mov ax, [saved_user_ds]
    mov [dos_io_user_ds], ax
    mov ax, bx
    call find_handle
    jc .error
    mov [dos_io_handle], di
    mov ax, [di]
    mov [dos_io_cluster], ax
    mov word [dos_io_sector_index], 0
.write_loop:
    cmp word [dos_io_remaining], 0
    je .done
    mov cx, 512
    cmp [dos_io_remaining], cx
    jae .copy_chunk
    mov cx, [dos_io_remaining]
.copy_chunk:
    mov [dos_io_chunk], cx
    mov ax, [dos_io_user_ds]
    mov ds, ax
    mov si, [dos_io_buffer]
    mov ax, 0x1000
    mov es, ax
    mov di, 0xA000
    rep movsb
    mov [cs:dos_io_buffer], si
    push cs
    pop ds
    mov cx, [dos_io_chunk]
    mov ax, [dos_io_cluster]
    call dos_write_data_sector
    jc .error
    sub [dos_io_remaining], cx
    cmp word [dos_io_remaining], 0
    je .done
    call dos_advance_data_cluster
    jc .error
    jmp .write_loop
.done:
    call dos_update_file_size
    mov ax, [dos_io_count]
    clc
    DOS_SET_CARRY
    jmp int21_handler.done
.error:
    mov ax, 0x0005
    stc
    DOS_SET_CARRY
    jmp int21_handler.done

dos_io_count dw 0
dos_io_remaining dw 0
dos_io_buffer dw 0
dos_io_user_ds dw 0
dos_io_handle dw 0
dos_io_cluster dw 0
dos_io_sector_index dw 0
dos_io_chunk dw 0
