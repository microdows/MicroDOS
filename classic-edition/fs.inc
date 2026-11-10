; ============================================================================
; fs.inc - Couche disque de MicroDOS
;   - lecture/ecriture de secteurs (disquette CHS ou disque dur LBA)
;   - montage d'un lecteur (lecture du BPB, calcul de la geometrie)
;   - FAT12 / FAT16 (fenetre de 2 secteurs en cache)
;   - repertoires (racine fixe ou chaine de clusters)
;   - resolution des chemins (A:\JEUX\DEMON.EXE, ..\TEST.TXT, ...)
;
; Convention : DS = segment du noyau. Les fonctions conservent tous les
; registres sauf ceux indiques en sortie. CF = 1 en cas d'erreur.
; ============================================================================

; ---------- Tampons dans le segment du noyau (0x1000) ----------
FAT_BUF     equ 0x7000      ; fenetre FAT (2 secteurs)
DIR_BUF     equ 0x7400      ; secteur de repertoire courant
SEC_BUF     equ 0x7600      ; secteur de donnees (cache)
BOOT_BUF    equ 0x7800      ; secteur de boot / MBR
BOUNCE_BUF  equ 0x7A00      ; tampon de rebond (frontiere DMA 64 Ko)
RELOC_BUF   equ 0x7C00      ; table de relocations des .EXE (1 Ko)
XFER_BUF    equ 0x8000      ; gros tampon (copy, diskcopy) : 28 Ko
XFER_SIZE   equ 0x7000
KERNEL_MAX  equ 0x7000      ; le noyau ne doit pas depasser 28 Ko

MAX_DRIVES  equ 3           ; A: B: C:

ATTR_RO     equ 0x01
ATTR_HIDDEN equ 0x02
ATTR_SYSTEM equ 0x04
ATTR_VOLUME equ 0x08
ATTR_DIR    equ 0x10
ATTR_ARCH   equ 0x20

; ============================================================================
; Etat du lecteur actif. Le bloc dv_start..dv_end est recopie dans
; drive_table quand on change de lecteur.
; ============================================================================
dv_start:
dv_mounted     db 0
dv_bios        db 0          ; numero BIOS (0=A, 1=B, 0x80=disque dur)
dv_is_hdd      db 0
dv_fat16       db 0
dv_part_lba    dd 0          ; debut de la partition (disque dur)
dv_spc         db 1          ; secteurs par cluster
dv_reserved    dw 1
dv_num_fats    db 2
dv_root_ent    dw 224
dv_fat_size    dw 9
dv_spt         dw 18
dv_heads       dw 2
dv_media       db 0xF0
dv_total       dd 2880
dv_root_lba    dd 19
dv_root_secs   dw 14
dv_data_lba    dd 33
dv_max_clust   dw 2849       ; dernier numero de cluster valide + 1
dv_free_hint   dw 2
dv_serial      dd 0          ; numero de serie du volume
dv_cwd_cluster dw 0          ; repertoire courant (0 = racine)
dv_cwd_path    times 68 db 0 ; ex: "\JEUX" (vide = racine)
dv_end:
DV_SIZE equ dv_end - dv_start

drive_table    times MAX_DRIVES*DV_SIZE db 0
cur_drive      db 0xFF       ; lecteur dont l'etat est dans dv_*
def_drive      db 0          ; lecteur par defaut (celui du prompt)

; ---------- variables internes ----------
dk_op          db 0
dk_tries       db 0
dk_sector      db 0
dk_lba         dd 0
dk_seg         dw 0
dk_off         dw 0
dk_dap         db 0x10, 0
               dw 1
dk_dap_off     dw 0
dk_dap_seg     dw 0
dk_dap_lba     dd 0, 0

sec_cache_lba  dd 0xFFFFFFFF
fat_win        dd 0xFFFFFFFF ; premier secteur (relatif a la FAT) en cache
fat_dirty      db 0
fat_tmp        dw 0

