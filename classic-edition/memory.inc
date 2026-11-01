; Gestion memoire DOS de l'edition Classic.

memory_ah48:
    mov ax, [next_free_segment]
    mov dx, ax
    add dx, bx
    jc .no_mem
    cmp dx, 0x9FFF
    ja .no_mem
    mov [next_free_segment], dx
    clc
    DOS_SET_CARRY
    jmp int21_handler.done
.no_mem:
    mov ax, 0x0008
    stc
    DOS_SET_CARRY
    jmp int21_handler.done

memory_ah49:
    clc
    DOS_SET_CARRY
    jmp int21_handler.done

memory_ah4a:
    cmp byte [cs:exe_active], 0
    je .resize_done
    mov ax, es
    cmp ax, [cs:exe_load_seg]
    jne .invalid_block
    mov dx, [cs:exe_load_seg]
    add dx, [cs:exe_alloc_para]
    cmp [cs:next_free_segment], dx
    jne .no_mem_resize
    mov ax, [cs:exe_load_seg]
    add ax, bx
    jc .no_mem_resize
    cmp ax, 0x9FFF
    ja .no_mem_resize
    mov [cs:exe_alloc_para], bx
    mov [cs:next_free_segment], ax
    mov [es:2], ax
.resize_done:
    clc
    DOS_SET_CARRY
    jmp int21_handler.done
.invalid_block:
    mov ax, 0x0007
    stc
    DOS_SET_CARRY
    jmp int21_handler.done
.no_mem_resize:
    mov ax, 0x0008
    stc
    DOS_SET_CARRY
    jmp int21_handler.done

memory_ah4c:
    pop bp
    pop es
    pop ds
    pop di
    pop dx
    pop si
    pop cx
    pop bx
    add sp, 6
    cmp byte [cs:exe_active], 0
    jne .exit_exe
    retf
.exit_exe:
    cli
    mov byte [cs:exe_active], 0
    mov ax, [cs:exe_prev_free_segment]
    mov [cs:next_free_segment], ax
    mov ax, [cs:old_ss]
    mov ss, ax
    mov sp, [cs:old_sp]
    sti
    jmp exec_exe.done

next_free_segment dw 0x4000
old_ss dw 0
old_sp dw 0
exe_active db 0
exe_header_size dw 0
exe_header_bytes dw 0
exe_file_size dw 0
exe_code_size dw 0
exe_minalloc dw 0
exe_maxalloc dw 0
exe_prev_free_segment dw 0
exe_base_para dw 0
exe_alloc_para dw 0
exe_ss dw 0
exe_sp dw 0
exe_ip dw 0
exe_cs dw 0
exe_reloc_count dw 0
exe_reloc_offset dw 0
exe_size_para dw 0
exe_load_seg dw 0
exe_code_seg dw 0
exe_entry_ptr dw 0, 0
