[BITS 16]
[ORG 0x0000]
start:
    push cs
    pop ds
    push cs
    pop es

	cli
	push es
	xor ax, ax
	mov es, ax
	mov word [es:0x84], int21_handler
	mov word [es:0x86], cs
	pop es
	sti

    mov word [current_cluster], 0
	mov byte [current_drive], 1
	mov byte [fat16_ready], 0

    ; --- INITIALISATION DU DISQUE DUR (FAT16) ---
    mov eax, 0
    mov cx, 1
    mov bx, 0x7000
    push cs
    pop es
    call lba_read
    jc .no_hdd

    mov bx, 0x7000
    push cs
    pop es
    call mbr_find_partition
    jc .no_hdd

    mov [saved_part_lba], eax

    mov cx, 1
    mov bx, 0x7200
    push cs
    pop es
    call lba_read
    jc .no_hdd

    mov bx, 0x7200
    push cs
    pop es
    call bpb16_parse
    
    mov eax, [saved_part_lba]
    call fat16_init
    mov byte [fat16_ready], 1
.no_hdd:

    cmp byte [fat16_ready], 1
    je .hdd_ready
    mov byte [current_drive], 0
    mov si, floppy_init_path
    jmp .set_init_path
.hdd_ready:
    mov byte [current_drive], 1
	mov si, init_path
.set_init_path:
	mov di, current_path
	call copy_str

    mov si, msg_welcome
    call print

main_loop:
	mov si, current_path
	call print
    mov si, msg_prompt
    call print

    mov di, buffer
    call read_line

	mov al, [buffer]
	and al, 0xDF
	cmp al, 'A'
	jne .check_c
	cmp byte [buffer+1], ':'
	jne .check_c
	cmp byte [buffer+2], 0
	jne .check_c
	mov byte [current_drive], 0
	mov byte [current_path], 'A'
	mov byte [current_path+1], ':'
	mov byte [current_path+2], '\'
	mov byte [current_path+3], 0
	jmp main_loop

.check_c:
	cmp al, 'C'
	jne .not_a_drive
	cmp byte [buffer+1], ':'
	jne .not_a_drive
	cmp byte [buffer+2], 0
	jne .not_a_drive
	mov byte [current_drive], 1
	mov byte [current_path], 'C'
	mov byte [current_path+1], ':'
	mov byte [current_path+2], '\'
	mov byte [current_path+3], 0
	jmp main_loop

.not_a_drive:
    mov si, buffer
    mov di, cmd_help
    call strcmp
    jc do_help

    mov si, buffer
    mov di, cmd_dir
    call strcmp
    jc do_dir

    mov si, buffer
    mov di, cmd_type
    call strcmp
    jc do_type

    mov si, buffer
    mov di, cmd_ver
    call strcmp
    jc do_ver

    mov si, buffer
    mov di, cmd_cls
    call strcmp
    jc do_cls

    mov si, buffer
	mov di, cmd_reset
	call strcmp
	jc do_reset

    mov si, buffer
    mov di, cmd_echo
    call strcmp
    jc do_echo

    mov si, buffer
    mov di, cmd_pause
    call strcmp
    jc do_pause

    mov si, buffer
    mov di, cmd_date
    call strcmp
    jc do_date

    mov si, buffer
    mov di, cmd_time
    call strcmp
    jc do_time

    mov si, buffer
    mov di, cmd_fatinfo
    call strcmp
    jc do_fatinfo

    mov si, buffer
    mov di, cmd_mkfile
    call strcmp
    jc do_mkfile

    mov si, buffer
    mov di, cmd_copy_name
    call strcmp
    jc do_copy

    mov si, buffer
    mov di, cmd_del
    call strcmp
    jc do_del

    mov si, buffer
    mov di, cmd_ren
    call strcmp
    jc do_ren

    mov si, buffer
    mov di, cmd_cd
    call strcmp
    jc do_cd

    mov si, buffer
    mov di, cmd_mkdir
    call strcmp
    jc do_mkdir

    mov si, buffer
    mov di, cmd_rmdir
    call strcmp
    jc do_rmdir

    mov si, buffer
    mov di, cmd_rebootdos
    call strcmp
    jc do_rebootdos

    mov si, buffer
    mov di, cmd_reboot
    call strcmp
    jc do_reboot

	mov si, buffer
	mov di, cmd_format
	call strcmp
	jc do_format

    mov si, buffer
    call do_exec
    jmp main_loop

do_help:
    mov si, msg_help
    call print
    jmp main_loop

do_cls:
    mov ax, 0x0003
    int 0x10
    jmp main_loop

do_ver:
    mov si, msg_ver
    call print
    jmp main_loop

do_reboot:
    mov si, msg_reboot
    call print
    mov al, 0xFE
    out 0x64, al
    jmp main_loop

do_rebootdos:
    mov si, msg_rebootdos
    call print
    int 0x19
    jmp main_loop

do_reset:
	mov ah, 0x00
	mov dl, 0
	int 0x13
	mov si, msg_reset
	call print
	jmp main_loop

do_echo:
	mov si, buffer
	add si, 4
	cmp byte [si], ' '
	jne .done
	inc si
.print:
	lodsb
	or al, al
	jz .newline
	mov ah, 0x0E
	int 0x10
	jmp .print
.newline:
.done:
	mov ah, 0x0E
	mov al, 13
	int 0x10
	mov al, 10
	int 0x10
	jmp main_loop

do_pause:
	mov si, msg_pause
	call print
	mov ah, 0x00
	int 0x16
	jmp main_loop

do_date:
	mov ah, 0x04
	int 0x1A
	mov si, msg_date_prefix
	call print
	mov al, dl
	call print_bcd
	mov al, '/'
	mov ah, 0x0E
	int 0x10
	mov al, ch
	call print_bcd
	mov al, cl
	call print_bcd
	mov ah, 0x0E
	mov al, 13
	int 0x10
	mov al, 10
	int 0x10
	jmp main_loop

do_time:
	mov ah, 0x02
	int 0x1A
	mov si, msg_time_prefix
	call print
	mov al, ch
	call print_bcd
	mov al, ':'
	mov ah, 0x0E
	int 0x10
	mov al, cl
	call print_bcd
	mov ah, 0x0E
	mov al, 13
	int 0x10
	mov al, 10
	int 0x10
	jmp main_loop

print_bcd:
	push ax
	shr al, 4
	add al, '0'
	mov ah, 0x0E
	int 0x10
	pop ax
	and al, 0x0F
	add al, '0'
	mov ah, 0x0E
	int 0x10
	ret

do_fatinfo:
	call fat_find_free
	push ax
	mov si, msg_fatinfo
	call print
	pop ax
	mov cx, 0
	mov bx, 10
.divide:
	xor dx, dx
	div bx
	push dx
	inc cx
	test ax, ax
	jnz .divide
.print:
	pop dx
	mov al, dl
	add al, '0'
	mov ah, 0x0E
	int 0x10
	loop .print
	mov ah, 0x0E
	mov al, 13
	int 0x10
	mov al, 10
	int 0x10
	jmp main_loop

do_mkfile:
    mov si, buffer
    add si, 7
    cmp byte [si-1], ' '
    jne .usage
    call resolve_path
    jc .error
    mov word [src_size], 0
    cmp byte [current_drive], 0
    je .create_fat12
    mov si, [path_filename]
    call build_fat_name
    xor bx, bx
    call fat16_create_file
    cmp ax, 0xFFFF
    je .error
    jmp .created