; ============================================================================
; disk_read / disk_write : EAX = LBA (relatif au volume), ES:BX = tampon
; ============================================================================
disk_read:
    mov byte [dk_op], 0x02
    jmp disk_io
disk_write:
    mov byte [dk_op], 0x03
    cmp eax, [sec_cache_lba]
    jne disk_io
    mov dword [sec_cache_lba], 0xFFFFFFFF
disk_io:
    pushad
    push es
    mov [dk_lba], eax
    mov [dk_seg], es
    mov [dk_off], bx

    ; Le DMA de la disquette ne peut pas traverser une frontiere de 64 Ko.
    mov ax, es
    shl ax, 4
    add ax, bx
    cmp ax, 0xFE00
    jbe .direct

    ; ---- passage par le tampon de rebond ----
    cmp byte [dk_op], 0x03
    jne .bounce_read
    push ds
    mov si, bx
    mov ds, [cs:dk_seg]
    push cs
    pop es
    mov di, BOUNCE_BUF
    mov cx, 256
    cld
    rep movsw
    pop ds
.bounce_read:
    push cs
    pop es
    mov bx, BOUNCE_BUF
    call disk_raw
    jc .done
    cmp byte [dk_op], 0x02
    jne .done_ok
    mov es, [dk_seg]
    mov di, [dk_off]
    mov si, BOUNCE_BUF
    mov cx, 256
    cld
    rep movsw
.done_ok:
    clc
    jmp .done

.direct:
    call disk_raw
.done:
    pop es
    popad
    ret

; disk_raw : [dk_lba], [dk_op], ES:BX. Fait jusqu'a 3 essais.
disk_raw:
    mov byte [dk_tries], 3
.retry:
    cmp byte [dv_is_hdd], 0
    jne .hdd

    ; ---- disquette : LBA -> CHS avec la geometrie du BPB ----
    mov eax, [dk_lba]
    xor edx, edx
    movzx ecx, word [dv_spt]
    div ecx                     ; EAX = LBA / spt, EDX = LBA % spt
    inc dl
    mov [dk_sector], dl
    xor edx, edx
    movzx ecx, word [dv_heads]
    div ecx                     ; EAX = cylindre, EDX = tete
    mov dh, dl
    mov ch, al
    mov cl, ah
    shl cl, 6                   ; bits 8-9 du cylindre
    or cl, [dk_sector]
    mov dl, [dv_bios]
    mov ah, [dk_op]
    mov al, 1
    push ds
    push es
    push bx
    int 0x13
    pop bx
    pop es
    pop ds
    jnc .ok
    jmp .fail

.hdd:
    mov [dk_dap_off], bx
    mov [dk_dap_seg], es
    mov eax, [dk_lba]
    add eax, [dv_part_lba]
    mov [dk_dap_lba], eax
    mov ah, [dk_op]
    add ah, 0x40                ; 0x42 = lire, 0x43 = ecrire
    xor al, al
    mov dl, [dv_bios]
    mov si, dk_dap
    push ds
    push es
    push bx
    int 0x13
    pop bx
    pop es
    pop ds
    jnc .ok

.fail:
    ; reinitialiser le lecteur puis reessayer
    xor ax, ax
    mov dl, [dv_bios]
    push ds
    push es
    push bx
    int 0x13
    pop bx
    pop es
    pop ds
    dec byte [dk_tries]
    jnz .retry
    stc
    ret
.ok:
    clc
    ret

; ============================================================================
; Cache d'un secteur de donnees dans SEC_BUF
; sec_load : EAX = LBA -> SEC_BUF contient le secteur
; sec_store : ecrit SEC_BUF au LBA EAX
; ============================================================================
sec_load:
    cmp eax, [sec_cache_lba]
    je .hit
    push es
    push bx
    push cs
    pop es
    mov bx, SEC_BUF
    call disk_read
    pop bx
    pop es
    jc .err
    mov [sec_cache_lba], eax
.hit:
    clc
    ret
.err:
    mov dword [sec_cache_lba], 0xFFFFFFFF
    stc
    ret

