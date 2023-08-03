bits 16
mov sp, di
mov bp, ax
mov bp, [bx + si + 16]  ; basically pointer math
mov [bx + 16], bp
mov bp, [32]       ; load address 12 into the bx register
mov [32], bp
mov cx, bx
mov ch, ah
mov dx, bx
mov si, bx
mov bx, di
mov al, cl
mov ch, ch
mov bx, ax
mov bx, si
mov cx, 5
mov cx, -5
mov dx, 3948
mov dx, -3948
mov dx, [bp]
mov al, [bx + si]
mov bx, [bp + di]
mov ah, [bx + si + 4] 
mov al, [bx + si + 4999]
mov [bx + di], cx
mov [bp + si], cl
mov [bp], ch