.create_fat12:
    call create_file
    cmp ax, 0xFFFF
    je .error
.created:
    mov si, msg_mkfile_ok
    call print
    jmp .exit
.error:
    mov si, msg_mkfile_err
    call print
    jmp .exit
.usage:
    mov si, msg_mkfile_usage
    call print
    jmp .exit
.exit:
	mov ax, [saved_cluster]
	mov [current_cluster], ax
	jmp main_loop

do_format:

    mov byte [format_drive], 0xFF
    cmp byte [buffer+7], 0
    je .confirm
    mov al, [buffer+7]
    and al, 0xDF
    cmp al, 'A'
    je .format_a
    cmp al, 'C'
    je .format_c
    jmp .error
.format_a:
    cmp byte [buffer+8], ':'
    jne .error
    mov byte [format_drive], 0
    jmp .confirm
.format_c:
    cmp byte [buffer+8], ':'
    jne .error
    mov byte [format_drive], 1
.confirm:
	mov si, msg_format_warn
	call print
	mov ah, 0x00
	int 0x16

	mov ah, 0x0E
	mov al, 13
	int 0x10
	mov al, 10
	int 0x10

    cmp byte [format_drive], 0
    je .format_floppy
    cmp byte [format_drive], 1
    jne .use_current
    mov byte [current_drive], 1
.use_current:
    cmp byte [current_drive], 0
    je .format_floppy
	call format_hdd
	jc .error
    cmp byte [current_drive], 1
    jne .formatted
    mov eax, 2048
    call fat16_init
    mov byte [fat16_ready], 1
    mov word [current_cluster], 0
    mov byte [current_path], 'C'
    mov byte [current_path+1], ':'
    mov byte [current_path+2], '\'
    mov byte [current_path+3], 0
    jmp .formatted
.format_floppy:
    mov byte [current_drive], 0
    call format_floppy
    jc .error
    mov word [current_cluster], 0
    mov byte [current_path], 'A'
    mov byte [current_path+1], ':'
    mov byte [current_path+2], '\'
    mov byte [current_path+3], 0
.formatted:
	
	mov si, msg_format_ok
	call print
	jmp main_loop

	.error:

	mov si, msg_format_err
	call print
	jmp main_loop

do_copy:
    call cmd_copy
    jmp main_loop

do_del:
	mov si, buffer
	add si, 4
	cmp byte [si-1], ' '
	jne .usage
	call resolve_path
	jc .error
    cmp byte [current_drive], 0
    je .delete_fat12
    mov si, [path_filename]
    call build_fat_name
    call fat16_del_file
    jmp .check_result
.delete_fat12:
	call del_file
.check_result:
	cmp ax, 0xFFFF
	je .error
	cmp ax, 0xFFFE
	je .protected
	cmp ax, 0xFFFD
	je .is_dir
	mov si, msg_del_ok
	call print
	jmp .exit
.error:
	mov si, msg_del_err
	call print
	jmp .exit
.protected:
	mov si, msg_del_protected
	call print
	jmp .exit
.usage:
	mov si, msg_del_usage
	call print
	jmp .exit
.is_dir:
	mov si, msg_del_dir
	call print
	jmp .exit
.exit:
	mov ax, [saved_cluster]
	mov [current_cluster], ax
	jmp main_loop

do_ren:
	mov si, buffer
	add si, 4
	cmp byte [si-1], ' '
	jne .usage
    cmp byte [current_drive], 0
    je .rename_fat12
    call fat16_ren_file
    jmp .check_result
.rename_fat12:
	call ren_file
.check_result:
	cmp ax, 0xFFFF
	je .error
	cmp ax, 0xFFFE
	je .protected
	mov si, msg_ren_ok
	call print
	jmp .exit
.error:
	mov si, msg_ren_err
	call print
	jmp .exit
.protected:
	mov si, msg_ren_protected
	call print
	jmp .exit
.usage:
	mov si, msg_ren_usage
	call print
	jmp .exit
.exit:
	mov ax, [saved_cluster]
	mov [current_cluster], ax
	jmp main_loop

do_cd:
	mov si, buffer
	add si, 3
	cmp byte [si-1], ' '
	jne .root
	call cd_dir
	cmp ax, 0xFFFF
	je .error
	jmp main_loop
.root:
	mov word [current_cluster], 0
	jmp main_loop
.error:
	mov si, msg_cd_err
	call print
	jmp main_loop

do_mkdir:
	mov si, buffer
	add si, 6
	cmp byte [si-1], ' '
	jne .usage
	call resolve_path
	jc .error
    cmp byte [current_drive], 0
    je .create_fat12
    mov si, [path_filename]
    call build_fat_name
    call fat16_create_dir
    jmp .check_result
.create_fat12:
	call mkdir_dir
.check_result:
	cmp ax, 0xFFFF
	je .error
	mov si, msg_mkdir_ok
	call print
	jmp .exit
.error:
	mov si, msg_mkdir_err
	call print
	jmp .exit
.usage:
	mov si, msg_mkdir_usage
	call print
	jmp .exit
.exit:
	mov ax, [saved_cluster]
	mov [current_cluster], ax
	jmp main_loop

do_rmdir:
	mov si, buffer
	add si, 6
	cmp byte [si-1], ' '
	jne .usage
	call resolve_path
	jc .error
    cmp byte [current_drive], 0
    je .rmdir_fat12
    mov si, [path_filename]
    call build_fat_name
    call fat16_del_dir
    jmp .check_result
.rmdir_fat12:
	call rmdir_dir
.check_result:
	cmp ax, 0xFFFF
	je .error
	mov si, msg_rmdir_ok
	call print
	jmp .exit
.error:
	mov si, msg_rmdir_err
	call print
	jmp .exit
.usage:
	mov si, msg_rmdir_usage
	call print
	jmp .exit
.exit:
    mov ax, [saved_cluster]
    mov [current_cluster], ax
    jmp main_loop

; ---------- DIR ----------
do_dir:
	cmp byte [current_drive], 0
	jne .dir_hdd

    ; --- FAT12 ---
    call load_current_dir
    jc .error_fat12
    mov si, 0x8000
    mov bp, 224
.loop_fat12:
    mov al, [si]
    cmp al, 0
    je .done_fat12
    cmp al, 0xE5
    je .next_fat12
    mov al, [si+11]
    cmp al, 0x0F
    je .next_fat12
    test al, 0x08
    jnz .next_fat12
    test al, 0x02
    jnz .next_fat12
    mov di, si
    mov cx, 8
.print_name_fat12:
    mov al, [di]
    cmp al, ' '
    je .name_done_fat12
    mov ah, 0x0E
    int 0x10
    inc di
    loop .print_name_fat12
.name_done_fat12:
    mov al, [si+8]
    cmp al, ' '
    je .no_ext_fat12
    mov ah, 0x0E
    mov al, '.'
    int 0x10
    mov di, si
    add di, 8
    mov cx, 3
.print_ext_fat12:
    mov al, [di]
    cmp al, ' '
    je .ext_done_fat12
    mov ah, 0x0E
    int 0x10
    inc di
    loop .print_ext_fat12