sec_store:
    push es
    push bx
    push cs
    pop es
    mov bx, SEC_BUF
    call disk_write
    pop bx
    pop es
    jc .err
    mov [sec_cache_lba], eax
    ret
.err:
    mov dword [sec_cache_lba], 0xFFFFFFFF
    stc
    ret

; ============================================================================
; drive_select : AL = lecteur (0=A, 1=B, 2=C)
; Rend ce lecteur actif (monte-le si besoin). CF=1 si le lecteur est absent.
; ============================================================================
drive_select:
    pushad
    push es
    cmp al, MAX_DRIVES
    jae .bad
    cmp al, [cur_drive]
    je .check

    push ax
    call fat_flush
    push cs
    pop es
    cld
    ; sauvegarder l'etat du lecteur actif
    movzx ax, byte [cur_drive]
    cmp al, 0xFF
    je .load
    mov dx, DV_SIZE
    mul dx
    mov di, drive_table
    add di, ax
    mov si, dv_start
    mov cx, DV_SIZE
    rep movsb
.load:
    pop ax
    mov [cur_drive], al
    xor ah, ah
    mov dx, DV_SIZE
    mul dx
    mov si, drive_table
    add si, ax
    mov di, dv_start
    mov cx, DV_SIZE
    rep movsb
    mov dword [fat_win], 0xFFFFFFFF
    mov byte [fat_dirty], 0
    mov dword [sec_cache_lba], 0xFFFFFFFF

.check:
    cmp byte [dv_mounted], 0
    je .mount
    cmp byte [dv_is_hdd], 0
    jne .ok
    ; disquette : a-t-elle ete changee ? (INT 13h AH=16h)
    mov ah, 0x16
    mov dl, [dv_bios]
    push ds
    int 0x13
    pop ds
    jnc .ok
    cmp ah, 0x06
    jne .ok
    ; le BIOS dit "change" : on verifie le numero de serie avant de tout relire
    call fat_flush
    mov dword [fat_win], 0xFFFFFFFF
    mov dword [sec_cache_lba], 0xFFFFFFFF
    push cs
    pop es
    mov bx, BOOT_BUF
    xor eax, eax
    call disk_read
    jc .mount
    mov eax, [BOOT_BUF+39]
    cmp eax, [dv_serial]
    jne .mount
    movzx eax, word [BOOT_BUF+19]
    test eax, eax
    jnz .cmp_total
    mov eax, [BOOT_BUF+32]
.cmp_total:
    cmp eax, [dv_total]
    je .ok
.mount:
    call drive_mount
    jc .bad
.ok:
    pop es
    popad
    clc
    ret
.bad:
    pop es
    popad
    stc
    ret

; ============================================================================
; drive_mount : lit le BPB du lecteur actif et calcule sa geometrie
; ============================================================================
drive_mount:
    pushad
    push es
    mov byte [dv_mounted], 0
    mov dword [fat_win], 0xFFFFFFFF
    mov byte [fat_dirty], 0
    mov dword [sec_cache_lba], 0xFFFFFFFF
    mov dword [dv_part_lba], 0
    mov word [dv_spt], 18
    mov word [dv_heads], 2

    mov al, [cur_drive]
    mov [dv_bios], al
    mov byte [dv_is_hdd], 0
    cmp al, 2
    jb .reset
    mov byte [dv_bios], 0x80
    mov byte [dv_is_hdd], 1

.reset:
    xor ax, ax
    mov dl, [dv_bios]
    push ds
    int 0x13
    pop ds

    push cs
    pop es
    mov bx, BOOT_BUF
    xor eax, eax
    call disk_read
    jc .fail

    cmp byte [dv_is_hdd], 0
    je .parse_bpb

    ; ---- disque dur : chercher la premiere partition FAT dans le MBR ----
    cmp word [BOOT_BUF+510], 0xAA55
    jne .fail
    mov si, BOOT_BUF+446
    mov cx, 4
