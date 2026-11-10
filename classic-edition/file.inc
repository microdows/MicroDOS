; ============================================================================
; file.inc - Fichiers ouverts (table SFT) et operations sur les fichiers
;   Taille et position sur 32 bits : plus de limite de 64 Ko.
; ============================================================================

SFT_COUNT   equ 24
SFT_SIZE    equ 32

F_USED      equ 0       ; db : 0 libre, 1 fichier, 2 console (CON)
F_REFS      equ 1       ; db : nombre de handles qui pointent ici
F_DRIVE     equ 2       ; db
F_MODE      equ 3       ; db : 0 lecture, 1 ecriture, 2 lecture/ecriture
F_FIRST     equ 4       ; dw : premier cluster (0 = fichier vide)
F_CUR       equ 6       ; dw : cluster courant (0 = a recalculer)
F_SIZE      equ 8       ; dd
F_POS       equ 12      ; dd
F_CIDX      equ 16      ; dw : rang de F_CUR dans la chaine
F_DOFF      equ 18      ; dw : position de l'entree dans son secteur
F_DLBA      equ 20      ; dd : secteur de l'entree de repertoire
F_ATTR      equ 24      ; db
F_DIRTY     equ 25      ; db : 1 si l'entree de repertoire doit etre mise a jour
F_TIME      equ 26      ; dw
F_DATE      equ 28      ; dw

ERR_FUNC    equ 1
ERR_NOFILE  equ 2
ERR_NOPATH  equ 3
ERR_HANDLES equ 4
ERR_ACCESS  equ 5
ERR_HANDLE  equ 6
ERR_NOMEM   equ 8
ERR_BADMEM  equ 9
ERR_FORMAT  equ 11
ERR_DRIVE   equ 15
ERR_NOMORE  equ 18

sft_table   times SFT_COUNT*SFT_SIZE db 0

fx_count    dd 0        ; octets restant a traiter
fx_done     dd 0        ; octets traites
fx_chunk    dw 0
fx_off      dw 0        ; position dans le secteur
fx_lba      dd 0
fx_parent   dw 0
fx_attr     db 0

; ---------------------------------------------------------------------------
; sft_init : l'entree 0 est la console (stdin/stdout/stderr)
; ---------------------------------------------------------------------------
sft_init:
    mov byte [sft_table+F_USED], 2
    mov byte [sft_table+F_REFS], 0xFF
    ret

; sft_alloc : -> BX = entree libre (F_REFS = 1). CF si table pleine.
sft_alloc:
    push cx
    mov bx, sft_table+SFT_SIZE
    mov cx, SFT_COUNT-1
.loop:
    cmp byte [bx+F_USED], 0
    je .found
    add bx, SFT_SIZE
    loop .loop
    pop cx
    stc
    ret
.found:
    push di
    push ax
    push es
    push cs
    pop es
    mov di, bx
    mov cx, SFT_SIZE
    xor al, al
    cld
    rep stosb
    pop es
    pop ax
    pop di
    mov byte [bx+F_REFS], 1
    pop cx
    clc
    ret

; sft_fill : BX = entree SFT, SI = entree de repertoire dans DIR_BUF
sft_fill:
    push eax
    mov byte [bx+F_USED], 1
    mov al, [cur_drive]
    mov [bx+F_DRIVE], al
    mov ax, [si+26]
    mov [bx+F_FIRST], ax
    mov word [bx+F_CUR], 0
    mov word [bx+F_CIDX], 0
    mov eax, [si+28]
    mov [bx+F_SIZE], eax
    mov dword [bx+F_POS], 0
    mov eax, [di_lba]
    mov [bx+F_DLBA], eax
    mov ax, si
    sub ax, DIR_BUF
    mov [bx+F_DOFF], ax
    mov al, [si+11]
    mov [bx+F_ATTR], al
    mov byte [bx+F_DIRTY], 0
    mov ax, [si+22]
    mov [bx+F_TIME], ax
    mov ax, [si+24]
    mov [bx+F_DATE], ax
    pop eax
    ret

