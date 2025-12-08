export begin
export done
 ldimm r0, 0
 ldimm r1, 1
 ldimm r2, 5
begin: beq r0, r2, out
 beq r0, r2, done
 jmp begin
done:
 halt
import out
