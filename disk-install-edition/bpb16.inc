; ============================================
; bpb16.inc - Parseur de BPB FAT16
; Pour MicroDOS Disk Install Edition
; ============================================

bpb_bytes_per_sector dw 0
bpb_sectors_per_cluster db 0
bpb_reserved_sectors dw 0
bpb_num_fats db 0
bpb_root_entries dw 0
bpb_sectors_per_fat dw 0

bpb16_parse:

	push ax
	push si
	push ds

	push es
	pop ds

	mov si, bx

	mov ax, [si+11]
	mov [bpb_bytes_per_sector], ax

	mov al, [si+13]
	mov [bpb_sectors_per_cluster], al

	mov ax, [si+14]
	mov [bpb_reserved_sectors], ax

	mov al, [si+16]
	mov [bpb_num_fats], al

	mov ax, [si+17]
	mov [bpb_root_entries], ax
	
	mov ax, [si+22]
	mov [bpb_sectors_per_fat], ax

	pop ds
	pop si
	pop ax
	ret
