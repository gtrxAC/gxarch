This is the "rewrite" of gxarch, with a better instruction set and better registers. There is no assembler, save RAM support, etc. For the old gxarch, choose the branch `v1`.

## Features
* 8-bit system with 16-bit arithmetic
* 64K shared RAM/ROM, 32 registers
* 24 instructions
* 128 × 128 screen


## Example

```
; %0 = scroll counter
; %1 = 1 (scroll increment)
; %2 = 0 (draw location)
; %3 = 128 (screen size)

dat main         ; program entry point

main:
	set %1 1
	set %2 0
	set %3 128

loop:
	add %0 %1 %0 ; increment scroll counter
	dw  %0 %0 %3 ; take a 128×128 area starting at %0
	at  %2 %2    ; and draw it at 0, 0
	end          ; draw frame
	jmp loop
```
![](assets/example.gif)