.part_loop:
    mov al, [si+4]
    cmp al, 0x01
    je .part_found
    cmp al, 0x04
    je .part_found
    cmp al, 0x06
    je .part_found
    cmp al, 0x0E
    je .part_found
    add si, 16
    loop .part_loop
    jmp .fail
.part_found:
    mov eax, [si+8]
    mov [dv_part_lba], eax
    xor eax, eax
    mov bx, BOOT_BUF
    call disk_read
    jc .fail

.parse_bpb:
    cmp word [BOOT_BUF+11], 512
    jne .fail
    mov al, [BOOT_BUF+13]
    test al, al
    jz .fail
    mov [dv_spc], al
    mov ax, [BOOT_BUF+14]
    test ax, ax
    jz .fail
    mov [dv_reserved], ax
    mov al, [BOOT_BUF+16]
    test al, al
    jz .fail
    mov [dv_num_fats], al
    mov ax, [BOOT_BUF+17]
    mov [dv_root_ent], ax
    mov al, [BOOT_BUF+21]
    mov [dv_media], al
    mov ax, [BOOT_BUF+22]
    test ax, ax
    jz .fail
    mov [dv_fat_size], ax
    cmp byte [dv_is_hdd], 0
    jne .total
    mov ax, [BOOT_BUF+24]
    test ax, ax
    jz .fail
    mov [dv_spt], ax
    mov ax, [BOOT_BUF+26]
    test ax, ax
    jz .fail
    mov [dv_heads], ax
.total:
    movzx eax, word [BOOT_BUF+19]
    test eax, eax
    jnz .have_total
    mov eax, [BOOT_BUF+32]
.have_total:
    mov [dv_total], eax
    mov eax, [BOOT_BUF+39]
    mov [dv_serial], eax

    ; racine = reserves + nb_fats * taille_fat
    movzx eax, byte [dv_num_fats]
    movzx ecx, word [dv_fat_size]
    mul ecx
    movzx ecx, word [dv_reserved]
    add eax, ecx
    mov [dv_root_lba], eax
    ; secteurs de la racine = (entrees * 32 + 511) / 512
    movzx eax, word [dv_root_ent]
    add eax, 15
    shr eax, 4
    mov [dv_root_secs], ax
    add eax, [dv_root_lba]
    mov [dv_data_lba], eax
    ; nombre de clusters = (total - data) / spc
    mov ecx, eax
    mov eax, [dv_total]
    sub eax, ecx
    jbe .fail
    xor edx, edx
    movzx ecx, byte [dv_spc]
    div ecx
    mov byte [dv_fat16], 0
    cmp eax, 4085
    jb .fat12
    mov byte [dv_fat16], 1
    cmp eax, 65525
    jae .fail                   ; FAT32 : pas gere
.fat12:
    add eax, 2
    mov [dv_max_clust], ax
    mov word [dv_free_hint], 2
    mov word [dv_cwd_cluster], 0
    mov byte [dv_cwd_path], 0
    mov byte [dv_mounted], 1
    pop es
    popad
    clc
    ret
.fail:
    pop es
    popad
    stc
    ret

; ============================================================================
; FAT
; ============================================================================

; fat_locate : AX = cluster -> SI = adresse de l'entree dans FAT_BUF
fat_locate:
    push eax
    push ebx
    push ecx
    push edx
    movzx eax, ax
    cmp byte [dv_fat16], 0
    jne .f16
    mov ebx, eax
    shr ebx, 1
    add eax, ebx                ; octet = cluster * 1.5
    jmp .pos
.f16:
    shl eax, 1                  ; octet = cluster * 2
.pos:
    mov ebx, eax
    and ebx, 511                ; position dans le secteur
    shr eax, 9                  ; secteur de la FAT
    ; la fenetre couvre [fat_win, fat_win+1]
    mov ecx, [fat_win]
    cmp ecx, 0xFFFFFFFF
    je .load                    ; fenetre vide
    cmp eax, ecx
    je .in
    cmp byte [dv_fat16], 0
    je .load                    ; FAT12 : on veut toujours ce secteur ET le suivant
    inc ecx
    cmp eax, ecx
    jne .load
    add ebx, 512
    jmp .in