.ext_done_fat12:
.no_ext_fat12:
	push si
	mov al, [si+11]
	test al, 0X10
	jz .not_dir_fat12
	mov si, msg_dir_tag
	call print
.not_dir_fat12:
	pop si
	mov ah, 0x0E
	mov al, 13
	int 0x10
	mov al, 10
	int 0x10
.next_fat12:
    add si, 32
    dec bp
    jnz .loop_fat12
.done_fat12:
    jmp main_loop
.error_fat12:
    mov si, msg_dir_err
    call print
    jmp main_loop

    ; --- FAT16 ---
.dir_hdd:
    call fat16_list_dir
    jc .error_hdd
    jmp main_loop
.error_hdd:
    mov si, msg_dir_err
    call print
    jmp main_loop

; ---------- TYPE ----------
do_type:
    cmp byte [current_drive], 0
    jne .type_hdd

    ; --- FAT12 ---
    mov si, buffer
    add si, 5
    call resolve_path
    call build_fat_name
    call find_file
    jc .not_found_fat12
    mov al, [si + 11]
    test al, 0x10
    jnz .not_found_fat12
    mov ax, [si + 26]
    mov bx, [si + 28]
    mov [file_size], bx
    add ax, 31
    call lba_to_chs
    mov ax, 0x1000
    mov es, ax
    mov bx, 0xA000
    mov ah, 0x02
    mov al, 1
    int 0x13
    jc .read_err_fat12
    mov si, 0xA000
    mov cx, [file_size]
.print_fat12:
    test cx, cx
    jz .done_fat12
    mov al, [si]
    mov ah, 0x0E
    int 0x10
    inc si
    dec cx
    jmp .print_fat12
.done_fat12:
    mov ax, [saved_cluster]
    mov [current_cluster], ax
    mov ah, 0x0E
    mov al, 13
    int 0x10
    mov al, 10
    int 0x10
    jmp main_loop
.not_found_fat12:
    mov ax, [saved_cluster]
    mov [current_cluster], ax
    mov si, msg_not_found
    call print
    jmp main_loop
.read_err_fat12:
    mov ax, [saved_cluster]
    mov [current_cluster], ax
    mov si, msg_dir_err
    call print
    jmp main_loop

    ; --- FAT16 ---
.type_hdd:
    mov si, buffer
    add si, 5
    call resolve_path
    jc .not_found_hdd
    mov si, [path_filename]
    call build_fat_name
    call fat16_find_file
    jc .not_found_hdd
    mov ax, [si + 26]
    mov bx, [si + 28]
    mov [file_size], bx
    call fat16_read_file
    jc .read_err_hdd
    mov ax, 0x1000
    mov es, ax
    mov si, 0xA000
    mov cx, [file_size]
.print_hdd:
    test cx, cx
    jz .done_hdd
    mov al, [es:si]
    mov ah, 0x0E
    int 0x10
    inc si
    dec cx
    jmp .print_hdd
.done_hdd:
    mov ax, [saved_cluster]
    mov [current_cluster], ax
    mov ah, 0x0E
    mov al, 13
    int 0x10
    mov al, 10
    int 0x10
    jmp main_loop
.not_found_hdd:
    mov ax, [saved_cluster]
    mov [current_cluster], ax
    mov si, msg_not_found
    call print
    jmp main_loop
.read_err_hdd:
    mov ax, [saved_cluster]
    mov [current_cluster], ax
    mov si, msg_dir_err
    call print
    jmp main_loop

fat_find_free:
	push bx
	push cx
	mov ax, 2
.loop:
	push ax
	call fat_get
	pop bx
	test ax, ax
	jz .found
	mov ax, bx
	inc ax
	cmp ax, 2848
	jb .loop
	mov ax, 0xFFFF
	jmp .done
.found:
	mov ax, bx
.done:
	pop cx
	pop bx
	ret

free_cluster_chain:
	push ax
	push bx
	push cx
	mov bx, ax
.loop:
	cmp bx, 0xFF8
	jae .done
	cmp bx, 2
	jb .done
	mov [free_temp], bx
	mov ax, bx
	call fat_get
	mov cx, ax
	mov ax, [free_temp]
	xor dx, dx
	call fat_set
	mov bx, cx
	jmp .loop
.done:
	pop cx
	pop bx
	pop ax
	ret

find_free_handle:
	push bx
	push di
	mov di, open_files
	mov cx, 5
	mov bx, 5
.loop:
	cmp byte [di+6], 0
	je .found
	add di, 16
	inc bx
	loop .loop
	mov ax, 0xFFFF
	jmp .done
.found:
	mov ax, bx
.done:
	pop di
	pop bx
	ret

find_handle:
	cmp ax, 5
	jb .error
	cmp ax, 10
	jae .error
	sub ax, 5
	shl ax, 4
	mov di, open_files
	add di, ax
	clc
	ret
.error:
	stc
	ret

copy_str:
	lodsb
	stosb
	test al, al
	jnz copy_str
	ret

append_path:
	push ax
	push di
	push si
	mov di, current_path
.find_end:
	cmp byte [di], 0
	je .add
	inc di
	jmp .find_end
.add:
	cmp di, current_path
	je .no_backslash
	cmp byte [di-1], '\'
	je .copy
	mov byte [di], '\'
	inc di
	jmp .copy
.no_backslash:
	mov byte [di], '\'
	inc di
.copy:
	lodsb
	test al, al
	jz .copy_done
	cmp al, 'a'
	jb .store
	cmp al, 'z'
	ja .store
	sub al, 32
.store:
	stosb
	jmp .copy
.copy_done:
	pop si
	pop di
	pop ax
	ret

chop_path:
	push ax
	push di
	mov di, current_path
	cmp byte [di+3], 0
	je .done
.find_end:
	cmp byte [di], 0
	je .go_back
	inc di
	jmp .find_end
.go_back:
.back:
	cmp di, current_path
	jbe .done
	dec di
	cmp byte [di], '\'
	je .truncate
	jmp .back
.truncate:
	mov ax, di
	sub ax, current_path
	cmp ax, 2
	je .keep_root
	mov byte [di], 0
	jmp .done
.keep_root:
	mov byte [di+1], 0
.done:
	pop di
	pop ax
	ret

resolve_path:
	push ax
	push bx
    push cx
    push si
	push di
	push es
	mov ax, [current_cluster]
	mov [saved_cluster], ax
	mov [path_filename], si
    push cs
    pop es
    mov di, resolve_saved_path
    mov si, current_path
    mov cx, 64
    rep movsb
    mov si, [path_filename]
.loop:
	mov di, si
.scan:
	mov al, [di]
	test al, al
	jz .done
	cmp al, '\'
	je .found_backslash
	inc di
	jmp .scan
.found_backslash:
	mov byte [di], 0
	inc di
	push di
	call cd_dir
	pop si
	jc .error
	mov [path_filename], si
	jmp .loop
.done:
    mov si, resolve_saved_path
    mov di, current_path
    mov cx, 64
    rep movsb
	clc
	jmp .exit
.error:
	mov ax, [saved_cluster]
	mov [current_cluster], ax
    mov si, resolve_saved_path
    mov di, current_path
    mov cx, 64
    rep movsb
	stc
.exit:
	pop es
	pop di
    pop si
    pop cx
	pop bx
	pop ax
	ret

