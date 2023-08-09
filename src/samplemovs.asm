bits 16
sub byte [bx], 34
; sub word [bx+di], 29 ; can't get this one correct
sub ax, 1000
sub al, -30
add bx, [bx+si]
add bx, [bp]
add si, byte 2
add bp, byte 2
add cx, byte 8
add bx, [bp+0]
add cx, [bx+2]
add bh, [bp+si+4]
add di, [bp+di+6]
add [bx+si], bx
add [bp], bx
add [bp+0], bx
add [bx+2], cx
add [bp+si+4], bh
add [bp+di+6], di
add byte [bx], 34
; add [bp+si+1000], word 29 ; can't get it right
add ax, [bp]
add al, [bx+si]
add ax, bx
add al, ah
add ax, 1000
add al, -30
add al, 9
sub bx, [bx+si]
sub bx, [bp]
; sub si, 2 ; this one my output says 'word' which is wrong
; sub bp, 2 ; my output says 'word' (incorrect)
; sub cx, 8 ; my output says 'word' (incorrect)
sub bx, [bp+0] ; my output says [bp] but seems to yield same binary
sub cx, [bx+2]
sub bh, [bp+si+4]
sub di, [bp+di+6]
sub [bx+si], bx
sub [bp], bx
sub [bp+0], bx
sub [bx+2], cx
sub [bp+si+4], bh
sub [bp+di+6], di
sub byte [bx], 34
; sub word [bx+di], 29 ; another 'word' one incorrect
sub ax, [bp]
sub al, [bx+si]
sub ax, bx
sub al, ah
sub ax, 1000
sub al, -30
add bx, [bx+si]
add bx, [bp]
add si, byte 2
add bp, byte 2
add cx, byte 8
add bx, [bp+0]
add cx, [bx+2]
add bh, [bp+si+4]
add di, [bp+di+6]
add [bx+si], bx
add [bp], bx
add [bp+0], bx
add [bx+2], cx
add [bp+si+4], bh
add [bp+di+6], di
add byte [bx], 34
; add word [bp+si+1000], 29 ; another 'word' one incorrect
add ax, [bp]
add al, [bx+si]
add ax, bx
add al, ah
add ax, 1000
add al, -30
add al, 9
sub bx, [bx+si]
sub bx, [bp]
; sub si, 2 ; this one my output says 'word' (incorrect)
; sub bp, 2 ; this one my output says 'word' (incorrect)
; sub cx, 8 ; this one my output says 'word' (incorrect)
sub bx, [bp+0]
sub cx, [bx+2]
sub bh, [bp+si+4]
sub di, [bp+di+6]
sub [bx+si], bx
sub [bp], bx
sub [bp+0], bx
sub [bx+2], cx
sub [bp+si+4], bh
sub [bp+di+6], di
sub ax, [bp]
sub al, [bx+si]
sub ax, bx
sub al, ah
cmp bx, [bx+si]
cmp bx, [bp]
; cmp si, word 2
; cmp bp, word 2
; cmp cx, word 8
cmp bx, [bp+0]
cmp cx, [bx+2]
cmp bh, [bp+si+4]
cmp di, [bp+di+6]
cmp [bx+si], bx
cmp [bp], bx
cmp [bp+0], bx
cmp [bx+2], cx
cmp [bp+si+4], bh
cmp [bp+di+6], di
cmp byte [bx], 34
; cmp word [4834], 29
cmp ax, [bp]
cmp al, [bx+si]
cmp ax, bx
cmp al, ah
cmp ax, 1000
cmp al, -30
cmp al, 9
test_label1:
jnz test_label1
; jnz test_label0 ; can't jump ahead 1 instruction although tbf why would we 
test_label0:
jnz test_label0
jnz test_label1
label2:
je label2
jl label2
jle label2
jb label2
jbe label2
jp label2
jo label2
js label2
jne label2
jnl label2
jg label2
jnb label2
ja label2
jnp label2
jno label2
jns label2
loop label2
loopz label2
loopnz label2
jcxz label2