.load:
    call fat_flush
    jc .err
    mov [fat_win], eax
    push es
    push cs
    pop es
    mov ecx, eax
    movzx eax, word [dv_reserved]
    add eax, ecx
    push bx
    mov bx, FAT_BUF
    call disk_read
    jc .rerr
    inc ecx
    cmp cx, [dv_fat_size]
    jae .one
    inc eax
    mov bx, FAT_BUF+512
    call disk_read
    jc .rerr
.one:
    pop bx
    pop es
.in:
    lea si, [bx+FAT_BUF]
    pop edx
    pop ecx
    pop ebx
    pop eax
    clc
    ret
.rerr:
    pop bx
    pop es
    mov dword [fat_win], 0xFFFFFFFF
.err:
    pop edx
    pop ecx
    pop ebx
    pop eax
    stc
    ret

; fat_get : AX = cluster -> AX = valeur (fin de chaine = 0xFFFF, mauvais = 0xFFF7)
fat_get:
    push si
    push bx
    mov bx, ax
    call fat_locate
    jc .err
    mov ax, [si]
    cmp byte [dv_fat16], 0
    jne .done
    test bx, 1
    jz .even
    shr ax, 4
.even:
    and ax, 0x0FFF
    cmp ax, 0x0FF7
    jb .done
    or ax, 0xF000               ; 0xFF7 -> 0xFFF7, 0xFF8+ -> 0xFFF8+
.done:
    cmp ax, 0xFFF8
    jb .ret
    mov ax, 0xFFFF
.ret:
    pop bx
    pop si
    clc
    ret
.err:
    mov ax, 0xFFFF
    pop bx
    pop si
    stc
    ret

; fat_set : AX = cluster, DX = valeur
fat_set:
    push si
    push dx
    push bx
    mov bx, ax
    call fat_locate
    jc .err
    mov byte [fat_dirty], 1
    cmp byte [dv_fat16], 0
    jne .f16
    and dx, 0x0FFF
    test bx, 1
    jz .even
    shl dx, 4
    and word [si], 0x000F
    or [si], dx
    jmp .ok
.even:
    and word [si], 0xF000
    or [si], dx
    jmp .ok
.f16:
    mov [si], dx
.ok:
    pop bx
    pop dx
    pop si
    clc
    ret
.err:
    pop bx
    pop dx
    pop si
    stc
    ret

; fat_flush : ecrit la fenetre FAT sur toutes les copies de la FAT
fat_flush:
    cmp byte [fat_dirty], 0
    je .clean
    pushad
    push es
    push cs
    pop es
    xor cl, cl                  ; numero de copie
.copy:
    cmp cl, [dv_num_fats]
    jae .done
    movzx eax, cl
    movzx edx, word [dv_fat_size]
    mul edx
    add eax, [fat_win]
    movzx edx, word [dv_reserved]
    add eax, edx
    mov bx, FAT_BUF
    call disk_write
    jc .err
    mov edx, [fat_win]
    inc edx
    cmp dx, [dv_fat_size]
    jae .next
    inc eax
    mov bx, FAT_BUF+512
    call disk_write
    jc .err
.next:
    inc cl
    jmp .copy
.done:
    mov byte [fat_dirty], 0
    pop es
    popad
.clean:
    clc
    ret
.err:
    pop es
    popad
    stc
    ret

; fat_alloc : -> AX = nouveau cluster (marque fin de chaine). CF si disque plein.
fat_alloc:
    push cx
    push dx
    mov cx, [dv_max_clust]
    sub cx, 2                   ; nombre de clusters a essayer
    mov ax, [dv_free_hint]
.loop:
    cmp ax, [dv_max_clust]
    jb .test
    mov ax, 2
.test:
    push ax
    call fat_get
    mov dx, ax
    pop ax
    jc .full
    test dx, dx
    jz .found
    inc ax
    loop .loop
