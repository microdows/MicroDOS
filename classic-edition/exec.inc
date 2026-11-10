; ============================================================================
; exec.inc - Lancement des programmes (.COM et .EXE) et fin de programme
;
; exec_program : SI = chemin du programme (segment noyau)
;                BX = queue de commande (octet longueur + texte + CR)
;   Charge le programme, cree son PSP et son environnement, l'execute,
;   puis revient ici quand il se termine (INT 20h, INT 21h/4Ch...).
;   CF=1 et AX = code d'erreur si le programme n'a pas pu etre lance.
;
; Le .EXE est lu directement a sa place en memoire : plus de "staging area",
; donc plus de limite de 64 Ko.
; ============================================================================

ex_mode     db 0                ; 0 = charger et executer, 3 = overlay
ex_sft      dw 0
ex_tail     dw 0                ; queue de commande (offset dans le noyau)
ex_tail_seg dw 0
ex_env_src  dw 0                ; environnement a copier (0 = celui du parent)
ex_env      dw 0                ; nouvel environnement
ex_env_len  dw 0
ex_psp      dw 0
ex_block    dw 0                ; taille du bloc du programme
ex_load     dw 0                ; segment de chargement de l'image
ex_reloc_f  dw 0                ; facteur de relocation
ex_is_exe   db 0
ex_img_size dd 0
ex_hdr      times 0x1C db 0
ex_path     times 80 db 0
ex_entry    dd 0                ; CS:IP de depart
ex_stack    dd 0                ; SS:SP de depart
ex_retcode  dw 0                ; code de retour (INT 21h/4Dh)
ex_fcb1     dd 0
ex_fcb2     dd 0

; ---------------------------------------------------------------------------
exec_program:
    mov byte [ex_mode], 0
    mov [ex_tail], bx
    mov [ex_tail_seg], cs
    mov word [ex_env_src], 0
    mov dword [ex_fcb1], 0
    mov dword [ex_fcb2], 0
exec_common:
    pushad
    push es
    push ds
    push fs
    push gs

    ; chemin complet (pour l'environnement : DOS/4GW s'en sert)
    push si
    push cs
    pop es
    mov di, ex_path
    call path_canon
    pop si
    jc .nofile

    mov al, 0
    call file_open
    jc .err_ax
    mov [ex_sft], bx

    ; en-tete MZ ?
    push cs
    pop es
    mov di, ex_hdr
    mov ecx, 0x1C
    call file_read
    jc .err_close_format
    mov byte [ex_is_exe], 0
    cmp eax, 0x1C
    jb .is_com
    cmp word [ex_hdr], 'MZ'
    je .is_exe
    cmp word [ex_hdr], 'ZM'
    jne .is_com
.is_exe:
    mov byte [ex_is_exe], 1
    ; taille de l'image = pages*512 (moins la fin de la derniere page) - en-tete
    movzx eax, word [ex_hdr+4]
    shl eax, 9
    movzx ecx, word [ex_hdr+2]
    test ecx, ecx
    jz .full_page
    sub eax, 512
    add eax, ecx
.full_page:
    movzx ecx, word [ex_hdr+8]
    shl ecx, 4
    sub eax, ecx
    jbe .err_close_format
    ; ne pas depasser la taille du fichier
    mov edx, [bx+F_SIZE]
    sub edx, ecx
    cmp eax, edx
    jbe .size_ok
    mov eax, edx
.size_ok:
    mov [ex_img_size], eax
    jmp .loaded_header
.is_com:
    mov eax, [bx+F_SIZE]
    cmp eax, 0xFF00
    ja .err_close_nomem
    mov [ex_img_size], eax
