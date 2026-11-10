; ============================================================================
; dos.inc - API DOS (INT 21h)
;
; A l'entree, le gestionnaire passe sur la pile du noyau et sauvegarde tous
; les registres de l'appelant. Les fonctions lisent et modifient ces
; registres sauvegardes via BP (R_AX, R_BX, ...), puis sortent par :
;   dos_ok   : CF = 0
;   dos_err  : CF = 1, AX = code d'erreur
;   dos_exit : sans toucher a CF
; ============================================================================

R_ES    equ 0
R_DI    equ 2
R_SI    equ 6
R_BP    equ 10
R_BX    equ 18
R_DX    equ 22
R_CX    equ 26
R_AX    equ 30
R_DS    equ 34
R_USP   equ 36
R_USS   equ 38
R_AL    equ R_AX
R_AH    equ R_AX+1
R_BL    equ R_BX
R_BH    equ R_BX+1
R_CL    equ R_CX
R_CH    equ R_CX+1
R_DL    equ R_DX
R_DH    equ R_DX+1

DOS_VERSION     equ 0x1606      ; AL=6, AH=22 -> MS-DOS 6.22

i21_stack_top   dw 0xFFF0
i21_t_ss        dw 0
i21_t_sp        dw 0
i21_t_ds        dw 0
dta_ptr         dd 0
last_error      dw 0
indos           db 0, 0
verify_flag     db 0
break_flag      db 0
dos_path        times 130 db 0
dos_path2       times 130 db 0

; ---------------------------------------------------------------------------
int21_handler:
    cli
    mov [cs:i21_t_ss], ss
    mov [cs:i21_t_sp], sp
    mov [cs:i21_t_ds], ds
    push cs
    pop ds
    push cs
    pop ss
    mov sp, [i21_stack_top]
    push word [i21_t_ss]
    push word [i21_t_sp]
    push word [i21_t_ds]
    pushad
    push es
    mov bp, sp
    sti
    cld
    inc byte [indos]
    movzx bx, ah
    cmp bx, DOS_MAX
    ja dos_bad
    shl bx, 1
    jmp word [dos_table+bx]

dos_ok:
    les bx, [bp+R_USP]
    and byte [es:bx+4], 0xFE
    jmp dos_exit
dos_err:
    mov [bp+R_AX], ax
    mov [last_error], ax
    les bx, [bp+R_USP]
    or byte [es:bx+4], 1
dos_exit:
    cli
    dec byte [indos]
    mov sp, bp
    pop es
    popad
    pop word [i21_t_ds]
    pop word [i21_t_sp]
    pop word [i21_t_ss]
    mov ss, [i21_t_ss]
    mov sp, [i21_t_sp]
    mov ds, [i21_t_ds]
    iret

dos_bad:
    mov ax, ERR_FUNC
    jmp dos_err

; met ZF dans les drapeaux de l'appelant selon AL (AL=0 -> ZF=1)
set_user_zf:
    les bx, [bp+R_USP]
    test al, al
    jnz .clear
    or byte [es:bx+4], 0x40
    ret
.clear:
    and byte [es:bx+4], 0xBF
    ret

; ---------------------------------------------------------------------------
; Table des fonctions (AH)
; ---------------------------------------------------------------------------
DOS_MAX equ 0x71
dos_table:
    dw f00, f01, f02, f_al0, f_al0, f02, f06, f07          ; 00-07
    dw f08, f09, f0A, f0B, f0C, f0D, f0E, f_fcb            ; 08-0F
    dw f_fcb, f_fcb, f_fcb, f_fcb, f_fcb, f_fcb, f_fcb, f_fcb ; 10-17
    dw f_al0, f19, f1A, f_fcb, f_fcb, f_al0, f_al0, f_fcb  ; 18-1F
    dw f_al0, f_fcb, f_fcb, f_fcb, f_fcb, f25, f_al0, f_fcb ; 20-27
    dw f_fcb, f29, f2A, f2B, f2C, f2D, f2E, f2F            ; 28-2F
    dw f30, f31, f_fcb, f33, f34, f35, f36, f37            ; 30-37
    dw f38, f39, f3A, f3B, f3C, f3D, f3E, f3F              ; 38-3F
    dw f40, f41, f42, f43, f44, f45, f46, f47              ; 40-47
    dw f48, f49, f4A, f4B, f4C, f4D, f4E, f4F              ; 48-4F
    dw f50, f51, f52, dos_bad, f54, dos_bad, f56, f57      ; 50-57
    dw f58, f59, f5A, f5B, dos_bad, f5D, dos_bad, dos_bad  ; 58-5F
    dw f60, dos_bad, f51, dos_bad, dos_bad, dos_bad, f66, f_ok ; 60-67
    dw f68, dos_bad, f68, dos_bad, f6C, dos_bad, dos_bad, dos_bad ; 68-6F
    dw dos_bad, f71                                        ; 70-71

