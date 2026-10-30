; ============================================
; fat16_find_free.inc - Trouver un cluster libre
; ============================================

fat16_find_free:

    push bx
    push cx

    mov ax, 2

.loop:

    push ax
    call fat16_get
    pop bx
    test ax, ax
    jz .found

    mov ax, bx
    inc ax
    cmp ax, 0xFFF0
    jb .loop

    mov ax, 0xFFFF
    jmp .done

.found:

    mov ax, bx

.done:

    pop cx
    pop bx
    ret