save_current_dir:
	push ax
	push bx
	push cx
	push bx
	push es
	mov ax, 0x1000
	mov es, ax
	mov bx, 0x8000
	mov ax, [current_cluster]
	test ax, ax
	jz .root
	add ax, 31
	call lba_to_chs
	mov al, 1
	jmp .do_write
.root:
	mov ch, 0
	mov cl, 2
	mov dh, 1
	mov al, 14
.do_write:
	mov dl, 0
	mov ah, 0x03
	int 0x13
	push ax
	xor ax, ax
	mov dl, 0
	int 0x13
	pop ax
	pop es
	pop dx
	pop cx
	pop dx
	pop ax
	ret

mkdir_dir:
	push bx
	push cx
	push dx
	push si
	push di
	call build_fat_name
	call fat_find_free
	cmp ax, 0xFFFF
	je .error
	mov [cluster_temp], ax
	mov dx, 0xFFF
	call fat_set
	call find_free_entry
	jc .error
	mov [dir_entry_addr], si
	mov di, si
	mov si, fatname
	mov ax, [cluster_temp]
	mov dx, ax
	xor bx, bx
	call write_dir_entry
	mov di, [dir_entry_addr]
	mov byte [di+11], 0x10
	call init_new_dir
	call save_current_dir
	call save_current_dir
	xor ax, ax
	jmp .done
.error:
	mov ax, 0xFFFF
.done:
	pop di
	pop si
	pop dx
	pop cx
	pop bx
	ret

init_new_dir:
	push ax
	push bx
	push cx
	push dx
	push di
	push es
	mov ax, 0x1000
	mov es, ax
	mov di, 0xA000
	mov al, '.'
	stosb
	mov cx, 10
	mov al, '.'
	stosb
	mov cx, 10
.dot_pad:
	mov al, ' '
	stosb
	loop .dot_pad
	mov al, 0x10
	stosb
	mov cx, 10
.dot_reserved:
	xor al, al
	stosb
	loop .dot_reserved
	xor ax, ax
	stosw
	stosw
	mov ax, [cluster_temp]
	stosw
	xor ax, ax
	stosw
	stosw
	mov al, '.'
	stosb
	mov al, '.'
	stosb
	mov cx, 9
.dotdot_pad:
	mov al, ' '
	stosb
	loop .dotdot_pad
	mov al, 0x10
	stosb
	mov cx, 10
.dotdot_reserved:
	xor al, al
	stosb
	loop .dotdot_reserved
	xor ax, ax
	stosw
	stosw
	mov ax, [current_cluster]
	stosw
	xor ax, ax
	stosw
	stosw
	mov ax, [cluster_temp]
	add ax, 31
	call lba_to_chs
	mov ax, 0x1000
	mov es, ax
	mov bx, 0xA000
	mov dl, 0
	mov ah, 0x03
	mov al, 1
	int 0x13
	push ax
	xor ax, ax
	mov dl, 0
	int 0x13
	pop ax
	pop es
	pop di
	pop dx
	pop cx
	pop bx
	pop ax
	ret

rmdir_dir:
	push bx
	push cx
	push dx
	push si
	push di
	push es
	call build_fat_name
	call find_file
	jc .error
	mov al, [si+11]
	test al, 0x10
	jz .error
	mov ax, [si + 26]
	mov [free_temp], ax
	add ax, 31
	call lba_to_chs
	mov ax, 0x1000
	mov es, ax
	mov bx, 0xA000
	mov dl, 0
	mov ah, 0x02
	mov al, 1
	int 0x13
	jc .error
	mov di, 0xA000
	mov cx, 16
.check_loop:
	mov al, [di]
	cmp al, 0
	je .empty
	cmp al, 0xE5
	je .skip
	cmp al, '.'
	je .skip
	jmp .error
.skip:
	add di, 32
	dec cx
	jnz .check_loop
	jmp .error
.empty:
	mov byte [si], 0xE5
	call save_current_dir
	mov ax, [free_temp]
	call free_cluster_chain
	xor ax, ax
	jmp .done
.error:
	mov ax, 0xFFFF
.done:
	pop es
	pop di
	pop si
	pop dx
	pop cx
	pop bx
	ret

cd_dir:
	push bx
	push cx
	push dx
	push si
	push di
	cmp byte [si], '.'
	jne .normal
	cmp byte [si+1], '.'
	jne .normal
    cmp byte [current_drive], 0
    je .parent_fat12
    cmp word [current_cluster], 0
    je .parent_root
    mov ax, [current_cluster]
    movzx eax, ax
    sub eax, 2
    movzx ebx, byte [bpb_sectors_per_cluster]
    imul eax, ebx
    add eax, [lba_data]
    mov cx, 1
    mov bx, 0x6000
    push cs
    pop es
    call lba_read
    jc .error
    mov ax, [0x6000+32+26]
    mov [current_cluster], ax
    call chop_path
    xor ax, ax
    jmp .done
.parent_root:
    mov word [current_cluster], 0
    call chop_path
    xor ax, ax
    jmp .done
.parent_fat12:
    mov word [current_cluster], 0
	call chop_path
	xor ax, ax
	jmp .done
.normal:
	mov [path_filename], si
	call build_fat_name
    cmp byte [current_drive], 0
    je .find_fat12
    call fat16_find_file
    jmp .check_directory
.find_fat12:
	call find_file
.check_directory:
	jc .error
	mov al, [si + 11]
	test al, 0x10
	jz .error
	mov ax, [si + 26]
	mov [current_cluster], ax
	mov si, [path_filename]
	call append_path
	xor ax, ax
	jmp .done
.error:
	mov ax, 0xFFFF
.done:
	pop di
	pop si
	pop dx
	pop cx
	pop bx
	ret

find_free_entry:
	push ax
	push bx
	push cx
	push dx
	push es
	call load_current_dir
	jc .error
	mov si, 0x8000
	mov bp, 224
.loop:
	mov al, [si]
	cmp al, 0
	je .found
	cmp al, 0xE5
	je .found
	add si, 32
	dec bp
	jnz .loop
	stc
	jmp .done
.found:
	clc
	jmp .done
.error:
	stc
.done:
	pop es
	pop dx
	pop cx
	pop bx
	pop ax
	ret

write_cluster:
	push ax
	push bx
	push cx
	push dx
	push es
	add ax, 31
	call lba_to_chs
	push ds
	pop es
	mov bx, 0xA000
	mov ah, 0x03
	mov al, 1
	mov dl, 0
	int 0x13
	push ax
	xor ax, ax
	mov dl, 0
	int 0x13
	pop ax
	pop es
	pop dx
	pop cx
	pop bx
	pop ax
	ret

write_file:
	push ax
	push bx
	push cx
	push dx
	push si
	push di
	push es
	mov [free_temp], ax
	mov [src_size], bx
	mov si, 0xA000
	cmp word [src_size], 0
	je .empty_file
.write_loop:
	mov ax, [free_temp]
	add ax, 31
	call lba_to_chs
	push si
	mov ax, 0x1000
	mov es, ax
	mov bx, si
	mov dl, 0
	mov ah, 0x03
	mov al, 1
	int 0x13
	pop si
	jc .error
	add si, 512
	mov ax, [src_size]
	sub ax, 512
	jbe .last_cluster
	mov [src_size], ax
	call fat_find_free
	cmp ax, 0xFFFF
	je .error
	mov bx, ax
	mov dx, bx
	mov ax, [free_temp]
	call fat_set
	mov ax, dx
	mov dx, 0xFFF
	call fat_set
	mov [free_temp], bx
	jmp .write_loop
