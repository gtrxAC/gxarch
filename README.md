**This is the "rewrite" of gxarch, with a better instruction set, better registers, etc. For the old gxarch, choose the branch `v1`.**

## Features
* 8-bit system with 16-bit arithmetic
* 64K shared RAM/ROM, 32 registers
* 24 instructions
* 128 × 128 screen


## Example

```
; %0: scroll counter

dat main         ; program entry point

main:            ; define constants
	set %1 1     ; %1: scroll increment
	set %2 0     ; %2: draw location
	set %3 128   ; %3: screen size

loop:
	add %0 %1 %0 ; increment scroll counter
	dw  %0 %0 %3 ; take a 128×128 area from the tileset at %0, %0
	at  %2 %2    ; and draw it at 0, 0
	end          ; draw frame
	jmp loop
```
![](assets/example.gif)