.full:
    pop dx
    pop cx
    stc
    ret
.found:
    mov dx, 0xFFFF
    call fat_set
    jc .full
    push ax
    inc ax
    mov [dv_free_hint], ax
    pop ax
    pop dx
    pop cx
    clc
    ret

; fat_free_chain : AX = premier cluster d'une chaine a liberer
fat_free_chain:
    push ax
    push bx
    push dx
.loop:
    cmp ax, 2
    jb .done
    cmp ax, [dv_max_clust]
    jae .done
    mov bx, ax
    call fat_get
    jc .done
    xchg ax, bx
    xor dx, dx
    call fat_set
    cmp ax, [dv_free_hint]
    jae .keep
    mov [dv_free_hint], ax
.keep:
    mov ax, bx
    jmp .loop
.done:
    pop dx
    pop bx
    pop ax
    ret

; fat_next : AX = cluster -> AX = cluster suivant, CF=1 si fin de chaine
fat_next:
    call fat_get
    jc .end
    cmp ax, 2
    jb .end
    cmp ax, [dv_max_clust]
    jae .end
    clc
    ret
.end:
    stc
    ret

; cluster_lba : AX = cluster -> EAX = premier secteur du cluster
cluster_lba:
    push edx
    movzx eax, ax
    sub eax, 2
    movzx edx, byte [dv_spc]
    imul eax, edx
    add eax, [dv_data_lba]
    pop edx
    ret

; free_clusters : -> EAX = nombre de clusters libres
free_clusters:
    push ebx
    push cx
    xor ebx, ebx
    mov cx, 2
.loop:
    cmp cx, [dv_max_clust]
    jae .done
    mov ax, cx
    call fat_get
    jc .done
    test ax, ax
    jnz .next
    inc ebx
.next:
    inc cx
    jmp .loop
.done:
    mov eax, ebx
    pop cx
    pop ebx
    ret

; ============================================================================
; Parcours d'un repertoire, secteur par secteur
;   dir_open  : AX = cluster du repertoire (0 = racine) -> SI = 1re entree
;   dir_next  : SI = entree suivante, CF=1 a la fin du repertoire
;   dir_store : reecrit le secteur courant (DIR_BUF) sur le disque
; ============================================================================
di_cluster  dw 0
di_lba      dd 0
di_left     dw 0

dir_open:
    push eax
    mov [di_cluster], ax
    test ax, ax
    jnz .sub
    mov eax, [dv_root_lba]
    mov [di_lba], eax
    mov ax, [dv_root_secs]
    jmp .set
.sub:
    call cluster_lba
    mov [di_lba], eax
    movzx ax, byte [dv_spc]
.set:
    dec ax
    mov [di_left], ax
    pop eax
    jmp dir_load

dir_load:
    push eax
    push bx
    push es
    push cs
    pop es
    mov bx, DIR_BUF
    mov eax, [di_lba]
    call disk_read
    pop es
    pop bx
    pop eax
    mov si, DIR_BUF
    ret

dir_next:
    add si, 32
    cmp si, DIR_BUF+512
    jb .ok
    cmp word [di_left], 0
    je .next_cluster
    dec word [di_left]
    inc dword [di_lba]
    jmp dir_load
.next_cluster:
    cmp word [di_cluster], 0
    je .end
    push ax
    mov ax, [di_cluster]
    call fat_next
    jc .end_pop
    mov [di_cluster], ax
    push eax
    call cluster_lba
    mov [di_lba], eax
    pop eax
    movzx ax, byte [dv_spc]
    dec ax
    mov [di_left], ax
    pop ax
    jmp dir_load
.end_pop:
    pop ax
.end:
    stc
    ret
.ok:
    clc
    ret

dir_store:
    push eax
    push bx
    push es
    push cs
    pop es
    mov bx, DIR_BUF
    mov eax, [di_lba]
    call disk_write
    pop es
    pop bx
    pop eax
    ret