.last_cluster:
	mov ax, [free_temp]
	mov dx, 0xFFF
	call fat_set
	xor ax, ax
	jmp .done
.error:
	mov ax, 0xFFFF
.done:
	pop es
	pop di
	pop si
	pop dx
	pop cx
	pop bx
	pop ax
	ret
.empty_file:
	xor ax, ax
	jmp .done

parse_two_args:
	push ax
	push si
	mov si, buffer
	add si, [parse_skip]
	mov [arg1_ptr], si
.skip1:
	lodsb
	or al, al
	jz .error
	cmp al, ' '
	jne .skip1
	dec si
	mov byte [si], 0
	inc si
	mov [arg2_ptr], si
	clc
	jmp .done
.error:
	stc
.done:
	pop si
	pop ax
	ret

copy_file:
	push bx
	push cx
	push dx
	push si
	push di
	mov word [parse_skip], 5
	call parse_two_args
	jc .error
	mov si, [arg1_ptr]
	call resolve_path
	jc .error
	mov si, [path_filename]
	mov [arg1_ptr], si
	call read_file
	cmp ax, 0xFFFF
	je .error
	mov ax, [saved_cluster]
	mov [current_cluster], ax
	mov si, [arg2_ptr]
	call resolve_path
	jc .error
	mov si, [path_filename]
	mov [arg2_ptr], si
	call create_file
	cmp ax, 0xFFFF
	je .error
	mov bx, [src_size]
	call write_file
	cmp ax, 0xFFFF
	je .error
	call write_cluster
	xor ax, ax
	jmp .done
.error:
	mov ax, 0xFFFF
.done:
	mov ax, [saved_cluster]
	mov [current_cluster], ax
	pop di
	pop si
	pop dx
	pop cx
	pop bx
	ret

del_file:
	push bx
	push cx
	push dx
	push si
	push di
	push es
	call build_fat_name
	call find_file
	jc .error
	mov al, [si+11]
	test al, 0x10
	jnz .is_dir
	mov al, [si + 11]
	test al, 0x01
	jnz .protected
	test al, 0x04
	jnz .protected
	mov ax, [si + 26]
	call free_cluster_chain
	mov byte [si], 0xE5
	mov ax, 0x1000
	mov es, ax
	mov bx, 0x8000
	mov ch, 0
	mov cl, 2
	mov dh, 1
	mov dl, 0
	mov ah, 0x03
	mov al, 14
	int 0x13
	jc .error
	push ax
	xor ax, ax
	mov dl, 0
	int 0x13
	pop ax
	xor ax, ax
	jmp .done
.is_dir:
	mov ax, 0xFFFD
	jmp .done
.protected:
	mov ax, 0xFFFE
	jmp .done
.error:
	mov ax, 0xFFFF
.done:
	pop es
	pop di
	pop si
	pop dx
	pop cx
	pop bx
	ret

ren_file:
	push bx
	push cx
	push dx
	push si
	push di
	push es
	mov word [parse_skip], 4
	call parse_two_args
	jc .error
	mov si, [arg1_ptr]
	call build_fat_name
	call find_file
	jc .error
	mov al, [si + 11]
	test al, 0x01
	jnz .protected
	test al, 0x04
	jnz .protected
	mov di, si
	mov si, [arg2_ptr]
	call build_fat_name
	mov si, fatname
	mov cx, 11
.copy_name:
	lodsb
	stosb
	loop .copy_name
	mov ax, 0x1000
	mov es, ax
	mov bx, 0x8000
	mov ch, 0
	mov cl, 2
	mov dh, 1
	mov dl, 0
	mov ah, 0x03
	mov al, 14
	int 0x13
	jc .error
	push ax
	xor ax, ax
	mov dl, 0
	int 0x13
	pop ax
	xor ax, ax
	jmp .done
.protected:
	mov ax, 0xFFFE
	jmp .done
.error:
	mov ax, 0xFFFF
.done:
	pop es
	pop di
	pop si
	pop dx
	pop cx
	pop bx
	ret

write_dir_entry:
	push ax
	push bx
	push cx
	push dx
	push si
	push di
	push es
	mov cx, 11
.copy_name:
	lodsb
	stosb
	loop .copy_name
	xor al, al
	stosb
	mov cx, 10
.reserved:
	xor al, al
	stosb
	loop .reserved
	xor ax, ax
	stosw
	stosw
	mov ax, dx
	stosw
	mov ax, bx
	stosw
	xor ax, ax
	stosw
	call save_current_dir
	pop es
	pop di
	pop si
	pop dx
	pop cx
	pop bx
	pop ax
	ret

create_file:
	push bx
	push cx
	push dx
	push si
	push di
	call build_fat_name
	cmp word [src_size], 0
	je .empty_file
	call fat_find_free
	cmp ax, 0xFFFF
	je .error
	mov [cluster_temp], ax
	mov dx, 0xFFF
	call fat_set
	jc .error
.find_entry:
	call find_free_entry
	jc .error
	mov di, si
	mov si, fatname
	mov ax, [cluster_temp]
	mov dx, ax
	mov bx, [src_size]
	call write_dir_entry
	mov ax, [cluster_temp]
	pop di
	pop si
	pop dx
	pop cx
	pop bx
	ret
.empty_file:
	mov word [cluster_temp], 0
	jmp .find_entry
.error:
	mov ax, 0xFFFF
	pop di
	pop si
	pop dx
	pop cx
	pop bx
	ret

print:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp print
.done:
    ret

do_exec:
	push ds
	push es
    mov ax, [current_cluster]
    mov [exec_saved_cluster], ax
    mov al, [current_drive]
    mov [exec_saved_drive], al
    mov si, current_path
    mov di, exec_saved_path
    mov cx, 64
    rep movsb

    mov si, buffer
    cmp byte [si+1], ':'
    jne .resolve_exec_path
    mov al, [si]
    and al, 0xDF
    cmp al, 'C'
    je .exec_c_prefix
    cmp al, 'A'
    jne .not_found
    mov byte [current_drive], 0
    mov word [current_cluster], 0
    mov byte [current_path], 'A'
    mov byte [current_path+1], ':'
    mov byte [current_path+2], '\'
    mov byte [current_path+3], 0
    add si, 2
    jmp .strip_exec_root
.exec_c_prefix:
    mov byte [current_drive], 1
    mov word [current_cluster], 0
    mov byte [current_path], 'C'
    mov byte [current_path+1], ':'
    mov byte [current_path+2], '\'
    mov byte [current_path+3], 0
    add si, 2
.strip_exec_root:
    cmp byte [si], '\'
    jne .resolve_exec_path
    inc si
.resolve_exec_path:
    call resolve_path
    jc .not_found
    mov si, [path_filename]
    call build_exe_name
    cmp byte [current_drive], 0
    jne .find_exe_fat16
    call find_file
    jc .try_com
    jmp .found_exe
.find_exe_fat16:
    call fat16_find_file
    jc .try_com
    jmp .found_exe
.try_com:
    mov si, [path_filename]
    cmp byte [current_drive], 0
    jne .exec_fat16
    call build_com_name
    call find_file
    jc .not_found
    jmp .found_fat12
