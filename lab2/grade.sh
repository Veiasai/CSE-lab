#!/bin/bash
./stop.sh >/dev/null 2>&1
./stop.sh >/dev/null 2>&1
./stop.sh >/dev/null 2>&1
./stop.sh >/dev/null 2>&1
./stop.sh >/dev/null 2>&1

score=0

mkdir yfs1 >/dev/null 2>&1
mkdir yfs2 >/dev/null 2>&1

./start.sh

test_if_has_mount(){
	mount | grep -q "yfs_client"
	if [ $? -ne 0 ];
	then
			echo "FATAL: Your YFS client has failed to mount its filesystem!"
			exit
	fi;
	yfs_count=$(ps -e | grep -o "yfs_client" | wc -l)
	extent_count=$(ps -e | grep -o "extent_server" | wc -l)

	if [ $yfs_count -ne 2 ];
	then
			echo "error: yfs_client not found (expecting 2)"
			exit
	fi;

	if [ $extent_count -ne 1 ];
	then
			echo "error: extent_server not found"
			exit
	fi;
}
test_if_has_mount

##################################################

# run test 1
./test-lab2-part1-a.pl yfs1 | grep -q "Passed all"
if [ $? -ne 0 ];
then
        echo "Failed test-part1-A"
        #exit
else

	ps -e | grep -q "yfs_client"
	if [ $? -ne 0 ];
	then
			echo "FATAL: yfs_client DIED!"
			exit
	else
		score=$((score+10))
		#echo $score
		echo "Passed part1 A"
	fi

fi
test_if_has_mount

##################################################

./test-lab2-part1-b.pl yfs1 | grep -q "Passed all"
if [ $? -ne 0 ];
then
        echo "Failed test-part1-B"
        #exit
else

	ps -e | grep -q "yfs_client"
	if [ $? -ne 0 ];
	then
			echo "FATAL: yfs_client DIED!"
			exit
	else
		score=$((score+10))
		#echo $score
		echo "Passed part1 B"
	fi

fi
test_if_has_mount

##################################################

./test-lab2-part1-c.pl yfs1 | grep -q "Passed all"
if [ $? -ne 0 ];
then
        echo "Failed test-part1-c"
        #exit
else

	ps -e | grep -q "yfs_client"
	if [ $? -ne 0 ];
	then
			echo "FATAL: yfs_client DIED!"
			exit
	else
		score=$((score+10))
		#echo $score
		echo "Passed part1 C"
	fi

fi
test_if_has_mount

##################################################


./test-lab2-part1-d.sh yfs1 >tmp.1
./test-lab2-part1-d.sh yfs2 >tmp.2
lcnt=$(cat tmp.1 tmp.2 | grep -o "Passed SYMLINK" | wc -l)

if [ $lcnt -ne 2 ];
then
        echo "Failed test-part1-d"
        #exit
else

	ps -e | grep -q "yfs_client"
	if [ $? -ne 0 ];
	then
			echo "FATAL: yfs_client DIED!"
			exit
	else
		score=$((score+10))
		echo "Passed part1 D"
		#echo $score
	fi

fi
test_if_has_mount

rm tmp.1 tmp.2

##################################################################################

./test-lab2-part1-e.sh yfs1 >tmp.1
./test-lab2-part1-e.sh yfs2 >tmp.2
lcnt=$(cat tmp.1 tmp.2 | grep -o "Passed BLOB" | wc -l)

if [ $lcnt -ne 2 ];
then
        echo "Failed test-part1-e"
else
        #exit
		ps -e | grep -q "yfs_client"
		if [ $? -ne 0 ];
		then
				echo "FATAL: yfs_client DIED!"
				exit
		else
			score=$((score+10))
			echo "Passed part1 E"
			#echo $score
		fi
fi

test_if_has_mount

rm tmp.1 tmp.2
##################################################################################
robust(){
./test-lab2-part1-f.sh yfs1 | grep -q "Passed ROBUSTNESS test"
if [ $? -ne 0 ];
then
        echo "Failed test-part1-f"
else
        #exit
		ps -e | grep -q "yfs_client"
		if [ $? -ne 0 ];
		then
				echo "FATAL: yfs_client DIED!"
				exit
		else
			score=$((score+10))
			echo "Passed part1 F -- Robustness"
			#echo $score
		fi
fi

test_if_has_mount
}



##################################################################################
consis_test(){
./test-lab2-part1-g yfs1 yfs2 | grep -q "test-lab2-part1-g: Passed all tests."
if [ $? -ne 0 ];
then
        echo "Failed test-part1-g"
else
        #exit
		ps -e | grep -q "yfs_client"
		if [ $? -ne 0 ];
		then
				echo "FATAL: yfs_client DIED!"
				exit
		else
			score=$((score+10))
			echo "Passed part1 G (consistency)"
			#echo $score
		fi
fi
}

consis_test

if [ $score -eq 60 ];
then
	echo "Lab2 part 1 passed"
else
	echo "Lab2 part 1 failed"
fi

test_if_has_mount

##################################################################################
./test-lab2-part2-a yfs1 yfs2 | tee tmp.0
lcnt=$(cat tmp.0 | grep -o "OK" | wc -l)

if [ $lcnt -ne 5 ];
then
        echo "Failed test-part2-a: pass "$lcnt"/5"
	score=$((score+$lcnt*10))
else
        #exit
		ps -e | grep -q "yfs_client"
		if [ $? -ne 0 ];
		then
				echo "FATAL: yfs_client DIED!"
				exit
		else
			score=$((score+50))
			echo "Passed part2 A"
			#echo $score
		fi
fi

rm tmp.0

test_if_has_mount

##################################################################################
./test-lab2-part2-b yfs1 yfs2 | tee tmp.0
lcnt=$(cat tmp.0 | grep -o "OK" | wc -l)

if [ $lcnt -ne 1 ];
then
        echo "Failed test-part2-b"
else
        #exit
		ps -e | grep -q "yfs_client"
		if [ $? -ne 0 ];
		then
				echo "FATAL: yfs_client DIED!"
				exit
		else
			score=$((score+10))
			echo "Passed part2 B"
			#echo $score
		fi
fi

rm tmp.0

# finally reaches here!
#echo "Passed all tests!"

./stop.sh
echo ""
echo "Score: "$score"/120"