.loaded_header:

    cmp byte [ex_mode], 3
    je .overlay

    ; ---- environnement ----
    call build_env
    jc .err_close_nomem

    ; ---- bloc memoire du programme ----
    call mem_largest
    movzx edx, ax               ; EDX = plus grand bloc libre
    mov eax, [ex_img_size]
    add eax, 15
    shr eax, 4                  ; paragraphes de l'image
    add eax, 0x10               ; + PSP
    cmp byte [ex_is_exe], 0
    jne .exe_mem
    ; .COM : il recoit toute la memoire libre, comme sous DOS
    add eax, 0x10               ; + un peu de pile
    cmp edx, eax
    jb .err_free_env
    mov bx, dx
    jmp .alloc
.exe_mem:
    movzx ecx, word [ex_hdr+0x0A]
    add ecx, eax                ; minimum necessaire
    cmp ecx, edx
    ja .err_free_env
    movzx ebx, word [ex_hdr+0x0C]
    add ebx, eax                ; maximum demande
    cmp ebx, edx
    jbe .alloc_ebx
    mov ebx, edx
.alloc_ebx:
    cmp ebx, ecx
    jae .alloc
    mov ebx, ecx
.alloc:
    mov [ex_block], bx
    call mem_alloc
    jc .err_free_env
    mov [ex_psp], ax
    mov dx, ax
    call mem_set_owner          ; le bloc appartient au programme
    mov ax, [ex_env]
    call mem_set_owner
    mov ax, [ex_psp]
    add ax, 0x10
    mov [ex_load], ax
    mov [ex_reloc_f], ax

.load_image:
    ; ---- lecture de l'image a sa place ----
    mov bx, [ex_sft]
    xor eax, eax
    cmp byte [ex_is_exe], 0
    je .seek
    movzx eax, word [ex_hdr+8]
    shl eax, 4
.seek:
    mov [bx+F_POS], eax
    mov word [bx+F_CUR], 0
    mov es, [ex_load]
    xor di, di
    cmp byte [ex_is_exe], 0
    jne .read_img
    cmp byte [ex_mode], 3
    je .read_img
    ; .COM : charge a PSP:0100
    mov es, [ex_psp]
    mov di, 0x100
.read_img:
    mov ecx, [ex_img_size]
    call file_read
    jc .err_load
    cmp byte [ex_is_exe], 0
    je .relocated

    ; ---- relocations ----
    mov cx, [ex_hdr+6]
    jcxz .relocated
    movzx eax, word [ex_hdr+0x18]
    mov [bx+F_POS], eax
    mov word [bx+F_CUR], 0
.reloc_chunk:
    push cx
    cmp cx, 256
    jbe .chunk_ok
    mov cx, 256
.chunk_ok:
    push cx
    push cs
    pop es
    mov di, RELOC_BUF
    movzx ecx, cx
    shl ecx, 2
    call file_read
    pop cx
    jc .err_reloc
    mov si, RELOC_BUF
    mov dx, [ex_reloc_f]
.reloc_one:
    mov di, [si]                ; offset
    mov ax, [si+2]              ; segment relatif
    add ax, [ex_load]
    mov es, ax
    add [es:di], dx
    add si, 4
    loop .reloc_one
    pop cx
    sub cx, 256
    ja .reloc_chunk
.relocated:
    mov bx, [ex_sft]
    call file_close
    push cs
    pop es

    cmp byte [ex_mode], 3
    je .overlay_done

    ; ---- PSP ----
    call build_psp

    ; ---- registres de depart ----
    cmp byte [ex_is_exe], 0
    jne .exe_regs
    mov ax, [ex_psp]
    mov word [ex_entry], 0x100
    mov [ex_entry+2], ax
    mov [ex_stack+2], ax
    mov ax, 0xFFFE
    cmp word [ex_block], 0x1000
    jae .com_sp
    mov ax, [ex_block]
    shl ax, 4
    sub ax, 2
.com_sp:
    mov [ex_stack], ax
    ; mot 0 en haut de pile : un RET final saute en PSP:0000 (INT 20h)
    mov es, [ex_psp]
    mov di, ax
    mov word [es:di], 0
    push cs
    pop es
    jmp .launch