.exec_fat16:
    mov si, [path_filename]
    call build_com_name
    call fat16_find_file
    jc .not_found
    mov ax, [si+26]
    mov bx, [si+28]
    mov [file_size], bx
    call fat16_read_file
    jc .read_err
    push ds
    push es
    mov ax, 0x1000
    mov ds, ax
    mov si, 0xA000
    mov ax, 0x2000
    mov es, ax
    mov di, 0x0100
    mov cx, [cs:file_size]
    rep movsb
    pop es
    pop ds
    jmp .restore_exec_state
.found_fat12:
	mov bx, [si + 28]
	mov [file_size], bx
	mov ax, [si + 26]
	mov [free_temp], ax
	mov di, 0x0100
.read_loop:
	mov ax, [free_temp]
	cmp ax, 0xFF8
	jae .read_done
	cmp ax, 2
	jb .read_done
	add ax, 31
	call lba_to_chs
	push di
	mov ax, 0x2000
	mov es, ax
	mov bx, di
	mov dl, 0
	mov ah, 0x02
	mov al, 1
	int 0x13
	pop di
	jc .read_err
	push ax
	push dx
	xor ax, ax
	mov dl, 0
	int 0x13
	pop dx
	pop ax
	add di, 512
	mov ax, [free_temp]
	call fat_get
	mov [free_temp], ax
	jmp .read_loop
.read_done:
.run_com:
    mov ax, [exec_saved_cluster]
    mov [current_cluster], ax
    mov al, [exec_saved_drive]
    mov [current_drive], al
    mov si, exec_saved_path
    mov di, current_path
    mov cx, 64
    rep movsb
	cli
	mov ax, 0x2000
	mov ss, ax
	mov sp, 0xFFFC
	push cs
	push .ret_from_com
	sti
	mov ax, 0x2000
	mov ds, ax
	mov es, ax
	jmp 0x2000:0x100
.found_exe:
	call exec_exe
.restore_exec_state:
	mov ax, [exec_saved_cluster]
	mov [current_cluster], ax
	mov al, [exec_saved_drive]
	mov [current_drive], al
	mov si, exec_saved_path
	mov di, current_path
	mov cx, 64
	rep movsb
	pop es
	pop ds
	ret
.ret_from_com:
	pop es
	pop ds
	ret
.not_found:
    mov ax, [exec_saved_cluster]
    mov [current_cluster], ax
    mov al, [exec_saved_drive]
    mov [current_drive], al
    mov si, exec_saved_path
    mov di, current_path
    mov cx, 64
    rep movsb
	mov si, msg_unknow
	call print
	pop es
	pop ds
	ret
.read_err:
    mov ax, [exec_saved_cluster]
    mov [current_cluster], ax
    mov al, [exec_saved_drive]
    mov [current_drive], al
    mov si, exec_saved_path
    mov di, current_path
    mov cx, 64
    rep movsb
	mov si, msg_dir_err
	call print
	pop es
	pop ds
	ret

build_com_name:
    call build_fat_name
    mov byte [fatname+8], 'C'
    mov byte [fatname+9], 'O'
    mov byte [fatname+10], 'M'
.done:
    ret

build_exe_name:
    call build_fat_name
    mov byte [fatname+8], 'E'
    mov byte [fatname+9], 'X'
    mov byte [fatname+10], 'E'
.done:
    ret

load_exe_source:
    push bx
    push cx
    push dx
    push si
    push di
    push es

    mov ax, [si+28]
    mov [src_size], ax
    test ax, ax
    jz .error
    mov cx, ax
    shr ax, 9
    and cx, 0x01FF
    jz .sector_count_ready
    inc ax
.sector_count_ready:
    mov [exe_source_sectors], ax
    mov ax, [si+26]
    cmp ax, 2
    jb .error
    mov [exe_source_cluster], ax
    mov word [exe_source_offset], 0
    mov byte [exe_source_sector_index], 0

.read_sector:
    mov ax, [exe_source_cluster]
    cmp byte [current_drive], 0
    jne .read_fat16

    add ax, 31
    xor bx, bx
    mov bl, [exe_source_sector_index]
    add ax, bx
    call lba_to_chs
    mov ax, 0x3000
    mov es, ax
    mov bx, [exe_source_offset]
    mov dl, 0
    mov ah, 0x02
    mov al, 1
    int 0x13
    jc .error
    jmp .sector_read

.read_fat16:
    mov ax, 0x3000
    mov es, ax
    movzx eax, word [exe_source_cluster]
    sub eax, 2
    movzx ebx, byte [bpb_sectors_per_cluster]
    imul eax, ebx
    add eax, [lba_data]
    movzx ebx, byte [exe_source_sector_index]
    add eax, ebx
    mov cx, 1
    mov bx, [exe_source_offset]
    call lba_read
    jc .error

.sector_read:
    add word [exe_source_offset], 512
    dec word [exe_source_sectors]
    jz .done

    cmp byte [current_drive], 0
    jne .advance_fat16
    mov ax, [exe_source_cluster]
    call fat_get
    cmp ax, 2
    jb .error
    cmp ax, 0x0FF8
    jae .error
    mov [exe_source_cluster], ax
    jmp .read_sector

.advance_fat16:
    inc byte [exe_source_sector_index]
    mov al, [exe_source_sector_index]
    cmp al, [bpb_sectors_per_cluster]
    jb .read_sector
    mov byte [exe_source_sector_index], 0
    mov ax, [exe_source_cluster]
    call fat16_get
    jc .error
    cmp ax, 2
    jb .error
    cmp ax, 0xFFF0
    jae .error
    mov [exe_source_cluster], ax
    jmp .read_sector

.done:
    xor ax, ax
    clc
    jmp .exit

.error:
    mov ax, 0xFFFF
    stc
.exit:
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    ret

dos_read_data_sector:
    push ax
    push bx
    push cx
    push dx
    push es
    cmp byte [dos_io_drive], 0
    jne .read_fat16
    add ax, 31
    add ax, [dos_io_sector_index]
    call lba_to_chs
    mov ax, 0x1000
    mov es, ax
    mov bx, 0xA000
    mov dl, 0
    mov ah, 0x02
    mov al, 1
    int 0x13
    jc .read_error
    jmp .read_done
.read_fat16:
    movzx eax, ax
    sub eax, 2
    movzx ebx, byte [bpb_sectors_per_cluster]
    imul eax, ebx
    add eax, [lba_data]
    movzx ebx, word [dos_io_sector_index]
    add eax, ebx
    push eax
    mov ax, 0x1000
    mov es, ax
    pop eax
    mov cx, 1
    mov bx, 0xA000
    call lba_read
    jc .read_error
.read_done:
    pop es
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret
.read_error:
    pop es
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret

dos_write_data_sector:
    push ax
    push bx
    push cx
    push dx
    push es
    cmp byte [dos_io_drive], 0
    jne .write_fat16
    add ax, 31
    add ax, [dos_io_sector_index]
    call lba_to_chs
    mov ax, 0x1000
    mov es, ax
    mov bx, 0xA000
    mov dl, 0
    mov ah, 0x03
    mov al, 1
    int 0x13
    jc .write_error
    jmp .write_done