f_al0:
    mov byte [bp+R_AL], 0
    jmp dos_exit

f_fcb:
    mov byte [bp+R_AL], 0xFF    ; fonctions FCB (anciennes) : non gerees
    jmp dos_exit

f_ok:
    jmp dos_ok

f71:
    mov ax, 0x7100              ; noms longs : non geres
    jmp dos_err

; ===========================================================================
; Console
; ===========================================================================
con_getc:
    mov ah, 0x00
    int 0x16
    ret

; con_putc : AL = caractere (les tabulations sont developpees)
con_putc:
    push ax
    push bx
    push cx
    push dx
    cmp al, 9
    jne .plain
    mov ah, 0x03
    xor bh, bh
    int 0x10                    ; DL = colonne
.tab:
    mov ax, 0x0E20
    mov bx, 0x0007
    int 0x10
    inc dl
    test dl, 7
    jnz .tab
    jmp .done
.plain:
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10
.done:
    pop dx
    pop cx
    pop bx
    pop ax
    ret

f00:
    xor ax, ax
    jmp dos_terminate

f01:
    call con_getc
    call con_putc
    mov [bp+R_AL], al
    jmp dos_exit

f02:
    mov al, [bp+R_DL]
    call con_putc
    mov [bp+R_AL], al
    jmp dos_exit

f06:
    mov al, [bp+R_DL]
    cmp al, 0xFF
    je .input
    call con_putc
    jmp dos_exit
.input:
    mov ah, 0x01
    int 0x16
    jz .none
    call con_getc
    mov [bp+R_AL], al
    mov al, 1
    call set_user_zf
    jmp dos_exit
.none:
    mov byte [bp+R_AL], 0
    xor al, al
    call set_user_zf
    jmp dos_exit

f07:
f08:
    call con_getc
    mov [bp+R_AL], al
    jmp dos_exit

f09:
    mov fs, [bp+R_DS]
    mov si, [bp+R_DX]
.loop:
    mov al, [fs:si]
    cmp al, '$'
    je .done
    call con_putc
    inc si
    jmp .loop
.done:
    mov byte [bp+R_AL], '$'
    jmp dos_exit

; 0Ah : saisie d'une ligne dans DS:DX ([0]=max, [1]=lu, [2..]=texte+CR)
f0A:
    mov es, [bp+R_DS]
    mov di, [bp+R_DX]
    movzx cx, byte [es:di]
    jcxz .out
    dec cx                      ; place pour le CR
    lea bx, [di+2]
    xor dx, dx
.key:
    call con_getc
    cmp al, 13
    je .enter
    cmp al, 8
    je .back
    cmp al, 32
    jb .key
    cmp dx, cx
    jae .key
    mov [es:bx], al
    inc bx
    inc dx
    call con_putc
    jmp .key
.back:
    test dx, dx
    jz .key
    dec bx
    dec dx
    mov al, 8
    call con_putc
    mov al, ' '
    call con_putc
    mov al, 8
    call con_putc
    jmp .key
.enter:
    mov byte [es:bx], 13
    mov [es:di+1], dl
    mov al, 13
    call con_putc
.out:
    jmp dos_exit

f0B:
    mov ah, 0x01
    int 0x16
    mov al, 0
    jz .no
    mov al, 0xFF
.no:
    mov [bp+R_AL], al
    jmp dos_exit

; 0Ch : vider le clavier puis appeler la fonction AL
f0C:
.flush:
    mov ah, 0x01
    int 0x16
    jz .flushed
    call con_getc
    jmp .flush
.flushed:
    mov al, [bp+R_AL]
    cmp al, 0x01
    je .call
    cmp al, 0x06
    je .call
    cmp al, 0x07
    je .call
    cmp al, 0x08
    je .call
    cmp al, 0x0A
    je .call
    jmp dos_exit
.call:
    mov [bp+R_AH], al
    movzx bx, al
    shl bx, 1
    jmp word [dos_table+bx]

; ===========================================================================
; Lecteurs, DTA, vecteurs, date et heure
; ===========================================================================
f0D:
    call fat_flush
    jmp dos_exit

f0E:
    mov al, [bp+R_DL]
    cmp al, MAX_DRIVES
    jae .count
    call drive_select
    jc .count
    mov [def_drive], al
.count:
    mov byte [bp+R_AL], MAX_DRIVES
    jmp dos_exit

f19:
    mov al, [def_drive]
    mov [bp+R_AL], al
    jmp dos_exit

f1A:
    mov ax, [bp+R_DX]
    mov [dta_ptr], ax
    mov ax, [bp+R_DS]
    mov [dta_ptr+2], ax
    jmp dos_exit

