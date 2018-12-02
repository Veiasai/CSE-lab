#!/bin/bash

PART=2

. test-lab4-common.sh

TOTAL_SCORE=$((5 + 5 + 5 + 5 + 5 + 5 + 10 + 10))

atexit() {
  stop_yfs_quiet
  exit $1
}

send_yfs() {
  safe_run $SCP namenode lock_server datanode yfs_client extent_server name:
}

start_yfs() {
  echo "Start YFS..."
  $SSH name ./extent_server 1234 </dev/null >extent_server.log 2>&1 &
  $SSH name ./lock_server 1235 </dev/null >lock_server.log 2>&1 &
  wait_port name 1234
  wait_port name 1235
  $SSH name ./namenode localhost:1234 localhost:1235 </dev/null >namenode.log 2>&1 &
  wait_port name 9000
  $SSH name ./datanode localhost:1234 localhost:9000 </dev/null >datanode.log 2>&1 &
  (mkdir yfs; ./yfs_client yfs name:1234 name:1235) >yfs_client.log 2>&1 &
  sleep 2
}

compile() {
  echo "Compile your code"
  make || atexit 1
}

check_touch() {
  echo -n "Test touch /test_touch... "
  if ! (quiet $HADOOP/bin/hdfs dfs -touchz /test_touch); then
    echo "failed"
    return
  fi
  echo "ok (5 pts)"
  score=$(($score + 5))
}

check_ls() {
  echo -n "Test ls /... "
  result=$(quiet_err $HADOOP/bin/hdfs dfs -ls /)
  if [ $? -ne 0 ]; then
    echo "failed"
    return
  fi
  if ! (echo "$result" | grep "/test_touch" >/dev/null); then
    echo "failed"
    return
  fi
  echo "ok (5 pts)"
  score=$(($score + 5))
}

check_put() {
  echo -n "Test put /test_put... "
  echo -n "test" >test_put
  if ! (quiet $HADOOP/bin/hdfs dfs -put test_put /test_put); then
    echo "failed"
    rm test_put
    return
  fi
  rm test_put
  echo "ok (5 pts)"
  score=$(($score + 5))
}

check_cat() {
  echo -n "Test cat /test_put... "
  result=$(quiet_err $HADOOP/bin/hdfs dfs -cat /test_put)
  if [ $? -ne 0 ]; then
    echo "failed"
    return
  fi
  if [ "$result" != "test" ]; then
    echo "failed"
    return
  fi
  echo "ok (5 pts)"
  score=$(($score + 5))
}

check_mkdir() {
  echo -n "Test mkdir /test_mkdir... "
  if ! (quiet $HADOOP/bin/hdfs dfs -mkdir /test_mkdir); then
    echo "failed"
    return
  fi
  echo "ok (5 pts)"
  score=$(($score + 5))
}

check_rm() {
  echo -n "Test rm /test_put... "
  if ! (quiet $HADOOP/bin/hdfs dfs -rm /test_put); then
    echo "failed"
    return
  fi
  echo "ok (5 pts)"
  score=$(($score + 5))
}

check_dfs() {
  check_touch
  check_ls
  check_put
  check_cat
  check_mkdir
  check_rm
  quiet $HADOOP/bin/hdfs dfs -rm -f -r /test_touch /test_put /test_mkdir
}

test_yfs_to_hdfs() {
  echo -n "Test interoperability (YFS to HDFS)... "
  echo -n test >yfs/test4
  echo -n test123 >yfs/test7
  mkdir yfs/testdir
  result=$(quiet_err $HADOOP/bin/hdfs dfs -ls /)
  if [ $? -ne 0 ]; then
    echo "failed"
    echo "ls / failed"
    return
  fi
  result=$(echo "$result" | grep -v "Found .* item")
  check=$(echo "$result" | grep /test4 | tr -s " " | cut -d' ' -f 5)
  if [ "x$check" != "x4" ]; then
    echo "failed"
    echo "Wrong size of /test4: $check"
    return
  fi
  check=$(echo "$result" | grep /test7 | tr -s " " | cut -d' ' -f 5)
  if [ "x$check" != "x7" ]; then
    echo "failed"
    echo "Wrong size of /test7: $check"
    return
  fi
  check=$(echo "$result" | grep /testdir | tr -s " " | cut -d' ' -f 1)
  if [ "x$check" != "xdrwxrwxrwx" ]; then
    echo "failed"
    echo "Wrong type or permission of /testdir: $check"
    return
  fi
  check=$(quiet_err $HADOOP/bin/hdfs dfs -cat /test4)
  if [ $? -ne 0 ]; then
    echo "failed"
    echo "Read /test4 failed"
    return
  fi
  if [ "x$check" != "xtest" ]; then
    echo "failed"
    echo "Wrong content of /test4: $check"
    return
  fi
  check=$(quiet_err $HADOOP/bin/hdfs dfs -cat /test7)
  if [ $? -ne 0 ]; then
    echo "failed"
    echo "Read /test7 failed"
    return
  fi
  if [ "x$check" != "xtest123" ]; then
    echo "failed"
    echo "Wrong content of /test7: $check"
    return
  fi
  echo "ok (10 pts)"
  score=$(($score + 10))
}