.write_fat16:
    movzx eax, ax
    sub eax, 2
    movzx ebx, byte [bpb_sectors_per_cluster]
    imul eax, ebx
    add eax, [lba_data]
    movzx ebx, word [dos_io_sector_index]
    add eax, ebx
    push eax
    mov ax, 0x1000
    mov es, ax
    pop eax
    mov cx, 1
    mov bx, 0xA000
    call lba_write
    jc .write_error
.write_done:
    pop es
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret
.write_error:
    pop es
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret

dos_advance_data_cluster:
    cmp byte [dos_io_drive], 0
    jne .advance_fat16
    mov ax, [dos_io_cluster]
    call fat_get
    cmp ax, 2
    jb .invalid
    cmp ax, 0x0FF8
    jae .invalid
    mov [dos_io_cluster], ax
    mov word [dos_io_sector_index], 0
    clc
    ret
.advance_fat16:
    inc word [dos_io_sector_index]
    mov al, [dos_io_sector_index]
    cmp al, [bpb_sectors_per_cluster]
    jb .same_cluster
    mov word [dos_io_sector_index], 0
    mov ax, [dos_io_cluster]
    call fat16_get
    jc .invalid
    cmp ax, 2
    jb .invalid
    cmp ax, 0xFFF0
    jae .invalid
    mov [dos_io_cluster], ax
.same_cluster:
    clc
    ret
.invalid:
    stc
    ret

dos_update_file_size:
    mov di, [dos_io_handle]
    cmp byte [di+7], 0
    jne .fat16
    mov ax, [dos_io_count]
    mov [di+4], ax
    mov si, [di+2]
    call save_current_dir
    clc
    ret

.fat16:
    mov ax, 0x1000
    mov es, ax
    mov bx, 0xA000
    mov eax, [di+8]
    mov cx, 1
    call lba_read
    jc .error

    mov di, [dos_io_handle]
    mov bx, [di+12]
    add bx, 28
    mov ax, [dos_io_count]
    mov [es:bx], ax
    mov word [es:bx+2], 0

    mov di, [dos_io_handle]
    mov ax, 0x1000
    mov es, ax
    mov bx, 0xA000
    mov eax, [di+8]
    mov cx, 1
    call lba_write
    jc .error

    mov di, [dos_io_handle]
    mov ax, [dos_io_count]
    mov [di+4], ax
    clc
    ret

.error:
    stc
    ret

read_line:
    mov cx, 0
.next:
    mov ah, 0x00
    int 0x16
    cmp al, 13
    je .done
    cmp al, 8
    je .backspace
    cmp cx, 254
    jae .next
    stosb
    inc cx
    mov ah, 0x0E
    int 0x10
    jmp .next
.backspace:
    test cx, cx
    jz .next
    dec di
    dec cx
    mov ah, 0x0E
    mov al, 8
    int 0x10
    mov al, ' '
    int 0x10
    mov al, 8
    int 0x10
    jmp .next
.done:
    xor al, al
    stosb
    mov ah, 0x0E
    mov al, 13
    int 0x10
    mov al, 10
    int 0x10
    ret

strcmp:
push si
push di
.loop:
	mov al, [di]
	test al, al
	jz .cmd_end
	mov bl, [si]
	cmp al, bl
	jne .no_match
	inc si
	inc di
	jmp .loop
.cmd_end:
	mov al, [si]
	cmp al, 0
	je .match
	cmp al, ' '
	je .match
	jmp .no_match
.match:
	pop di
	pop si
	stc
	ret
.no_match:
	pop di
	pop si
	clc
	ret

int21_handler:
	sti
	push bx
	push cx
	push si
	push dx
	push di
	push ds
	push es
	push bp
	mov [cs:saved_user_ds], ds
	push cs
	pop ds
	cmp ah, 0x09
	je .ah09
	cmp ah, 0x02
	je .ah02
	cmp ah, 0x06
	je dos_ah06
	cmp ah, 0x07
	je dos_ah07
	cmp ah, 0x4C
	je memory_ah4c
	cmp ah, 0x30
	je .ah30
	cmp ah, 0x0B
	je .ah0b
	cmp ah, 0x01
	je .ah01
	cmp ah, 0x08
	je .ah08
	cmp ah, 0x2C
	je .ah2c
	cmp ah, 0x00
	je .ah00
	cmp ah, 0x0F
	je .ah0f
	cmp ah, 0x25
	je dos_ah25
	cmp ah, 0x35
	je .ah35
	cmp ah, 0x3C
	je .ah3c
	cmp ah, 0x3D
	je .ah3d
	cmp ah, 0x3E
	je .ah3e
	cmp ah, 0x3F
	je dos_ah3f
	cmp ah, 0x40
	je dos_ah40
	cmp ah, 0x48
	je memory_ah48
	cmp ah, 0x49
	je memory_ah49
	cmp ah, 0x4A
	je memory_ah4a
	jmp .done
.ah09:
	mov ax, [saved_user_ds]
	mov es, ax
	mov si, dx
.loop09:
	mov al, [es:si]
	inc si
	cmp al, '$'
	je .done
	mov ah, 0x0E
	int 0x10
	jmp .loop09
.ah02:
	mov al,dl
	mov ah, 0x0E
	int 0x10
	jmp .done
.ah30:
	mov al, 3
	mov ah, 30
	jmp .done
.ah0b:
	mov ah, 0x01
	int 0x16
	jz .nokey
	mov al, 0xFF
	jmp .done
.nokey:
	xor al, al
	jmp .done
.ah01:
	mov ah, 0x00
	int 0x16
	mov ah, 0x0E
	int 0x10
	jmp .done
.ah08:
	mov ah, 0x00
	int 0x16
	jmp .done
.ah2c:
	mov ah, 0x02
	int 0x1A
	jmp .done
.ah00:
	jmp memory_ah4c
.ah0f:
	xor al, al
	jmp .done
.ah35:
	push ds
	mov bl, al
	xor bh, bh
	shl bx, 1
	shl bx, 1
	xor ax, ax
	mov ds, ax
	mov ax, [bx]
	mov bx, [bx+2]
	mov es, bx
	mov bx, ax
	pop ds
	jmp .done
.ah3c:
	mov si, dx
	mov word [src_size], 0
	call create_file
	cmp ax, 0xFFFF
	je .error3c
	clc
	jmp .done
.error3c:
	stc
	jmp .done
.ah3d:
	mov si, dx
	call build_fat_name
	cmp byte [current_drive], 0
	jne .open_fat16
	call find_file
	jc .error3d
	jmp .found_file3d
.open_fat16:
	call fat16_find_file
	jc .error3d
.found_file3d:
	mov al, [si + 11]
	test al, 0x10
	jnz .error3d
	call find_free_handle
	cmp ax, 0xFFFF
	je .error3d
	push ax
	call find_handle
	pop ax
	jc .error3d
	mov bx, [si + 26]
	mov [di], bx
	mov word [di+2], si
	mov bx, [si + 28]
	mov [di+4], bx
	mov byte [di+6], 1
	mov al, [current_drive]
	mov [di+7], al
	mov dword [di+8], 0
	mov word [di+12], 0
	mov word [di+14], 0
	cmp byte [current_drive], 0
	je .handle_ready
	push ax
	mov eax, [fat16_found_dir_lba]
	mov [di+8], eax
	mov ax, [fat16_found_dir_offset]
	mov [di+12], ax
	pop ax
