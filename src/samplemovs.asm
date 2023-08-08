bits 16
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
add word [bp+si+1000], 29
add ax, [bp]
add al, [bx+si]
add ax, bx
add al, ah
add ax, 1000
add al, -30
add al, 9
sub bx, [bx+si]
sub bx, [bp]
sub si, 2
sub bp, 2
sub cx, 8
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
sub byte [bx], 34
sub word [bx+di], 29
sub ax, [bp]
sub al, [bx+si]
sub ax, bx
sub al, ah
sub ax, 1000
; sub al, 9