test_hdfs_to_yfs() {
  echo -n "Test interoperability (HDFS to YFS)... "
  echo -n test >test4
  echo -n test123 >test7
  quiet $HADOOP/bin/hdfs dfs -put test4 /test4
  if [ $? -ne 0 ]; then
    echo "failed"
    echo "Put test4 into HDFS failed"
    return
  fi
  quiet $HADOOP/bin/hdfs dfs -put test7 /test7
  if [ $? -ne 0 ]; then
    echo "failed"
    echo "Put test7 into HDFS failed"
    return
  fi
  rm test4 test7
  quiet $HADOOP/bin/hdfs dfs -mkdir /testdir2
  if [ $? -ne 0 ]; then
    echo "failed"
    echo "mkdir /testdir2 in HDFS failed"
    return
  fi
  result=$(quiet_err ls -l yfs/)
  if [ $? -ne 0 ]; then
    echo "failed"
    echo "ls yfs/ failed"
    return
  fi
  check=$(echo "$result" | grep test4 | tr -s " " | cut -d' ' -f 5)
  if [ "x$check" != "x4" ]; then
    echo "failed"
    echo "Wrong size of yfs/test4: $check"
    return
  fi
  check=$(echo "$result" | grep test7 | tr -s " " | cut -d' ' -f 5)
  if [ "x$check" != "x7" ]; then
    echo "failed"
    echo "Wrong size of yfs/test7: $check"
    return
  fi
  check=$(echo "$result" | grep testdir2 | tr -s " " | cut -d' ' -f 1)
  if [ "x$check" != "xdrwxrwxrwx" ]; then
    echo "failed"
    echo "Wrong type or permission of yfs/testdir2: $check"
    return
  fi
  check=$(quiet_err cat yfs/test4)
  if [ $? -ne 0 ]; then
    echo "failed"
    echo "Read yfs/test4 failed"
    return
  fi
  if [ "x$check" != "xtest" ]; then
    echo "failed"
    echo "Wrong content of yfs/test4: $check"
    return
  fi
  check=$(quiet_err cat yfs/test7)
  if [ $? -ne 0 ]; then
    echo "failed"
    echo "Read yfs/test7 failed"
    return
  fi
  if [ "x$check" != "xtest123" ]; then
    echo "failed"
    echo "Wrong content of yfs/test7: $check"
    return
  fi
  echo "ok (10 pts)"
  score=$(($score + 10))
}

test_interoperability() {
  test_yfs_to_hdfs
  quiet rm -rf yfs/*
  test_hdfs_to_yfs
  quiet $HADOOP/bin/hdfs dfs -rm -r -f /test4 /test7 /testdir2
  quiet rm -rf test4 test7
}

if [ -f app_public_ip ]; then
  compile
  safe_run $SCP namenode datanode lock_server extent_server yfs_client test-lab4-part2.sh test-lab4-common.sh cse@$(cat app_public_ip): >/dev/null 2>&1
  remote_grade /home/cse/test-lab4-part2.sh
  ret=$?
  safe_run $SCP cse@$(cat app_public_ip):*.log ./ >/dev/null 2>&1
  safe_run $SSH cse@$(cat app_public_ip) rm namenode datanode yfs_client lock_server extent_server test-lab4-part2.sh test-lab4-common.sh *.log >/dev/null 2>&1
  exit $ret
fi

trap "stop_yfs; exit 1" INT

stop_hdfs
stop_yfs
send_yfs
start_yfs
check_dfs
test_interoperability
stop_yfs_quiet
print_score