.handle_ready:
	clc
	jmp .done
.error3d:
	mov ax, 0x0004
	stc
	jmp .done
.ah3e:
	mov ax, bx
	call find_handle
	jc .error3e
	mov byte [di+6], 0
	xor ax, ax
	clc
	jmp .done
.error3e:
	mov ax, 0x0006
	stc
	jmp .done
.done:
	pop bp
	pop es
	pop ds
	pop di
	pop dx
	pop si
	pop cx
	pop bx
	iret

; ---------- Donnees ----------
msg_welcome  db 'MicroDOS v1.1.0 Disk Edition - Shell', 13, 10
             db 'tapez "help" pour la liste de commandes.', 13, 10, 10, 0
msg_prompt   db '>', 0
msg_help     db 'Liste des Commandes', 13, 10
             db ' help - cette aide', 13, 10
             db ' dir - liste les fichiers', 13, 10
             db ' type - affiche un fichier', 13, 10
             db ' <programme> - execute un fichier .COM ou .EXE', 13, 10
             db ' reset - reinitialise la disquette', 13, 10
             db ' cls - efface l ecran', 13, 10
             db ' ver - version', 13, 10
             db ' reboot - redemarre le PC', 13, 10
             db ' rebootdos - redemarre MicroDOS', 13, 10
             db ' echo <txt> - affiche un texte', 13, 10
             db ' pause - attend une touche', 13, 10
             db ' date - affiche la date', 13, 10
             db ' time - affiche l heure', 13, 10
	     db ' mkfile <nom> - cree un fichier', 13, 10
	     db ' copy <src> <dst> - copie un fichier', 13, 10
	     db ' del - Supprime un fichier', 13, 10
	     db ' ren <ANCIEN NOM> <NOUVEAU NOM> - renomme un fichier', 13, 10
             db ' cd - Change de Repectoire', 13, 10
             db ' mkdir <nom> - cree un dossier', 13, 10
             db ' rmdir <nom> - Suprimme un dossier', 13, 10, 0
msg_unknow   db 'Commande inconnue. Tapez "help".', 13, 10, 0
msg_ver      db 'MicroDOS v1.1.0 Disk Edition', 13, 10, 0
msg_reboot   db 'Redemarrage en cours...', 13, 10, 0
msg_rebootdos db 'Redemarrage de MicroDOS...', 13, 10, 0
msg_dir_err  db 'Erreur lecture disque.', 13, 10, 0
msg_not_found db 'Fichier non trouve.', 13, 10, 0
msg_reset db 'Disque reinitialise', 13, 10, 0
msg_pause db 'Appuyez sur une touche...', 13, 10, 0
msg_date_prefix db 'Date :', 0
msg_time_prefix db 'Heure :', 0
msg_fatinfo db 'Cluster libre : ', 0
msg_mkfile_ok db 'Fichier cree', 13, 10, 0
msg_mkfile_err db 'Erreur creation', 13, 10, 0
msg_mkfile_usage db 'Usage : mkfile NOM.TXT', 13, 10, 0
msg_copy_ok db 'Fichier copie.', 13, 10, 0
msg_copy_err db 'Erreur de copie', 13, 10, 0
msg_del_ok db 'Fichier supprime.', 13, 10, 0
msg_del_err db 'Erreur de Suppression', 13, 10, 0
msg_del_protected db 'Suppression refusee : fichier systeme.',  13, 10, 0
msg_del_dir db 'Impossible de suprimer un repertoire.', 13, 10, 0
msg_del_usage db 'Usage : del NOM.TXT', 13, 10, 0
msg_ren_ok db 'Fichier renomme.', 13, 10, 0
msg_ren_err db 'Erreur renomage.', 13, 10, 0
msg_ren_usage db 'Usage : <ANCIEN NOM> <NOUVEAU NOM>', 13, 10, 0
msg_ren_protected db 'Impossible de Renommer : fichier systeme.', 13, 10, 0
msg_cd_err db 'Repertoire Introuvable', 13, 10, 0
msg_mkdir_ok db 'Repertoire', 13, 10, 0
msg_mkdir_err db 'Erreur lors de la creation du dossier', 13, 10, 0
msg_mkdir_usage db 'Usage : mkdir <NOM DU DOSSIER>', 13, 10, 0
msg_rmdir_ok db 'Dossier suprime.', 13, 10, 0
msg_rmdir_err db 'Erreur de suppression', 13, 10, 0
msg_rmdir_usage db 'rmdir <NOM DU DOSSIER>', 13, 10, 0
msg_dir_tag db ' <DIR>', 0
msg_format_warn db 'ATTENTION: Tout le disque dur va etre efface. Appuyez sur une touche pour continuer...', 13, 10, 0
msg_format_ok db 'Disque dur formate avec succes !', 13, 10, 0
msg_format_err db 'Erreur lors du formatage du disque dur.', 13, 10, 0 

cmd_help     db 'help', 0
cmd_dir      db 'dir', 0
cmd_type     db 'type', 0
cmd_cls      db 'cls', 0
cmd_ver      db 'ver', 0
cmd_reboot   db 'reboot', 0
cmd_rebootdos db 'rebootdos', 0
cmd_reset db 'reset', 0
cmd_echo db 'echo', 0
cmd_pause db 'pause', 0
cmd_date db  'date', 0
cmd_time db 'time', 0
cmd_fatinfo db 'fatinfo', 0
cmd_mkfile db 'mkfile', 0
cmd_copy_name db 'copy', 0
cmd_del db 'del', 0
cmd_ren db 'ren', 0
cmd_cd db 'cd', 0
cmd_mkdir db 'mkdir', 0
cmd_rmdir db 'rmdir', 0
cmd_format db 'format', 0

fatname      times 11 db 0
file_size    dw 0
cluster_temp dw 0
arg1_ptr dw 0
arg2_ptr dw 0
src_size dw 0
parse_skip dw 5
free_temp dw 0
current_cluster dw 0
saved_user_ds dw 0

temp_name times 64 db 0
open_files times 5*16 db 0
dir_entry_addr dw 0
saved_cluster dw 0
path_filename dw 0
resolve_saved_path times 64 db 0
current_path times 64 db 0
init_path db 'C:\', 0
buffer       times 256 db 0
current_drive db 0
saved_part_lba dd 0
fat16_ready db 0
format_drive db 0xFF
exec_saved_drive db 0
exec_saved_cluster dw 0
exec_saved_path times 64 db 0
floppy_init_path db 'A:\', 0
exe_source_cluster dw 0
exe_source_offset dw 0
exe_source_sectors dw 0
exe_source_sector_index db 0

%include "lba.inc"
%include "lba_write.inc"
%include "mbr.inc"
%include "bpb16.inc"
%include "fat16_init.inc"
%include "fat16_get.inc"
%include "fat16_set.inc"
%include "fat16_find_free.inc"
%include "fat16_find_file.inc"
%include "fat16_dir.inc"
%include "fat16_read_file.inc"
%include "fat16_write_file.inc"
%include "fat16_create_file.inc"
%include "fat16_create_dir.inc"
%include "fat16_del_file.inc"
%include "fat16_ren_file.inc"
%include "cmd_copy.inc"
%include "format.inc"
%include "format_floppy.inc"
%include "fat12.inc"
%include "int21_ext.inc"
%include "memory.inc"
%include "exec_exe.inc"