f2F:
    mov ax, [dta_ptr]
    mov [bp+R_BX], ax
    mov ax, [dta_ptr+2]
    mov [bp+R_ES], ax
    jmp dos_exit

f25:
    movzx bx, byte [bp+R_AL]
    shl bx, 2
    xor ax, ax
    mov es, ax
    mov ax, [bp+R_DX]
    mov dx, [bp+R_DS]
    cli
    mov [es:bx], ax
    mov [es:bx+2], dx
    sti
    jmp dos_exit

f35:
    movzx bx, byte [bp+R_AL]
    shl bx, 2
    xor ax, ax
    mov es, ax
    mov ax, [es:bx]
    mov [bp+R_BX], ax
    mov ax, [es:bx+2]
    mov [bp+R_ES], ax
    jmp dos_exit

f29:
    ; analyse de nom de fichier pour FCB : on rend un FCB vide
    mov es, [bp+R_ES]
    mov di, [bp+R_DI]
    call blank_fcb
    mov byte [bp+R_AL], 0
    jmp dos_exit

f2A:
    mov ah, 0x04
    int 0x1A
    mov al, ch
    call bcd_to_bin
    mov bl, 100
    mul bl
    mov bx, ax
    mov al, cl
    call bcd_to_bin
    xor ah, ah
    add ax, bx
    mov [bp+R_CX], ax
    mov al, dh
    call bcd_to_bin
    mov [bp+R_DH], al
    mov al, dl
    call bcd_to_bin
    mov [bp+R_DL], al
    ; jour de la semaine (Zeller simplifie) : on renvoie 0
    mov byte [bp+R_AL], 0
    jmp dos_exit

f2B:
f2D:
    mov byte [bp+R_AL], 0
    jmp dos_exit

f2C:
    mov ah, 0x02
    int 0x1A
    mov al, ch
    call bcd_to_bin
    mov [bp+R_CH], al
    mov al, cl
    call bcd_to_bin
    mov [bp+R_CL], al
    mov al, dh
    call bcd_to_bin
    mov [bp+R_DH], al
    mov byte [bp+R_DL], 0
    jmp dos_exit

f2E:
    mov al, [bp+R_AL]
    mov [verify_flag], al
    jmp dos_exit

f54:
    mov al, [verify_flag]
    mov [bp+R_AL], al
    jmp dos_exit