; ============================================================================
; dir_search : AX = cluster du repertoire, [fname] = nom (11 car., '?' permis)
; Sortie : CF=0 et SI = entree trouvee dans DIR_BUF (secteur [di_lba])
; Ignore les entrees supprimees, les noms longs et le nom de volume.
; ============================================================================
dir_search:
    call dir_open
    jc .err
.loop:
    mov al, [si]
    test al, al
    jz .not_found
    cmp al, 0xE5
    je .next
    test byte [si+11], ATTR_VOLUME
    jnz .next
    call name_match
    je .found
.next:
    call dir_next
    jnc .loop
.not_found:
.err:
    stc
    ret
.found:
    clc
    ret

; name_match : compare [fname] (avec jokers '?') a l'entree SI. ZF=1 si egal.
name_match:
    push si
    push di
    push cx
    push ax
    mov di, fname
    mov cx, 11
.loop:
    mov al, [di]
    cmp al, '?'
    je .skip
    cmp al, [si]
    jne .done
.skip:
    inc si
    inc di
    loop .loop
    xor ax, ax                  ; ZF = 1
.done:
    pop ax
    pop cx
    pop di
    pop si
    ret

; ============================================================================
; dir_alloc : AX = cluster du repertoire -> SI = entree libre dans DIR_BUF
; Agrandit un sous-repertoire plein. CF=1 si la racine est pleine.
; ============================================================================
dir_alloc:
    call dir_open
    jc .err
.loop:
    mov al, [si]
    test al, al
    jz .found
    cmp al, 0xE5
    je .found
    call dir_next
    jnc .loop
    ; fin du repertoire : on ajoute un cluster (sauf racine)
    cmp word [di_cluster], 0
    je .err
    call fat_alloc
    jc .err
    mov dx, ax
    mov ax, [di_cluster]
    call fat_set
    mov ax, dx
    call cluster_zero
    jc .err
    mov [di_cluster], ax
    call cluster_lba
    mov [di_lba], eax
    movzx ax, byte [dv_spc]
    dec ax
    mov [di_left], ax
    call dir_load
    jc .err
.found:
    clc
    ret
.err:
    stc
    ret

; cluster_zero : AX = cluster -> remplit le cluster de zeros
cluster_zero:
    pushad
    push es
    push cs
    pop es
    push ax
    mov di, BOUNCE_BUF
    mov cx, 256
    xor ax, ax
    cld
    rep stosw
    pop ax
    call cluster_lba
    movzx cx, byte [dv_spc]
    mov bx, BOUNCE_BUF
.loop:
    call disk_write
    jc .err
    inc eax
    loop .loop
    pop es
    popad
    clc
    ret
.err:
    pop es
    popad
    stc
    ret

; ============================================================================
; Noms et chemins
; ============================================================================
fname       times 11 db ' '
path_last   dw 0               ; adresse du dernier composant dans la chaine
path_empty  db 0               ; 1 si le chemin ne contient pas de nom final

; is_sep : AL = caractere -> ZF=1 si c'est un separateur de chemin
is_sep:
    cmp al, '\'
    je .ret
    cmp al, '/'
.ret:
    ret

; is_end : AL = caractere -> ZF=1 si c'est la fin d'un argument
is_end:
    cmp al, 0
    je .ret
    cmp al, ' '
    je .ret
    cmp al, 13
    je .ret
    cmp al, 9
.ret:
    ret

; to_upper : AL -> AL en majuscule
to_upper:
    cmp al, 'a'
    jb .ret
    cmp al, 'z'
    ja .ret
    sub al, 32
.ret:
    ret

; make_fatname : SI = "nom.ext" (arret sur separateur ou fin) -> [fname]
; Sortie : SI pointe sur le caractere d'arret.
make_fatname:
    push ax
    push cx
    push di
    mov di, fname
    mov cx, 11
    mov al, ' '