; ---------------------------------------------------------------------------
; dos_datetime : -> AX = heure DOS, DX = date DOS (depuis l'horloge BIOS)
; ---------------------------------------------------------------------------
dos_datetime:
    push bx
    push cx
    mov ah, 0x02
    int 0x1A                    ; CH=heures CL=minutes DH=secondes (BCD)
    mov al, ch
    call bcd_to_bin
    movzx bx, al
    shl bx, 11
    mov al, cl
    call bcd_to_bin
    movzx ax, al
    shl ax, 5
    or bx, ax
    mov al, dh
    call bcd_to_bin
    shr al, 1
    movzx ax, al
    or bx, ax
    push bx
    mov ah, 0x04
    int 0x1A                    ; CH=siecle CL=annee DH=mois DL=jour
    mov al, ch
    call bcd_to_bin
    mov bl, 100
    mul bl
    mov bx, ax
    mov al, cl
    call bcd_to_bin
    xor ah, ah
    add ax, bx
    sub ax, 1980
    jns .year_ok
    xor ax, ax
.year_ok:
    shl ax, 9
    mov bx, ax
    mov al, dh
    call bcd_to_bin
    xor ah, ah
    shl ax, 5
    or bx, ax
    mov al, dl
    call bcd_to_bin
    xor ah, ah
    or bx, ax
    mov dx, bx
    pop ax
    pop cx
    pop bx
    ret

bcd_to_bin:
    push cx
    mov cl, al
    shr al, 4
    mov ch, 10
    mul ch
    and cl, 0x0F
    add al, cl
    pop cx
    ret

; ---------------------------------------------------------------------------
; file_open : SI = chemin, AL = mode (0/1/2) -> BX = entree SFT
; CF=1 et AX = code d'erreur DOS sinon.
; ---------------------------------------------------------------------------
file_open:
    push si
    push dx
    and al, 3
    mov dl, al
    call path_resolve
    jc .no_path
    cmp byte [path_empty], 0
    jne .no_file
    call dir_search
    jc .no_file
    test byte [si+11], ATTR_DIR | ATTR_VOLUME
    jnz .access
    test dl, dl
    jz .mode_ok
    test byte [si+11], ATTR_RO
    jnz .access
.mode_ok:
    call sft_alloc
    jc .too_many
    call sft_fill
    mov [bx+F_MODE], dl
    pop dx
    pop si
    clc
    ret
.no_path:
    mov ax, ERR_NOPATH
    jmp .err
.no_file:
    mov ax, ERR_NOFILE
    jmp .err
.access:
    mov ax, ERR_ACCESS
    jmp .err
.too_many:
    mov ax, ERR_HANDLES
.err:
    pop dx
    pop si
    stc
    ret

; ---------------------------------------------------------------------------
; file_create : SI = chemin, CL = attributs -> BX = entree SFT (fichier vide)
; Si le fichier existe, il est vide.
; ---------------------------------------------------------------------------
file_create:
    push si
    push di
    push cx
    push dx
    mov [fx_attr], cl
    call path_resolve
    jc .no_path
    cmp byte [path_empty], 0
    jne .no_path
    ; pas de jokers dans un nom a creer
    mov di, fname
    mov cx, 11
.chk:
    cmp byte [di], '?'
    je .no_path
    inc di
    loop .chk
    mov [fx_parent], ax
    call dir_search
    jc .new
    test byte [si+11], ATTR_DIR | ATTR_VOLUME | ATTR_RO
    jnz .access
    mov ax, [si+26]
    call fat_free_chain
    jmp .fill
.new:
    mov ax, [fx_parent]
    call dir_alloc
    jc .access
    push si
    mov di, si
    mov si, fname
    mov cx, 11
    cld
    push es
    push cs
    pop es
    rep movsb
    mov cx, 21
    xor al, al
    rep stosb
    pop es
    pop si
.fill:
    mov al, [fx_attr]
    and al, ATTR_RO | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_ARCH
    or al, ATTR_ARCH
    mov [si+11], al
    mov word [si+26], 0
    mov dword [si+28], 0
    call dos_datetime
    mov [si+22], ax
    mov [si+24], dx
    call dir_store
    jc .access
    call fat_flush
    call sft_alloc
    jc .too_many
    call sft_fill
    mov byte [bx+F_MODE], 2
    pop dx
    pop cx
    pop di
    pop si
    clc
    ret
.no_path:
    mov ax, ERR_NOPATH
    jmp .err
.access:
    mov ax, ERR_ACCESS
    jmp .err
.too_many:
    mov ax, ERR_HANDLES
.err:
    pop dx
    pop cx
    pop di
    pop si
    stc
    ret

; ---------------------------------------------------------------------------
; file_locate : BX = SFT -> position F_POS : EAX = secteur, [fx_off] = octet
; Si alloc=1 ([fx_alloc]), agrandit la chaine si besoin.
; CF=1 si la position est hors de la chaine (et alloc=0) ou disque plein.
; ---------------------------------------------------------------------------
fx_alloc    db 0
fx_target   dw 0
fx_insec    dw 0

file_locate:
    push ecx
    push edx
    ; rang du cluster et secteur dans le cluster
    movzx ecx, byte [dv_spc]
    shl ecx, 9                  ; taille d'un cluster en octets
    mov eax, [bx+F_POS]
    xor edx, edx
    div ecx                     ; EAX = rang, EDX = octet dans le cluster
    mov [fx_target], ax
    mov ax, dx
    and ax, 511
    mov [fx_off], ax
    shr edx, 9
    mov [fx_insec], dx

    ; premier cluster
    mov ax, [bx+F_FIRST]
    test ax, ax
    jnz .have_first
    cmp byte [fx_alloc], 0
    je .err
    call fat_alloc
    jc .err
    mov [bx+F_FIRST], ax
    mov byte [bx+F_DIRTY], 1
    mov word [bx+F_CUR], 0
.have_first:
    ; partir du cluster courant si on est deja avance
    mov cx, [bx+F_CIDX]
    mov dx, [bx+F_CUR]
    test dx, dx
    jz .from_start
    cmp cx, [fx_target]
    jbe .walk
.from_start:
    mov dx, [bx+F_FIRST]
    xor cx, cx
.walk:
    cmp cx, [fx_target]
    je .found
    mov ax, dx
    call fat_next
    jnc .next_ok
    ; fin de chaine
    cmp byte [fx_alloc], 0
    je .err
    call fat_alloc
    jc .err
    push ax
    xchg ax, dx                 ; AX = dernier cluster, DX = nouveau
    call fat_set
    pop ax
.next_ok:
    mov dx, ax
    inc cx
    jmp .walk
.found:
    mov [bx+F_CUR], dx
    mov [bx+F_CIDX], cx
    mov ax, dx
    call cluster_lba
    movzx edx, word [fx_insec]
    add eax, edx
    pop edx
    pop ecx
    clc
    ret
.err:
    pop edx
    pop ecx
    stc
    ret

; normalise ES:DI pour que DI reste < 16
norm_esdi:
    push ax
    mov ax, di
    shr ax, 4
    push bx
    mov bx, es
    add bx, ax
    mov es, bx
    pop bx
    and di, 15
    pop ax
    ret

; ---------------------------------------------------------------------------
; file_read : BX = SFT, ES:DI = destination, ECX = nombre d'octets
; Sortie : EAX = octets lus. CF=1 en cas d'erreur disque.
; ---------------------------------------------------------------------------
file_read:
    push ecx
    push edx
    push si
    push di
    push es
    mov al, [bx+F_DRIVE]
    call drive_select
    jc .err
    mov dword [fx_done], 0
    ; ne pas lire au-dela de la fin du fichier
    mov eax, [bx+F_SIZE]
    sub eax, [bx+F_POS]
    jbe .done
    cmp ecx, eax
    jbe .count_ok
    mov ecx, eax
.count_ok:
    mov [fx_count], ecx
    mov byte [fx_alloc], 0
.loop:
    cmp dword [fx_count], 0
    je .done
    call norm_esdi
    call file_locate
    jc .err
    mov [fx_lba], eax
    mov dx, 512
    sub dx, [fx_off]
    movzx edx, dx
    cmp edx, [fx_count]
    jbe .chunk_ok
    mov edx, [fx_count]
.chunk_ok:
    mov [fx_chunk], dx
    cmp dx, 512
    jne .partial
    ; secteur complet : lecture directe dans le tampon de l'appelant
    push bx
    mov bx, di
    call disk_read
    pop bx
    jc .err
    jmp .advance
.partial:
    call sec_load
    jc .err
    mov si, SEC_BUF
    add si, [fx_off]
    movzx ecx, word [fx_chunk]
    cld
    rep movsb
    sub di, [fx_chunk]
.advance:
    movzx edx, word [fx_chunk]
    add di, dx
    add [bx+F_POS], edx
    add [fx_done], edx
    sub [fx_count], edx
    jmp .loop
.done:
    mov eax, [fx_done]
    pop es
    pop di
    pop si
    pop edx
    pop ecx
    clc
    ret
.err:
    mov eax, [fx_done]
    pop es
    pop di
    pop si
    pop edx
    pop ecx
    stc
    ret

; ---------------------------------------------------------------------------
; file_write : BX = SFT, ES:DI = source, ECX = nombre d'octets
; Sortie : EAX = octets ecrits. ECX = 0 : tronque le fichier a la position.
; ---------------------------------------------------------------------------
file_write:
    push ecx
    push edx
    push si
    push di
    push es
    push ds
    mov al, [bx+F_DRIVE]
    call drive_select
    jc .err
    mov dword [fx_done], 0
    test ecx, ecx
    jz .truncate
    mov [fx_count], ecx
    mov byte [fx_alloc], 1
.loop:
    cmp dword [fx_count], 0
    je .done
    call norm_esdi
    call file_locate
    jc .err
    mov [fx_lba], eax
    mov dx, 512
    sub dx, [fx_off]
    movzx edx, dx
    cmp edx, [fx_count]
    jbe .chunk_ok
    mov edx, [fx_count]
.chunk_ok:
    mov [fx_chunk], dx
    cmp dx, 512
    jne .partial
    push bx
    mov bx, di
    call disk_write
    pop bx
    jc .err
    jmp .advance
.partial:
    call sec_load
    jc .err
    ; copier ES:DI -> SEC_BUF+off
    push es
    push di
    mov si, di
    mov di, SEC_BUF
    add di, [fx_off]
    movzx ecx, word [fx_chunk]
    push ds
    push es
    pop ds
    push cs
    pop es
    cld
    rep movsb
    pop ds
    pop di
    pop es
    mov eax, [fx_lba]
    call sec_store
    jc .err
.advance:
    movzx edx, word [fx_chunk]
    add di, dx
    add [bx+F_POS], edx
    add [fx_done], edx
    sub [fx_count], edx
    mov eax, [bx+F_POS]
    cmp eax, [bx+F_SIZE]
    jbe .loop
    mov [bx+F_SIZE], eax
    mov byte [bx+F_DIRTY], 1
    jmp .loop

.truncate:
    ; la taille devient la position courante
    mov eax, [bx+F_POS]
    cmp eax, [bx+F_SIZE]
    jae .done
    mov [bx+F_SIZE], eax
    mov byte [bx+F_DIRTY], 1
    mov word [bx+F_CUR], 0
    test eax, eax
    jnz .keep_part
    mov ax, [bx+F_FIRST]
    call fat_free_chain
    mov word [bx+F_FIRST], 0
    jmp .done
.keep_part:
    ; garder les clusters jusqu'a la position-1, liberer la suite
    push dword [bx+F_POS]
    dec dword [bx+F_POS]
    mov byte [fx_alloc], 0
    call file_locate
    pop dword [bx+F_POS]
    jc .err
    mov ax, [bx+F_CUR]
    push ax
    call fat_next
    mov si, ax
    pop ax
    jc .done
    mov dx, 0xFFFF
    call fat_set
    mov ax, si
    call fat_free_chain
.done:
    mov byte [bx+F_DIRTY], 1
    call fat_flush
    mov eax, [fx_done]
    pop ds
    pop es
    pop di
    pop si
    pop edx
    pop ecx
    clc
    ret
.err:
    call fat_flush
    mov eax, [fx_done]
    pop ds
    pop es
    pop di
    pop si
    pop edx
    pop ecx
    stc
    ret

; ---------------------------------------------------------------------------
; file_update : BX = SFT -> ecrit taille, premier cluster et date dans le
; repertoire si le fichier a change
; ---------------------------------------------------------------------------
file_update:
    cmp byte [bx+F_USED], 1
    jne .ok
    cmp byte [bx+F_DIRTY], 0
    je .ok
    push eax
    push dx
    push si
    mov al, [bx+F_DRIVE]
    call drive_select
    jc .err
    mov eax, [bx+F_DLBA]
    mov [di_lba], eax
    call dir_load
    jc .err
    add si, [bx+F_DOFF]
    mov ax, [bx+F_FIRST]
    mov [si+26], ax
    mov eax, [bx+F_SIZE]
    mov [si+28], eax
    or byte [si+11], ATTR_ARCH
    call dos_datetime
    mov [si+22], ax
    mov [si+24], dx
    mov [bx+F_TIME], ax
    mov [bx+F_DATE], dx
    call dir_store
    jc .err
    call fat_flush
    mov byte [bx+F_DIRTY], 0
    pop si
    pop dx
    pop eax
.ok:
    clc
    ret
.err:
    pop si
    pop dx
    pop eax
    stc
    ret

; file_close : BX = SFT
file_close:
    cmp byte [bx+F_USED], 2
    je .ok
    cmp byte [bx+F_USED], 0
    je .bad
    call file_update
    dec byte [bx+F_REFS]
    jnz .ok
    mov byte [bx+F_USED], 0
.ok:
    clc
    ret
.bad:
    stc
    ret

; ---------------------------------------------------------------------------
; file_delete : SI = chemin
; ---------------------------------------------------------------------------
file_delete:
    push si
    call path_lookup
    jc .nofile
    test byte [si+11], ATTR_DIR | ATTR_VOLUME | ATTR_RO
    jnz .access
    mov byte [si], 0xE5
    call dir_store
    jc .access
    mov ax, [si+26]
    call fat_free_chain
    call fat_flush
    pop si
    clc
    ret
.nofile:
    mov ax, ERR_NOFILE
    pop si
    stc
    ret
.access:
    mov ax, ERR_ACCESS
    pop si
    stc
    ret

; ---------------------------------------------------------------------------
; file_rename : SI = ancien chemin, DI = nouveau chemin
; ---------------------------------------------------------------------------
fr_entry    times 32 db 0
fr_lba      dd 0
fr_off      dw 0
fr_parent   dw 0
fr_drive    db 0

file_rename:
    push si
    push di
    push cx
    call path_lookup
    jc .nofile
    test byte [si+11], ATTR_VOLUME
    jnz .access
    mov [fr_parent], ax
    mov al, [cur_drive]
    mov [fr_drive], al
    mov eax, [di_lba]
    mov [fr_lba], eax
    mov ax, si
    sub ax, DIR_BUF
    mov [fr_off], ax
    push di
    push es
    push cs
    pop es
    mov di, fr_entry
    mov cx, 32
    cld
    rep movsb
    pop es
    pop di

    ; nouveau nom
    mov si, di
    call path_resolve
    jc .nopath
    cmp byte [path_empty], 0
    jne .nopath
    mov cl, [cur_drive]
    cmp cl, [fr_drive]
    jne .access                 ; pas de deplacement entre lecteurs
    mov [fx_parent], ax
    push ax
    call dir_search
    pop ax
    jnc .access                 ; le nouveau nom existe deja
    cmp ax, [fr_parent]
    je .same_dir
    test byte [fr_entry+11], ATTR_DIR
    jnz .access                 ; on ne deplace pas un dossier

    ; nouvelle entree dans l'autre dossier
    call dir_alloc
    jc .access
    mov di, si
    push es
    push cs
    pop es
    mov si, fname
    mov cx, 11
    cld
    rep movsb
    mov si, fr_entry+11
    mov cx, 21
    rep movsb
    pop es
    call dir_store
    jc .access
    ; effacer l'ancienne entree
    mov eax, [fr_lba]
    mov [di_lba], eax
    call dir_load
    jc .access
    add si, [fr_off]
    mov byte [si], 0xE5
    call dir_store
    jc .access
    jmp .ok

.same_dir:
    mov eax, [fr_lba]
    mov [di_lba], eax
    call dir_load
    jc .access
    add si, [fr_off]
    mov di, si
    push es
    push cs
    pop es
    mov si, fname
    mov cx, 11
    cld
    rep movsb
    pop es
    call dir_store
    jc .access
.ok:
    pop cx
    pop di
    pop si
    clc
    ret
.nofile:
    mov ax, ERR_NOFILE
    jmp .err
.nopath:
    mov ax, ERR_NOPATH
    jmp .err
.access:
    mov ax, ERR_ACCESS
.err:
    pop cx
    pop di
    pop si
    stc
    ret

; ---------------------------------------------------------------------------
; dir_make : SI = chemin du nouveau dossier
; ---------------------------------------------------------------------------
dm_cluster  dw 0

dir_make:
    push si
    push di
    push cx
    push dx
    call path_resolve
    jc .nopath
    cmp byte [path_empty], 0
    jne .nopath
    cmp byte [fname], '.'
    je .access
    mov [fx_parent], ax
    push ax
    call dir_search
    pop ax
    jnc .access                 ; existe deja
    ; cluster du nouveau dossier
    call fat_alloc
    jc .access
    mov [dm_cluster], ax
    call cluster_zero
    jc .access
    ; entree dans le dossier parent
    mov ax, [fx_parent]
    call dir_alloc
    jc .access
    mov di, si
    push es
    push cs
    pop es
    push si
    mov si, fname
    mov cx, 11
    cld
    rep movsb
    mov cx, 21
    xor al, al
    rep stosb
    pop si
    pop es
    mov byte [si+11], ATTR_DIR
    mov ax, [dm_cluster]
    mov [si+26], ax
    call dos_datetime
    mov [si+22], ax
    mov [si+24], dx
    call dir_store
    jc .access
    ; "." et ".." dans le nouveau dossier
    mov ax, [dm_cluster]
    call dir_open
    jc .access
    mov di, si
    push es
    push cs
    pop es
    mov si, dot_entries
    mov cx, 64
    cld
    rep movsb
    pop es
    mov ax, [dm_cluster]
    mov [DIR_BUF+26], ax
    mov ax, [fx_parent]
    mov [DIR_BUF+32+26], ax
    call dos_datetime
    mov [DIR_BUF+22], ax
    mov [DIR_BUF+24], dx
    mov [DIR_BUF+32+22], ax
    mov [DIR_BUF+32+24], dx
    call dir_store
    jc .access
    call fat_flush
    pop dx
    pop cx
    pop di
    pop si
    clc
    ret
.nopath:
    mov ax, ERR_NOPATH
    jmp .err
.access:
    call fat_flush
    mov ax, ERR_ACCESS
.err:
    pop dx
    pop cx
    pop di
    pop si
    stc
    ret

dot_entries:
    db '.          ', ATTR_DIR
    times 20 db 0
    db '..         ', ATTR_DIR
    times 20 db 0

; ---------------------------------------------------------------------------
; dir_remove : SI = chemin d'un dossier vide
; ---------------------------------------------------------------------------
dr_cluster  dw 0
dr_lba      dd 0
dr_off      dw 0

dir_remove:
    push si
    call path_lookup
    jc .nopath
    test byte [si+11], ATTR_DIR
    jz .access
    cmp byte [si], '.'
    je .access
    mov ax, [si+26]
    mov [dr_cluster], ax
    cmp ax, [dv_cwd_cluster]
    je .access                  ; on ne supprime pas le dossier courant
    mov eax, [di_lba]
    mov [dr_lba], eax
    mov ax, si
    sub ax, DIR_BUF
    mov [dr_off], ax
    ; le dossier doit etre vide
    mov ax, [dr_cluster]
    call dir_open
    jc .access
.check:
    mov al, [si]
    test al, al
    jz .empty
    cmp al, 0xE5
    je .next
    cmp al, '.'
    jne .access
.next:
    call dir_next
    jnc .check
.empty:
    mov eax, [dr_lba]
    mov [di_lba], eax
    call dir_load
    jc .access
    add si, [dr_off]
    mov byte [si], 0xE5
    call dir_store
    jc .access
    mov ax, [dr_cluster]
    call fat_free_chain
    call fat_flush
    pop si
    clc
    ret
.nopath:
    mov ax, ERR_NOPATH
    pop si
    stc
    ret
.access:
    mov ax, ERR_ACCESS
    pop si
    stc
    ret

; ---------------------------------------------------------------------------
; dir_change : SI = chemin -> change le dossier courant de ce lecteur
; ---------------------------------------------------------------------------
dc_text     times 80 db 0

dir_change:
    push si
    push di
    push cx
    push si
    call path_resolve
    jc .bad_pop
    cmp byte [path_empty], 0
    jne .have
    ; dernier composant : "." / ".." / nom
    cmp word [fname], '. '
    je .have
    cmp word [fname], '..'
    jne .search
    test ax, ax
    jz .have                    ; ".." depuis la racine
.search:
    call dir_search
    jc .bad_pop
    test byte [si+11], ATTR_DIR
    jz .bad_pop
    mov ax, [si+26]
.have:
    mov [fx_parent], ax
    ; texte du nouveau chemin
    pop si
    mov di, dc_text
    call path_canon
    jc .bad
    mov ax, [fx_parent]
    mov [dv_cwd_cluster], ax
    ; dv_cwd_path = chemin sans "X:" (et vide pour la racine)
    mov si, dc_text+2
    cmp byte [si+1], 0
    jne .copy
    inc si                      ; "\" seul -> chaine vide
.copy:
    mov di, dv_cwd_path
    push es
    push cs
    pop es
    call copy_str
    pop es
    pop cx
    pop di
    pop si
    clc
    ret
.bad_pop:
    pop si
.bad:
    mov ax, ERR_NOPATH
    pop cx
    pop di
    pop si
    stc
    ret

; ---------------------------------------------------------------------------
; path_canon : SI = chemin, DI = sortie (ex: "A:\JEUX\DEMON.EXE")
; Traite ".", "..", majuscules. Ne verifie pas que le chemin existe.
; ---------------------------------------------------------------------------
pc_out      dw 0
pc_end      dw 0

path_canon:
    pushad
    push es
    push cs
    pop es
    mov [pc_out], di
.skip:
    cmp byte [si], ' '
    jne .drive
    inc si
    jmp .skip
.drive:
    mov al, [def_drive]
    cmp byte [si+1], ':'
    jne .sel
    mov al, [si]
    call to_upper
    sub al, 'A'
    add si, 2
.sel:
    call drive_select
    jc .err
    mov al, [cur_drive]
    add al, 'A'
    mov [di], al
    mov byte [di+1], ':'
    add di, 2
    mov [pc_end], di
    mov byte [di], 0
    mov al, [si]
    call is_sep
    je .absolute
    ; relatif : on part du dossier courant
    push si
    mov si, dv_cwd_path
    call copy_str
    dec di
    mov [pc_end], di
    pop si
    jmp .comp
.absolute:
    inc si
.comp:
    mov al, [si]
    call is_end
    je .finish
    call is_sep
    jne .name
    inc si
    jmp .comp
.name:
    call make_fatname
    cmp word [fname], '. '
    je .comp
    cmp word [fname], '..'
    jne .append
    ; remonter d'un niveau : effacer jusqu'au dernier '\'
    mov di, [pc_end]
.back:
    mov bx, [pc_out]
    add bx, 2
    cmp di, bx
    jbe .back_done
    dec di
    cmp byte [di], '\'
    jne .back
.back_done:
    mov byte [di], 0
    mov [pc_end], di
    jmp .comp
.append:
    mov di, [pc_end]
    mov ax, di
    sub ax, [pc_out]
    cmp ax, 64
    jae .err                    ; chemin trop long
    mov byte [di], '\'
    inc di
    call fatname_to_text
    mov [pc_end], di
    mov byte [di], 0
    jmp .comp
.finish:
    mov di, [pc_end]
    mov bx, [pc_out]
    add bx, 2
    cmp di, bx
    jne .done
    mov byte [di], '\'          ; racine : "A:\"
    mov byte [di+1], 0
.done:
    pop es
    popad
    clc
    ret
.err:
    pop es
    popad
    stc
    ret

; fatname_to_text : [fname] -> ES:DI "NOM.EXT" (DI avance, pas de 0 final)
fatname_to_text:
    push si
    push cx
    push ax
    mov si, fname
    mov cx, 8
.name:
    lodsb
    cmp al, ' '
    je .name_end
    stosb
    loop .name
.name_end:
    cmp byte [fname+8], ' '
    je .done
    mov al, '.'
    stosb
    mov si, fname+8
    mov cx, 3
.ext:
    lodsb
    cmp al, ' '
    je .done
    stosb
    loop .ext
.done:
    pop ax
    pop cx
    pop si
    ret

; entry_to_text : SI = entree de repertoire, ES:DI = sortie ASCIIZ "NOM.EXT"
entry_to_text:
    push si
    push di
    push es
    push cx
    push di
    push es
    push cs
    pop es
    mov di, fname
    mov cx, 11
    cld
    rep movsb
    pop es
    pop di
    call fatname_to_text
    mov byte [es:di], 0
    pop cx
    pop es
    pop di
    pop si
    ret

; ---------------------------------------------------------------------------
; find_entry : AX = cluster du dossier, [fname] = motif, CL = attributs
;              admis, DX = rang de depart -> SI = entree, DX = son rang
; ---------------------------------------------------------------------------
fe_attr     db 0

find_entry:
    push ax
    push bx
    mov [fe_attr], cl
    xor bx, bx                  ; rang courant
    call dir_open
    jc .none
.loop:
    mov al, [si]
    test al, al
    jz .none
    cmp bx, dx
    jb .next
    cmp al, 0xE5
    je .next
    mov al, [si+11]
    cmp al, 0x0F
    je .next                    ; nom long
    test al, ATTR_VOLUME
    jz .not_vol
    cmp byte [fe_attr], ATTR_VOLUME
    je .match_name              ; on cherche le nom de volume
    jmp .next
.not_vol:
    cmp byte [fe_attr], ATTR_VOLUME
    je .next
    and al, ATTR_HIDDEN | ATTR_SYSTEM | ATTR_DIR
    not al
    or al, [fe_attr]
    not al
    test al, al
    jnz .next                   ; attribut non demande
.match_name:
    call name_match
    je .found
.next:
    inc bx
    call dir_next
    jnc .loop
.none:
    pop bx
    pop ax
    stc
    ret
.found:
    mov dx, bx
    pop bx
    pop ax
    clc
    ret
