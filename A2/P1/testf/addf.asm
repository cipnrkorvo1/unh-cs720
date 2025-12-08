export mainx20
export result
export word1
export word2

mainx20:
  load   r1, word1
  load   r2, word2
  addf   r1, r2
done:
  store  r1, resultx
  halt

result:
  word 0
word1:
  word 0x3f800000
word2:
  word 0x40000000
import resultx
