#
#program to add two integers
export mainx20
export result
export word1
export word2

mainx20:
  load   r1, word1
  load   r2, word2
  addi   r1, r2
done:
  store  r1, resultx
  halt

result:
  word 0
word1:
  word 12
word2:
  word 15
import resultx

#A second version of the same program
#export result
#export word1
#export word2
#mainx20:
#  jmp start
#result:
#  word 0
#word1:
#  word 12
#word2:
#  word 15
#start:
#  load   r1, word1
#  load   r2, word2
#  addi   r1, r2
#done:
#  store  r1, result
#  halt