; 30h : version de DOS -> 6.22 (certains utilitaires MS-DOS l'exigent)
f30:
    mov word [bp+R_AX], DOS_VERSION
    mov word [bp+R_BX], 0xFF00
    mov word [bp+R_CX], 0
    jmp dos_exit

f33:
    mov al, [bp+R_AL]
    cmp al, 0
    je .get
    cmp al, 1
    je .set
    cmp al, 5
    je .boot
    cmp al, 6
    je .true
    mov byte [bp+R_AL], 0xFF
    jmp dos_exit
.get:
    mov al, [break_flag]
    mov [bp+R_DL], al
    jmp dos_exit
.set:
    mov al, [bp+R_DL]
    mov [break_flag], al
    jmp dos_exit
.boot:
    mov al, [boot_drive]
    inc al
    mov [bp+R_DL], al
    jmp dos_exit
.true:
    mov word [bp+R_BX], DOS_VERSION
    mov word [bp+R_DX], 0
    jmp dos_exit

f34:
    mov word [bp+R_BX], indos
    mov [bp+R_ES], cs
    jmp dos_exit

f37:
    cmp byte [bp+R_AL], 0
    jne .set
    mov byte [bp+R_DL], '/'
.set:
    mov byte [bp+R_AL], 0
    jmp dos_exit

; 38h : informations sur le pays (format USA)
f38:
    cmp word [bp+R_DX], 0xFFFF
    je .done
    mov es, [bp+R_DS]
    mov di, [bp+R_DX]
    mov si, country_info
    mov cx, 34
    rep movsb
    mov ax, cs
    mov [es:di-34+20], ax       ; segment de la routine de casse
.done:
    mov word [bp+R_AX], 1
    mov word [bp+R_BX], 1
    jmp dos_ok

country_info:
    dw 0                        ; date : mm/jj/aa
    db '$', 0, 0, 0, 0
    db ',', 0
    db '.', 0
    db '/', 0
    db ':', 0
    db 0, 2, 0
    dw case_map, 0
    db ',', 0
    times 10 db 0

case_map:
    retf

; 36h : espace libre (DL = 0 lecteur courant, 1 = A:)
f36:
    mov al, [bp+R_DL]
    test al, al
    jnz .drive
    mov al, [def_drive]
    inc al
.drive:
    dec al
    call drive_select
    jc .bad
    call free_clusters
    mov [bp+R_BX], ax
    movzx ax, byte [dv_spc]
    mov [bp+R_AX], ax
    mov word [bp+R_CX], 512
    mov ax, [dv_max_clust]
    sub ax, 2
    mov [bp+R_DX], ax
    mov al, [def_drive]
    call drive_select
    jmp dos_exit
.bad:
    mov word [bp+R_AX], 0xFFFF
    jmp dos_exit

; ===========================================================================
; Memoire et programmes
; ===========================================================================
f48:
    mov bx, [bp+R_BX]
    call mem_alloc
    jc .err
    mov [bp+R_AX], ax
    jmp dos_ok
.err:
    mov [bp+R_BX], bx
    jmp dos_err

f49:
    mov es, [bp+R_ES]
    call mem_free
    jc .err
    jmp dos_ok
.err:
    jmp dos_err

f4A:
    mov es, [bp+R_ES]
    mov bx, [bp+R_BX]
    call mem_resize
    jc .err
    ; mettre a jour la fin de memoire dans le PSP si c'est le programme
    mov ax, es
    cmp ax, [cur_psp]
    jne .ok
    add ax, [bp+R_BX]
    mov [es:2], ax
.ok:
    jmp dos_ok
.err:
    mov [bp+R_BX], bx
    jmp dos_err

f58:
    mov al, [bp+R_AL]
    cmp al, 0
    je .get
    cmp al, 1
    je .set
    cmp al, 2
    je .umb
    cmp al, 3
    je .ok
    jmp dos_bad
.get:
    mov ax, [mem_strategy]
    mov [bp+R_AX], ax
    jmp dos_ok
.set:
    mov ax, [bp+R_BX]
    mov [mem_strategy], ax
.ok:
    jmp dos_ok
.umb:
    mov byte [bp+R_AL], 0
    jmp dos_ok

f50:
    mov ax, [bp+R_BX]
    mov [cur_psp], ax
    jmp dos_exit

f51:
    mov ax, [cur_psp]
    mov [bp+R_BX], ax
    jmp dos_exit

; 52h : "liste des listes" (on fournit surtout le premier MCB en ES:[BX-2])
f52:
    mov word [lol_first_mcb], MEM_START
    mov word [bp+R_BX], lol_area
    mov [bp+R_ES], cs
    jmp dos_exit

lol_first_mcb dw MEM_START
lol_area      times 64 db 0

f4C:
    mov al, [bp+R_AL]
    xor ah, ah
    jmp dos_terminate

f31:
    mov al, [bp+R_AL]
    mov ah, 3
    mov dx, [bp+R_DX]
    jmp dos_terminate

dos_terminate:
    jmp exec_terminate

f4D:
    mov ax, [ex_retcode]
    mov [bp+R_AX], ax
    mov word [ex_retcode], 0
    jmp dos_ok

; 4Bh : AL=0 charger et executer, AL=3 charger un overlay
f4B:
    call get_user_path
    mov es, [bp+R_ES]
    mov bx, [bp+R_BX]
    mov al, [bp+R_AL]
    cmp al, 0
    je .exec
    cmp al, 3
    je .overlay
    jmp dos_bad
.exec:
    mov ax, [es:bx]
    mov [ex_env_src], ax
    mov ax, [es:bx+2]
    mov [ex_tail], ax
    mov ax, [es:bx+4]
    mov [ex_tail_seg], ax
    mov byte [ex_mode], 0
    jmp .go
.overlay:
    mov ax, [es:bx]
    mov [ex_load], ax
    mov ax, [es:bx+2]
    mov [ex_reloc_f], ax
    mov byte [ex_mode], 3
.go:
    push cs
    pop es
    mov si, dos_path
    call exec_common
    jc .err
    jmp dos_ok
.err:
    jmp dos_err

f59:
    mov ax, [last_error]
    mov [bp+R_AX], ax
    mov word [bp+R_BX], 0x0104
    mov byte [bp+R_CH], 1
    jmp dos_exit

; 5Dh : AL=06 adresse de la "zone de donnees echangeable" (utilisee par
; certains extendeurs DOS), AL=0A fixer l'erreur etendue
f5D:
    mov al, [bp+R_AL]
    cmp al, 0x06
    je .sda
    cmp al, 0x0A
    je .ok
    jmp dos_bad
.sda:
    mov [bp+R_DS], cs
    mov word [bp+R_SI], sda_area
    mov word [bp+R_CX], SDA_SIZE
    mov word [bp+R_DX], 0x20
    mov byte [sda_area], 0      ; drapeau "erreur critique"
    mov al, [indos]
    mov [sda_area+1], al        ; drapeau InDOS
.ok:
    jmp dos_ok

SDA_SIZE equ 0x80
sda_area times SDA_SIZE db 0

f66:
    mov word [bp+R_BX], 437
    mov word [bp+R_DX], 437
    jmp dos_ok

; ===========================================================================
; Fichiers et dossiers
; ===========================================================================

; get_user_path : DS:DX de l'appelant -> dos_path
get_user_path:
    mov si, [bp+R_DX]
get_user_path_si:
    mov fs, [bp+R_DS]
    mov di, dos_path
copy_user_str:
    push cx
    mov cx, 127
.loop:
    mov al, [fs:si]
    mov [di], al
    test al, al
    jz .done
    inc si
    inc di
    loop .loop
    mov byte [di], 0
.done:
    pop cx
    ret

; jft_get : AX = handle -> ES:DI = case de la table des handles du PSP
jft_get:
    push cx
    mov es, [cur_psp]
    mov cx, [es:0x32]
    cmp ax, cx
    jae .bad
    les di, [es:0x34]
    add di, ax
    pop cx
    clc
    ret
.bad:
    pop cx
    stc
    ret

; get_sft : AX = handle -> BX = entree SFT. CF si handle invalide.
get_sft:
    push es
    push di
    call jft_get
    jc .bad
    movzx bx, byte [es:di]
    cmp bl, SFT_COUNT
    jae .bad
    shl bx, 5
    add bx, sft_table
    cmp byte [bx+F_USED], 0
    je .bad
    pop di
    pop es
    clc
    ret
.bad:
    pop di
    pop es
    mov ax, ERR_HANDLE
    stc
    ret

; new_handle : BX = entree SFT -> AX = nouveau handle. CF si table pleine.
new_handle:
    push es
    push di
    push cx
    push dx
    mov dx, bx
    sub dx, sft_table
    shr dx, 5                   ; indice dans la SFT
    mov es, [cur_psp]
    mov cx, [es:0x32]
    les di, [es:0x34]
    xor ax, ax
.find:
    cmp byte [es:di], 0xFF
    je .found
    inc di
    inc ax
    loop .find
    mov ax, ERR_HANDLES
    pop dx
    pop cx
    pop di
    pop es
    stc
    ret
.found:
    mov [es:di], dl
    pop dx
    pop cx
    pop di
    pop es
    clc
    ret

; 3Ch : creer un fichier
f3C:
    call get_user_path
    mov si, dos_path
    mov cl, [bp+R_CL]
    call file_create
    jc open_done.err
open_done:
    call new_handle
    jc .full
    mov [bp+R_AX], ax
    jmp dos_ok
.full:
    push ax
    call file_close
    pop ax
.err:
    jmp dos_err

; 5Bh : creer un fichier qui n'existe pas encore
f5B:
    call get_user_path
    mov si, dos_path
    call path_lookup
    jnc .exists
    mov si, dos_path
    mov cl, [bp+R_CL]
    call file_create
    jc open_done.err
    jmp open_done
.exists:
    mov ax, 80                  ; le fichier existe
    jmp dos_err

; 5Ah : creer un fichier temporaire (DS:DX = dossier termine par '\')
f5A:
    call get_user_path
    mov di, dos_path
    call str_end
    push di
    mov ah, 0
    int 0x1A                    ; compteur d'horloge -> nom unique
    mov ax, dx
    mov cx, 4
    mov byte [di], 'M'
    mov byte [di+1], 'D'
    add di, 2
.hex:
    rol ax, 4
    push ax
    and al, 15
    add al, '0'
    cmp al, '9'
    jbe .digit
    add al, 7
.digit:
    mov [di], al
    inc di
    pop ax
    loop .hex
    mov dword [di], '.TMP'
    mov byte [di+4], 0
    pop di
    mov si, dos_path
    mov cl, [bp+R_CL]
    call file_create
    jc open_done.err
    ; recopier le nom complet chez l'appelant
    push bx
    mov es, [bp+R_DS]
    mov di, [bp+R_DX]
    mov si, dos_path
    call copy_str
    pop bx
    jmp open_done

; 3Dh : ouvrir un fichier
f3D:
    call get_user_path
    mov si, dos_path
    mov al, [bp+R_AL]
    call file_open
    jc open_done.err
    jmp open_done

; 6Ch : ouverture etendue (BL=mode, CX=attributs, DL=action, DS:SI=nom)
f6C:
    mov si, [bp+R_SI]
    call get_user_path_si
    mov si, dos_path
    call path_lookup
    jc .absent
    mov al, [bp+R_DL]
    and al, 0x0F
    cmp al, 1
    je .open
    cmp al, 2
    je .replace
    mov ax, 80
    jmp dos_err
.open:
    mov si, dos_path
    mov al, [bp+R_BL]
    call file_open
    jc open_done.err
    mov word [bp+R_CX], 1
    jmp open_done
.replace:
    mov si, dos_path
    mov cl, [bp+R_CL]
    call file_create
    jc open_done.err
    mov word [bp+R_CX], 3
    jmp open_done
.absent:
    test byte [bp+R_DL], 0xF0
    jz .not_found
    mov si, dos_path
    mov cl, [bp+R_CL]
    call file_create
    jc open_done.err
    mov word [bp+R_CX], 2
    jmp open_done
.not_found:
    mov ax, ERR_NOFILE
    jmp dos_err

; 3Eh : fermer
f3E:
    mov ax, [bp+R_BX]
    call get_sft
    jc .err
    call file_close
    mov ax, [bp+R_BX]
    call jft_get
    mov byte [es:di], 0xFF
    jmp dos_ok
.err:
    jmp dos_err

; 3Fh : lire
f3F:
    mov ax, [bp+R_BX]
    call get_sft
    jc .err
    cmp byte [bx+F_USED], 2
    je .console
    mov es, [bp+R_DS]
    mov di, [bp+R_DX]
    movzx ecx, word [bp+R_CX]
    call file_read
    jc .ioerr
    mov [bp+R_AX], ax
    jmp dos_ok
.console:
    mov es, [bp+R_DS]
    mov di, [bp+R_DX]
    mov cx, [bp+R_CX]
    call con_read
    mov [bp+R_AX], ax
    jmp dos_ok
.ioerr:
    mov ax, ERR_ACCESS
.err:
    jmp dos_err

; 40h : ecrire (CX=0 : tronquer)
f40:
    mov ax, [bp+R_BX]
    call get_sft
    jc .err
    cmp byte [bx+F_USED], 2
    je .console
    cmp byte [bx+F_MODE], 0
    je .denied
    mov es, [bp+R_DS]
    mov di, [bp+R_DX]
    movzx ecx, word [bp+R_CX]
    call file_write
    jc .ioerr
    mov [bp+R_AX], ax
    jmp dos_ok
.console:
    mov fs, [bp+R_DS]
    mov si, [bp+R_DX]
    mov cx, [bp+R_CX]
    jcxz .con_done
.con_loop:
    mov al, [fs:si]
    call con_putc
    inc si
    loop .con_loop
.con_done:
    mov ax, [bp+R_CX]
    mov [bp+R_AX], ax
    jmp dos_ok
.denied:
    mov ax, ERR_ACCESS
    jmp dos_err
.ioerr:
    mov ax, ERR_ACCESS
.err:
    jmp dos_err

; 41h : supprimer
f41:
    call get_user_path
    mov si, dos_path
    call file_delete
    jc .err
    jmp dos_ok
.err:
    jmp dos_err

; 42h : deplacer la position (AL = 0 debut, 1 courant, 2 fin)
f42:
    mov ax, [bp+R_BX]
    call get_sft
    jc .err
    cmp byte [bx+F_USED], 2
    je .console
    mov ax, [bp+R_CX]
    shl eax, 16
    mov ax, [bp+R_DX]
    mov cl, [bp+R_AL]
    cmp cl, 0
    je .set
    cmp cl, 1
    je .cur
    cmp cl, 2
    je .end
    mov ax, ERR_FUNC
    jmp dos_err
.cur:
    add eax, [bx+F_POS]
    jmp .set
.end:
    add eax, [bx+F_SIZE]
.set:
    mov [bx+F_POS], eax
    mov [bp+R_AX], ax
    shr eax, 16
    mov [bp+R_DX], ax
    jmp dos_ok
.console:
    mov word [bp+R_AX], 0
    mov word [bp+R_DX], 0
    jmp dos_ok
.err:
    jmp dos_err

; 43h : lire (AL=0) ou changer (AL=1) les attributs
f43:
    call get_user_path
    mov si, dos_path
    call path_lookup
    jc .nofile
    cmp byte [bp+R_AL], 0
    jne .set
    movzx ax, byte [si+11]
    mov [bp+R_CX], ax
    jmp dos_ok
.set:
    mov al, [si+11]
    and al, ATTR_DIR | ATTR_VOLUME
    mov ah, [bp+R_CL]
    and ah, ATTR_RO | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_ARCH
    or al, ah
    mov [si+11], al
    call dir_store
    jc .denied
    jmp dos_ok
.nofile:
    mov ax, ERR_NOFILE
    jmp dos_err
.denied:
    mov ax, ERR_ACCESS
    jmp dos_err

; 44h : IOCTL
f44:
    mov al, [bp+R_AL]
    cmp al, 0x00
    je .info
    cmp al, 0x01
    je .ok
    cmp al, 0x06
    je .ready
    cmp al, 0x07
    je .ready
    cmp al, 0x08
    je .removable
    cmp al, 0x09
    je .local
    cmp al, 0x0A
    je .local
    cmp al, 0x0E
    je .map
    jmp dos_bad
.info:
    mov ax, [bp+R_BX]
    call get_sft
    jc .err
    cmp byte [bx+F_USED], 2
    je .con
    movzx ax, byte [bx+F_DRIVE]
    mov [bp+R_DX], ax
    mov [bp+R_AX], ax
    jmp dos_ok
.con:
    mov word [bp+R_DX], 0x80D3
    mov word [bp+R_AX], 0x80D3
    jmp dos_ok
.ready:
    mov byte [bp+R_AL], 0xFF
    jmp dos_ok
.removable:
    mov al, [bp+R_BL]
    test al, al
    jnz .rdrive
    mov al, [def_drive]
    inc al
.rdrive:
    xor ah, ah
    cmp al, 3
    jb .floppy
    mov ax, 1                   ; disque dur : fixe
    jmp .rset
.floppy:
    xor ax, ax                  ; disquette : amovible
.rset:
    mov [bp+R_AX], ax
    jmp dos_ok
.local:
    mov word [bp+R_DX], 0
.ok:
    jmp dos_ok
.map:
    mov byte [bp+R_AL], 0
    jmp dos_ok
.err:
    jmp dos_err

; 45h : dupliquer un handle
f45:
    mov ax, [bp+R_BX]
    call get_sft
    jc .err
    call new_handle
    jc .err
    cmp byte [bx+F_REFS], 0xFF
    je .no_inc
    inc byte [bx+F_REFS]
.no_inc:
    mov [bp+R_AX], ax
    jmp dos_ok
.err:
    jmp dos_err

; 46h : forcer un handle (CX devient une copie de BX)
f46:
    mov ax, [bp+R_BX]
    call get_sft
    jc .err
    push bx
    mov ax, [bp+R_CX]
    call get_sft
    jc .not_open
    call file_close
.not_open:
    pop bx
    mov ax, [bp+R_CX]
    call jft_get
    jc .bad
    mov dx, bx
    sub dx, sft_table
    shr dx, 5
    mov [es:di], dl
    cmp byte [bx+F_REFS], 0xFF
    je .ok
    inc byte [bx+F_REFS]
.ok:
    jmp dos_ok
.bad:
    mov ax, ERR_HANDLE
.err:
    jmp dos_err

; 47h : dossier courant du lecteur DL (0 = courant) dans DS:SI
f47:
    mov al, [bp+R_DL]
    test al, al
    jnz .drive
    mov al, [def_drive]
    inc al
.drive:
    dec al
    call drive_select
    jc .bad
    mov es, [bp+R_DS]
    mov di, [bp+R_SI]
    mov si, dv_cwd_path
    cmp byte [si], '\'
    jne .copy
    inc si
.copy:
    call copy_str
    mov al, [def_drive]
    call drive_select
    mov word [bp+R_AX], 0x0100
    jmp dos_ok
.bad:
    mov ax, ERR_DRIVE
    jmp dos_err

; 39h / 3Ah / 3Bh : creer / supprimer / changer de dossier
f39:
    call get_user_path
    mov si, dos_path
    call dir_make
    jc .err
    jmp dos_ok
.err:
    jmp dos_err

f3A:
    call get_user_path
    mov si, dos_path
    call dir_remove
    jc .err
    jmp dos_ok
.err:
    jmp dos_err

f3B:
    call get_user_path
    mov si, dos_path
    call dir_change
    jc .err
    mov al, [def_drive]
    call drive_select
    jmp dos_ok
.err:
    jmp dos_err

; 56h : renommer (DS:DX ancien nom, ES:DI nouveau nom)
f56:
    call get_user_path
    mov fs, [bp+R_ES]
    mov si, [bp+R_DI]
    mov di, dos_path2
    call copy_user_str
    mov si, dos_path
    mov di, dos_path2
    call file_rename
    jc .err
    jmp dos_ok
.err:
    jmp dos_err

; 57h : date et heure d'un fichier ouvert (AL=0 lire, AL=1 changer)
f57:
    mov ax, [bp+R_BX]
    call get_sft
    jc .err
    cmp byte [bx+F_USED], 1
    jne .con
    cmp byte [bp+R_AL], 0
    jne .set
    mov ax, [bx+F_TIME]
    mov [bp+R_CX], ax
    mov ax, [bx+F_DATE]
    mov [bp+R_DX], ax
    jmp dos_ok
.set:
    call file_update
    mov ax, [bp+R_CX]
    mov [bx+F_TIME], ax
    mov ax, [bp+R_DX]
    mov [bx+F_DATE], ax
    mov al, [bx+F_DRIVE]
    call drive_select
    mov eax, [bx+F_DLBA]
    mov [di_lba], eax
    call dir_load
    jc .denied
    add si, [bx+F_DOFF]
    mov ax, [bp+R_CX]
    mov [si+22], ax
    mov ax, [bp+R_DX]
    mov [si+24], ax
    call dir_store
    jmp dos_ok
.con:
    mov word [bp+R_CX], 0
    mov word [bp+R_DX], 0
    jmp dos_ok
.denied:
    mov ax, ERR_ACCESS
.err:
    jmp dos_err

; 68h / 6Ah : vider les tampons d'un fichier sur le disque
f68:
    mov ax, [bp+R_BX]
    call get_sft
    jc .err
    call file_update
    call fat_flush
    jmp dos_ok
.err:
    jmp dos_err

; 60h : nom complet d'un fichier (DS:SI -> ES:DI)
f60:
    mov si, [bp+R_SI]
    call get_user_path_si
    mov si, dos_path
    mov di, dos_path2
    call path_canon
    jc .err
    mov es, [bp+R_ES]
    mov di, [bp+R_DI]
    mov si, dos_path2
    call copy_str
    mov al, [def_drive]
    call drive_select
    jmp dos_ok
.err:
    mov ax, ERR_NOPATH
    jmp dos_err

; ---------------------------------------------------------------------------
; 4Eh / 4Fh : recherche de fichiers (resultats dans la DTA)
;   DTA+0 lecteur, +1 motif (11), +12 attributs, +13 rang, +15 dossier,
;   +21 attribut trouve, +22 heure, +24 date, +26 taille, +30 nom
; ---------------------------------------------------------------------------
f4E:
    call get_user_path
    mov si, dos_path
    call path_resolve
    jc .nopath
    cmp byte [path_empty], 0
    je .have_pattern
    push ax
    mov di, fname
    mov cx, 11
    mov al, '?'
    push es
    push cs
    pop es
    rep stosb
    pop es
    pop ax
.have_pattern:
    les di, [dta_ptr]
    mov dl, [cur_drive]
    mov [es:di], dl
    mov si, fname
    inc di
    mov cx, 11
    rep movsb
    mov dl, [bp+R_CL]
    mov [es:di], dl             ; +12
    mov word [es:di+1], 0       ; +13 rang
    mov [es:di+3], ax           ; +15 dossier
    jmp find_common
.nopath:
    mov ax, ERR_NOPATH
    jmp dos_err

f4F:
find_common:
    les di, [dta_ptr]
    mov al, [es:di]
    call drive_select
    jc .none
    ; motif
    push di
    lea si, [di+1]
    mov di, fname
    mov cx, 11
.pat:
    mov al, [es:si]
    mov [di], al
    inc si
    inc di
    loop .pat
    pop di
    mov ax, [es:di+15]
    mov cl, [es:di+12]
    mov dx, [es:di+13]
    call find_entry
    jc .none
    ; resultat
    les di, [dta_ptr]
    inc dx
    mov [es:di+13], dx
    mov al, [si+11]
    mov [es:di+21], al
    mov ax, [si+22]
    mov [es:di+22], ax
    mov ax, [si+24]
    mov [es:di+24], ax
    mov eax, [si+28]
    mov [es:di+26], eax
    add di, 30
    call entry_to_text
    mov al, [def_drive]
    call drive_select
    jmp dos_ok
.none:
    mov al, [def_drive]
    call drive_select
    mov ax, ERR_NOMORE
    jmp dos_err

; ---------------------------------------------------------------------------
; con_read : lecture de la console ligne par ligne (handle 0)
;   ES:DI = destination, CX = maximum -> AX = octets lus
; ---------------------------------------------------------------------------
con_line    times 132 db 0
con_left    dw 0
con_pos     dw 0

con_read:
    cmp word [con_left], 0
    jne .copy
    ; nouvelle ligne
    push es
    push di
    push cx
    push cs
    pop es
    mov di, con_line
    xor cx, cx
.key:
    call con_getc
    cmp al, 13
    je .enter
    cmp al, 8
    je .back
    cmp al, 32
    jb .key
    cmp cx, 127
    jae .key
    stosb
    inc cx
    call con_putc
    jmp .key
.back:
    jcxz .key
    dec di
    dec cx
    mov al, 8
    call con_putc
    mov al, ' '
    call con_putc
    mov al, 8
    call con_putc
    jmp .key
.enter:
    mov ax, 0x0A0D
    stosw
    add cx, 2
    mov [con_left], cx
    mov word [con_pos], con_line
    mov al, 13
    call con_putc
    mov al, 10
    call con_putc
    pop cx
    pop di
    pop es
.copy:
    mov ax, cx
    cmp ax, [con_left]
    jbe .n
    mov ax, [con_left]
.n:
    mov cx, ax
    mov si, [con_pos]
    rep movsb
    mov [con_pos], si
    sub [con_left], ax
    ret