.exe_regs:
    mov ax, [ex_hdr+0x14]
    mov [ex_entry], ax
    mov ax, [ex_hdr+0x16]
    add ax, [ex_load]
    mov [ex_entry+2], ax
    mov ax, [ex_hdr+0x10]
    mov [ex_stack], ax
    mov ax, [ex_hdr+0x0E]
    add ax, [ex_load]
    mov [ex_stack+2], ax

.launch:
    ; ---- sauvegarde du contexte du parent ----
    push word [indos]
    push word [cur_psp]
    push dword [dta_ptr]
    push word [i21_stack_top]
    mov es, [ex_psp]
    mov [es:0x2E], sp           ; SS:SP du parent dans le PSP de l'enfant
    mov [es:0x30], ss
    ; les appels INT 21h de l'enfant utiliseront la pile sous ce contexte
    mov ax, ss
    mov bx, cs
    cmp ax, bx
    jne .not_nested
    mov ax, sp
    sub ax, 32
    mov [i21_stack_top], ax
.not_nested:
    mov ax, [ex_psp]
    mov [cur_psp], ax
    mov word [dta_ptr], 0x80
    mov [dta_ptr+2], ax

    ; ---- saut vers le programme ----
    cli
    mov ss, [ex_stack+2]
    mov sp, [ex_stack]
    sti
    mov ax, [cs:ex_psp]
    mov ds, ax
    mov es, ax
    xor ax, ax
    xor bx, bx
    mov cx, 0x00FF
    mov dx, ds
    xor si, si
    xor di, di
    xor bp, bp
    jmp far [cs:ex_entry]

; ---------------------------------------------------------------------------
; Retour ici quand le programme est termine (voir exec_terminate)
; ---------------------------------------------------------------------------
..@exec_resume:
    pop word [cs:i21_stack_top]
    pop dword [cs:dta_ptr]
    pop word [cs:cur_psp]
    pop word [cs:indos]
    pop gs
    pop fs
    pop ds
    pop es
    popad
    clc
    ret

; ---- overlay (INT 21h AX=4B03) : charge l'image a un segment donne ----
.overlay:
    jmp .load_image
.overlay_done:
    pop gs
    pop fs
    pop ds
    pop es
    popad
    clc
    ret

; ---- erreurs ----
.err_reloc:
    pop cx
.err_load:
    mov bx, [ex_sft]
    call file_close
    mov ax, [ex_psp]
    call mem_free_owner
    mov ax, ERR_FORMAT
    jmp .err_ax
.err_free_env:
    mov ax, [ex_env]
    push es
    mov es, ax
    call mem_free
    pop es
.err_close_nomem:
    mov bx, [ex_sft]
    call file_close
    mov ax, ERR_NOMEM
    jmp .err_ax
.err_close_format:
    mov bx, [ex_sft]
    call file_close
    mov ax, ERR_FORMAT
    jmp .err_ax
.nofile:
    mov ax, ERR_NOFILE
.err_ax:
    mov [cs:ex_err], ax
    pop gs
    pop fs
    pop ds
    pop es
    popad
    mov ax, [cs:ex_err]
    stc
    ret

ex_err dw 0

; ---------------------------------------------------------------------------
; build_env : copie l'environnement du parent + le chemin du programme
; ---------------------------------------------------------------------------
build_env:
    push ds
    push es
    push si
    push di
    push cx
    ; source : environnement demande, sinon celui du parent
    mov ax, [ex_env_src]
    test ax, ax
    jnz .have_src
    mov es, [cur_psp]
    mov ax, [es:0x2C]