.clear:
    mov [di], al
    inc di
    loop .clear
    mov di, fname

    ; cas speciaux "." et ".."
    cmp byte [si], '.'
    jne .name
    mov byte [fname], '.'
    inc si
    cmp byte [si], '.'
    jne .dots_end
    mov byte [fname+1], '.'
    inc si
.dots_end:
    jmp .skip_rest

.name:
    mov cx, 8
.name_loop:
    mov al, [si]
    call is_end
    je .done
    call is_sep
    je .done
    cmp al, '.'
    je .ext
    inc si
    cmp al, '*'
    je .star_name
    jcxz .name_loop             ; nom trop long : on ignore la suite
    call to_upper
    mov [di], al
    inc di
    dec cx
    jmp .name_loop
.star_name:
    jcxz .name_loop
    mov byte [di], '?'
    inc di
    loop .star_name
    jmp .name_loop

.ext:
    inc si
    mov di, fname+8
    mov cx, 3
.ext_loop:
    mov al, [si]
    call is_end
    je .done
    call is_sep
    je .done
    inc si
    cmp al, '*'
    je .star_ext
    cmp al, '.'
    je .ext_loop
    jcxz .ext_loop
    call to_upper
    mov [di], al
    inc di
    dec cx
    jmp .ext_loop
.star_ext:
    jcxz .ext_loop
    mov byte [di], '?'
    inc di
    loop .star_ext
    jmp .ext_loop

.skip_rest:
    mov al, [si]
    call is_end
    je .done
    call is_sep
    je .done
    inc si
    jmp .skip_rest
.done:
    pop di
    pop cx
    pop ax
    ret

; ============================================================================
; path_resolve : SI = chemin (ex: "B:\JEUX\DEMON.EXE", "..\X.TXT")
; Sortie : CF=0, AX = cluster du repertoire parent, [fname] = nom final,
;          [path_empty] = 1 s'il n'y a pas de nom final ("A:", "\JEUX\")
;          SI pointe apres le chemin. Le lecteur du chemin devient actif.
; CF=1 si le lecteur ou un repertoire intermediaire n'existe pas.
; ============================================================================
path_resolve:
    push bx
    push dx
.skip_spaces:
    cmp byte [si], ' '
    jne .drive
    inc si
    jmp .skip_spaces
.drive:
    mov al, [def_drive]
    cmp byte [si+1], ':'
    jne .select
    mov al, [si]
    call to_upper
    sub al, 'A'
    add si, 2
.select:
    call drive_select
    jc .err

    mov bx, [dv_cwd_cluster]
    mov al, [si]
    call is_sep
    jne .component
    xor bx, bx                  ; chemin absolu : on part de la racine
    inc si

.component:
    mov byte [path_empty], 0
    mov [path_last], si
    mov al, [si]
    call is_end
    jne .parse
    mov byte [path_empty], 1
    mov ax, bx
    jmp .ok
.parse:
    call make_fatname
    mov al, [si]
    call is_sep
    jne .final

    ; composant intermediaire : il doit s'agir d'un repertoire
    inc si
    cmp word [fname], '. '
    je .component               ; "." : on reste ici
    cmp word [fname], '..'
    jne .enter
    test bx, bx
    jz .component               ; ".." depuis la racine : on reste a la racine
.enter:
    push si
    mov ax, bx
    call dir_search
    jc .err_pop
    test byte [si+11], ATTR_DIR
    jz .err_pop
    mov bx, [si+26]
    pop si
    jmp .component

.final:
    mov ax, bx
.ok:
    pop dx
    pop bx
    clc
    ret
.err_pop:
    pop si
.err:
    pop dx
    pop bx
    stc
    ret

; path_lookup : SI = chemin -> CF=0, SI = entree trouvee dans DIR_BUF,
;               AX = cluster du repertoire parent.
; "." et ".." sont geres ; la racine n'est pas une entree (CF=1).
path_lookup:
    call path_resolve
    jc .err
    cmp byte [path_empty], 0
    jne .err
    push ax
    call dir_search
    pop ax
    ret
.err:
    stc
    ret
