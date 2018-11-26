#!/bin/bash

while [ $# -ne 0 ]; do
  case $1 in
    -q)
      QUIET=1
      ;;
    *)
      ;;
  esac
  shift
done

if [ "x$QUIET" != "x1" ]; then
  score=0
  exec 5>&1
  ./test-lab4-part0.sh
  if [ $? -eq 0 ]; then
    echo "Part 0 OK"
  else
    echo "Part 0 ERROR"
  fi
  part1=$(./test-lab4-part1.sh | tee /dev/fd/5 | grep -o -P "(?<=Part 1 score: )[0-9]+")
  score=$(($score + $part1))
  part2=$(./test-lab4-part2.sh | tee /dev/fd/5 | grep -o -P "(?<=Part 2 score: )[0-9]+")
  score=$(($score + $part2))
  part3=$(./test-lab4-part3.sh | tee /dev/fd/5 | grep -o -P "(?<=Part 3 score: )[0-9]+")
  score=$(($score + $part3))
  echo "Score: $score/$((15 + 50 + 35))"
else
  score=0
  part1=$(./test-lab4-part1.sh 2>/dev/null | grep -o -P "(?<=Part 1 score: )[0-9]+")
  score=$(($score + $part1))
  part2=$(./test-lab4-part2.sh 2>/dev/null | grep -o -P "(?<=Part 2 score: )[0-9]+")
  score=$(($score + $part2))
  part3=$(./test-lab4-part3.sh 2>/dev/null | grep -o -P "(?<=Part 3 score: )[0-9]+")
  score=$(($score + $part3))
  echo $score
fi