.have_src:
    mov [ex_env_src], ax
    ; longueur des chaines (jusqu'au double 0)
    mov ds, ax
    xor si, si
.scan:
    cmp word [si], 0
    je .scanned
    inc si
    cmp si, 0x7F00
    jb .scan
.scanned:
    add si, 2                   ; les deux 0
    push cs
    pop ds
    mov [ex_env_len], si
    ; + mot "1" + chemin du programme
    mov di, ex_path
    call str_end
    sub di, ex_path
    mov bx, si
    add bx, di
    add bx, 2+1+15
    shr bx, 4
    call mem_alloc
    jc .fail
    mov [ex_env], ax
    ; copie
    mov es, ax
    xor di, di
    mov ds, [cs:ex_env_src]
    xor si, si
    mov cx, [cs:ex_env_len]
    cld
    rep movsb
    push cs
    pop ds
    mov ax, 1
    stosw
    mov si, ex_path
    call copy_str
    clc
.fail:
    pop cx
    pop di
    pop si
    pop es
    pop ds
    ret

; ---------------------------------------------------------------------------
; build_psp : remplit le PSP du nouveau programme (segment [ex_psp])
; ---------------------------------------------------------------------------
build_psp:
    push es
    mov es, [ex_psp]
    ; tout a zero
    xor di, di
    mov cx, 128
    xor ax, ax
    cld
    rep stosw
    mov word [es:0x00], 0x20CD  ; INT 20h
    mov ax, [ex_psp]
    add ax, [ex_block]
    mov [es:0x02], ax           ; fin de la memoire du programme
    mov byte [es:0x05], 0xCB    ; RETF (appel CP/M, non gere)
    ; vecteurs 22h/23h/24h du parent
    push ds
    xor ax, ax
    mov ds, ax
    mov eax, [0x22*4]
    mov [es:0x0A], eax
    mov eax, [0x23*4]
    mov [es:0x0E], eax
    mov eax, [0x24*4]
    mov [es:0x12], eax
    pop ds
    mov ax, [cur_psp]
    mov [es:0x16], ax           ; PSP du parent
    ; table des handles : heritee du parent
    call copy_jft
    mov ax, [ex_env]
    mov [es:0x2C], ax
    mov word [es:0x32], 20
    mov word [es:0x34], 0x18
    mov ax, [ex_psp]
    mov [es:0x36], ax
    mov dword [es:0x38], 0xFFFFFFFF
    mov word [es:0x40], DOS_VERSION
    mov word [es:0x50], 0x21CD  ; INT 21h
    mov byte [es:0x52], 0xCB    ; RETF
    ; FCB vides
    mov di, 0x5C
    call blank_fcb
    mov di, 0x6C
    call blank_fcb
    ; queue de commande
    push ds
    mov ds, [cs:ex_tail_seg]
    mov si, [cs:ex_tail]
    mov di, 0x80
    lodsb
    cmp al, 126
    jbe .len_ok
    mov al, 126
.len_ok:
    stosb
    movzx cx, al
    rep movsb
    mov byte [es:di], 13
    pop ds
    pop es
    ret

blank_fcb:
    mov byte [es:di], 0
    inc di
    mov cx, 11
    mov al, ' '
    rep stosb
    mov cx, 4
    xor al, al
    rep stosb
    ret

; copy_jft : ES = PSP de l'enfant -> copie la table de handles du parent
copy_jft:
    push ds
    push si
    push di
    push cx
    push bx
    mov di, 0x18
    mov ax, [cur_psp]
    test ax, ax
    jz .default
    mov ds, ax
    mov si, 0x18
    mov cx, 20
.copy:
    lodsb
    stosb
    cmp al, 0xFF
    je .next
    ; une reference de plus sur ce fichier ouvert
    push ds
    push cs
    pop ds
    movzx bx, al
    shl bx, 5
    add bx, sft_table
    cmp byte [bx+F_REFS], 0xFF
    je .no_inc
    inc byte [bx+F_REFS]
.no_inc:
    pop ds
.next:
    loop .copy
    jmp .done
.default:
    mov cx, 5
    xor al, al
    rep stosb
    mov cx, 15
    mov al, 0xFF
    rep stosb
.done:
    pop bx
    pop cx
    pop di
    pop si
    pop ds
    ret

; ---------------------------------------------------------------------------
; exec_terminate : AL = code de retour, AH = type (0 normal, 3 resident)
;                  DX = paragraphes a garder (resident)
; Ne revient pas : reprend l'execution du parent dans exec_resume.
; ---------------------------------------------------------------------------
exec_terminate:
    cli
    push cs
    pop ds
    mov [ex_retcode], ax
    mov [ex_keep], dx
    ; pile du noyau (sous le contexte sauvegarde du parent)
    push cs
    pop ss
    mov sp, [i21_stack_top]
    sti
    cld

    mov ax, [cur_psp]
    cmp ax, [root_psp]
    je .no_program
    mov es, ax

    cmp byte [ex_retcode+1], 3
    je .resident
    ; fermer les fichiers du programme
    mov di, 0x18
    mov cx, 20
.close:
    mov al, [es:di]
    cmp al, 0xFF
    je .closed
    movzx bx, al
    shl bx, 5
    add bx, sft_table
    call file_close
    mov byte [es:di], 0xFF
.closed:
    inc di
    loop .close
    call fat_flush
    ; liberer sa memoire
    mov ax, es
    call mem_free_owner
    jmp .restore
.resident:
    mov bx, [ex_keep]
    cmp bx, 6
    jae .keep_ok
    mov bx, 6
.keep_ok:
    call mem_resize
.restore:
    ; vecteurs 22h/23h/24h
    push ds
    xor ax, ax
    mov ds, ax
    mov eax, [es:0x0A]
    mov [0x22*4], eax
    mov eax, [es:0x0E]
    mov [0x23*4], eax
    mov eax, [es:0x12]
    mov [0x24*4], eax
    pop ds
    ; retour sur la pile du parent
    cli
    mov ss, [es:0x30]
    mov sp, [es:0x2E]
    sti
    jmp ..@exec_resume
.no_program:
    ; personne a qui revenir : retour au shell
    cli
    xor ax, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    jmp main_loop

ex_keep dw 0

; INT 20h : fin de programme
int20_handler:
    xor ax, ax
    jmp exec_terminate

; INT 27h : rester resident (DX = octets a garder)
int27_handler:
    add dx, 15
    shr dx, 4
    mov ax, 0x0300
    jmp exec_terminate

; ---------------------------------------------------------------------------
; Environnement de depart (proprietaire : le PSP systeme)
; ---------------------------------------------------------------------------
env_init:
    push es
    ; environnement maitre
    mov bx, (env_master_end - env_master + 15) / 16 + 1
    call mem_alloc
    jc .done
    mov [ex_env], ax
    mov es, ax
    xor di, di
    mov si, env_master
    mov cx, env_master_end - env_master
    rep movsb
    ; lettre du lecteur de demarrage dans COMSPEC et PATH
    mov al, [boot_drive]
    add al, 'A'
    mov [es:env_comspec_drive - env_master], al
    mov [es:env_path_drive1 - env_master], al
    mov [es:env_path_drive2 - env_master], al
    ; PSP systeme
    mov bx, 0x10
    call mem_alloc
    jc .done
    mov [root_psp], ax
    mov [cur_psp], ax
    mov [ex_psp], ax
    mov word [ex_block], 0x10
    mov word [ex_tail], empty_tail
    mov [ex_tail_seg], cs
    mov dx, ax
    call mem_set_owner
    mov ax, [ex_env]
    call mem_set_owner
    mov word [cur_psp], 0       ; pas de parent
    call build_psp
    mov ax, [root_psp]
    mov [cur_psp], ax
    mov es, ax
    mov [es:0x16], ax           ; son propre parent
    mov word [dta_ptr], 0x80
    mov [dta_ptr+2], ax
.done:
    pop es
    ret

empty_tail  db 0, 13

env_master:
    db 'COMSPEC='
env_comspec_drive:
    db 'A:\COMMAND.COM', 0
    db 'PATH='
env_path_drive1:
    db 'A:\;'
env_path_drive2:
    db 'A:\JEUX', 0
    db 'PROMPT=$P$G', 0
    db 0
env_master_end:
