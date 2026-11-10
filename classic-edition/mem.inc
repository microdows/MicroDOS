; ============================================================================
; mem.inc - Gestion de la memoire conventionnelle facon DOS (blocs MCB)
;
; Chaque bloc est precede d'un MCB de 16 octets :
;   +0 db 'M' (un autre bloc suit) ou 'Z' (dernier bloc)
;   +1 dw proprietaire (segment du PSP, 0 = libre, 8 = systeme)
;   +3 dw taille du bloc en paragraphes (16 octets)
;   +8 8 octets : nom du programme
; ============================================================================

MEM_START   equ 0x2000          ; premier MCB (juste apres le noyau)

mem_top     dw 0                ; segment de fin de la memoire conventionnelle
cur_psp     dw 0                ; PSP du programme en cours
root_psp    dw 0                ; PSP "systeme" (parent des programmes du shell)
mem_strategy dw 0

; mem_init : un seul grand bloc libre de MEM_START a la fin de la memoire
mem_init:
    push es
    int 0x12                    ; AX = memoire conventionnelle en Ko
    shl ax, 6                   ; -> paragraphes
    mov [mem_top], ax
    mov bx, MEM_START
    mov es, bx
    sub ax, bx
    dec ax
    mov byte [es:0], 'Z'
    mov word [es:1], 0
    mov [es:3], ax
    pop es
    ret

; mem_check : ES = MCB -> CF=1 si ce n'est pas un MCB valide
mem_check:
    cmp byte [es:0], 'M'
    je .ok
    cmp byte [es:0], 'Z'
    je .ok
    stc
    ret
.ok:
    clc
    ret

; mem_next : ES = MCB -> ES = MCB suivant, CF=1 si c'etait le dernier
mem_next:
    cmp byte [es:0], 'Z'
    je .last
    push ax
    mov ax, es
    add ax, [es:3]
    inc ax
    mov es, ax
    pop ax
    clc
    ret
.last:
    stc
    ret

; mem_merge : fusionne les blocs libres voisins
mem_merge:
    push ax
    push es
    push ds
    mov ax, MEM_START
    mov es, ax
.loop:
    call mem_check
    jc .done
    cmp byte [es:0], 'Z'
    je .done
    cmp word [es:1], 0
    jne .next
    ; le bloc suivant est-il libre aussi ?
    mov ax, es
    add ax, [es:3]
    inc ax
    mov ds, ax
    cmp word [ds:1], 0
    jne .next
    mov al, [ds:0]
    mov [es:0], al
    mov ax, [ds:3]
    inc ax
    add [es:3], ax
    jmp .loop                   ; on reessaie avec le meme bloc
.next:
    call mem_next
    jnc .loop
.done:
    pop ds
    pop es
    pop ax
    ret

; mem_split : ES = MCB, BX = nouvelle taille (< taille actuelle)
; Le reste devient un bloc libre.
mem_split:
    push ax
    push ds
    mov ax, [es:3]
    sub ax, bx
    jbe .done
    dec ax                      ; taille du reste (moins son MCB)
    push ax
    mov ax, es
    add ax, bx
    inc ax
    mov ds, ax
    pop ax
    mov [ds:3], ax
    mov word [ds:1], 0
    mov al, [es:0]
    mov [ds:0], al
    mov byte [es:0], 'M'
    mov [es:3], bx
.done:
    pop ds
    pop ax
    ret

; ---------------------------------------------------------------------------
; mem_alloc : BX = paragraphes -> AX = segment du bloc
; CF=1 : AX = code d'erreur, BX = plus grand bloc libre
; ---------------------------------------------------------------------------
mem_alloc:
    push dx
    push es
    call mem_merge
    xor dx, dx                  ; plus grand bloc libre vu
    mov ax, MEM_START
    mov es, ax
.loop:
    call mem_check
    jc .corrupt
    cmp word [es:1], 0
    jne .next
    mov ax, [es:3]
    cmp ax, dx
    jbe .not_bigger
    mov dx, ax
.not_bigger:
    cmp ax, bx
    jae .found
.next:
    call mem_next
    jnc .loop
    mov bx, dx
    mov ax, ERR_NOMEM
    pop es
    pop dx
    stc
    ret
.found:
    call mem_split
    mov ax, [cur_psp]
    test ax, ax
    jnz .owner
    mov ax, 8
.owner:
    mov [es:1], ax
    mov ax, es
    inc ax
    pop es
    pop dx
    clc
    ret
.corrupt:
    mov ax, 7
    pop es
    pop dx
    stc
    ret

; mem_free : ES = segment du bloc
mem_free:
    push ax
    push es
    mov ax, es
    dec ax
    mov es, ax
    call mem_check
    jc .bad
    mov word [es:1], 0
    pop es
    pop ax
    call mem_merge
    clc
    ret
.bad:
    pop es
    pop ax
    mov ax, ERR_BADMEM
    stc
    ret

; ---------------------------------------------------------------------------
; mem_resize : ES = segment du bloc, BX = nouvelle taille
; CF=1 : AX = code d'erreur, BX = taille maximale possible
; ---------------------------------------------------------------------------
mem_resize:
    push dx
    push es
    push ds
    call mem_merge
    mov ax, es
    dec ax
    mov es, ax
    call mem_check
    jc .bad
    cmp bx, [es:3]
    jbe .shrink
    ; agrandir : absorber le bloc suivant s'il est libre
    mov dx, [es:3]              ; taille maximale possible
    cmp byte [es:0], 'Z'
    je .too_big
    mov ax, es
    add ax, [es:3]
    inc ax
    mov ds, ax
    cmp word [ds:1], 0
    jne .too_big
    add dx, [ds:3]
    inc dx
    cmp bx, dx
    ja .too_big
    mov al, [ds:0]
    mov [es:0], al
    mov [es:3], dx
.shrink:
    call mem_split
    pop ds
    pop es
    pop dx
    call mem_merge
    clc
    ret
.too_big:
    mov bx, dx
    mov ax, ERR_NOMEM
    pop ds
    pop es
    pop dx
    stc
    ret
.bad:
    mov ax, ERR_BADMEM
    pop ds
    pop es
    pop dx
    stc
    ret

; mem_largest : -> AX = taille du plus grand bloc libre (paragraphes)
mem_largest:
    push bx
    mov bx, 0xFFFF
    call mem_alloc              ; echoue toujours, BX = plus grand bloc
    mov ax, bx
    pop bx
    ret

; mem_free_owner : AX = PSP -> libere tous les blocs de ce programme
mem_free_owner:
    push es
    push bx
    mov bx, MEM_START
    mov es, bx
.loop:
    call mem_check
    jc .done
    cmp [es:1], ax
    jne .next
    mov word [es:1], 0
.next:
    call mem_next
    jnc .loop
.done:
    pop bx
    pop es
    call mem_merge
    ret

; mem_set_owner : AX = segment du bloc, DX = nouveau proprietaire
mem_set_owner:
    push es
    push ax
    dec ax
    mov es, ax
    mov [es:1], dx
    pop ax
    pop es
    ret
