; ============================================
; format.inc - Formater un disque FAT16
; ============================================

format_hdd:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es

    ; --- 1. Écrire le BPB à LBA 2048 ---
    mov ax, 0x1000
    mov es, ax
    mov di, 0x7000
    
    mov cx, 256
    xor ax, ax
    rep stosw

    mov ax, 0x1000
    mov es, ax
    mov di, 0x7000
    
    ; Saut de boot
    mov byte [es:di], 0xEB
    mov byte [es:di+1], 0x3C
    mov byte [es:di+2], 0x90
    
    ; OEM Name (MSDOS5.0)
    mov si, oem_name
    mov cx, 8
    rep movsb
    
    ; IMPORTANT : On remet DI au début du BPB !
    mov di, 0x7000
    
    ; BytesPerSector = 512
    mov word [es:di+11], 512
    ; SectorsPerCluster = 4
    mov byte [es:di+13], 4
    ; ReservedSectors = 4
    mov word [es:di+14], 4
    ; NumFATs = 2
    mov byte [es:di+16], 2
    ; RootEntries = 512
    mov word [es:di+17], 512
    ; TotalSectors16 = 0
    mov word [es:di+19], 0
    ; MediaDescriptor = 0xF8
    mov byte [es:di+21], 0xF8
    ; SectorsPerFAT = 64
    mov word [es:di+22], 64
    ; SectorsPerTrack = 18
    mov word [es:di+24], 18
    ; NumHeads = 2
    mov word [es:di+26], 2
    ; HiddenSectors = 2048
    mov dword [es:di+28], 2048
    ; TotalSectors32 = 63488
    mov dword [es:di+32], 63488
    ; DriveNumber = 0x80
    mov byte [es:di+36], 0x80
    ; BootSignature = 0x29
    mov byte [es:di+38], 0x29
    ; VolumeID
    mov dword [es:di+39], 0x12345678
    
    ; VolumeLabel = "MICRODOS   " (11 octets)
    mov si, vol_label
    mov cx, 11
    rep movsb
    
    ; IMPORTANT : On remet DI au début du BPB !
    mov di, 0x7000
    
    ; FileSystem = "FAT16   " (8 octets)
    mov si, fs_type
    mov cx, 8
    rep movsb

    ; Signature 0xAA55 (offset absolu)
    mov byte [es:0x7000+510], 0x55
    mov byte [es:0x7000+511], 0xAA

    ; Écrire le BPB sur le disque
    mov eax, 2048
    mov cx, 1
    mov bx, 0x7000
    push cs
    pop es
    call lba_write
    jc .error

    ; --- 2. Écrire la FAT1 ---
    mov eax, [lba_fat1]
    mov [current_format_lba], eax
    call .write_fat_at_lba

    ; --- 3. Écrire la FAT2 ---
    mov eax, [lba_fat1]
    movzx ebx, word [bpb_sectors_per_fat]
    add eax, ebx
    mov [current_format_lba], eax
    call .write_fat_at_lba

    ; --- 4. Effacer le Root Directory ---
    mov eax, [lba_rootdir]
    mov [current_format_lba], eax
    mov cx, 32
.clear_root:
    push cx
    mov eax, [current_format_lba]
    mov cx, 1
    mov bx, 0x7000
    mov dx, 0x1000
    mov es, dx
    call lba_write
    pop cx              ; <-- FIX : pop cx AVANT le jc
    jc .write_fat_error
    inc dword [current_format_lba]
    loop .clear_root

    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret

.error:
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret

.write_fat_at_lba:
    mov ax, 0x1000
    mov es, ax
    mov di, 0x7000
    mov cx, 256
    xor ax, ax
    rep stosw

    mov ax, 0x1000
    mov es, ax
    mov byte [es:0x7000], 0xF8
    mov byte [es:0x7001], 0xFF
    mov byte [es:0x7002], 0xFF
    mov byte [es:0x7003], 0xFF

    mov cx, 64
.write_fat_sector:
    push cx
    mov eax, [current_format_lba]
    mov cx, 1
    mov bx, 0x7000
    mov dx, 0x1000
    mov es, dx
    call lba_write
    pop cx              ; <-- FIX : pop cx AVANT le jc
    jc .error
    inc dword [current_format_lba]
    loop .write_fat_sector
    
    mov ax, 0x1000
    mov es, ax
    mov di, 0x7000
    mov cx, 256
    xor ax, ax
    rep stosw
    
    clc
    ret
.write_fat_error:
    stc
    ret

current_format_lba dd 0
oem_name db 'MSDOS5.0'
vol_label db 'MICRODOS   '
fs_type db 'FAT16   '