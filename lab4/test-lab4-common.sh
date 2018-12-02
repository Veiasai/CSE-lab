#!/bin/bash

VERBOSE=${VERBOSE:-}
SSH="ssh -o PasswordAuthentication=no -o StrictHostKeyChecking=no"
SCP="scp -o PasswordAuthentication=no -o StrictHostKeyChecking=no"
HADOOP="/home/cse/hadoop-2.8.5"
score=0

quiet() {
  if [ $VERBOSE ]; then
    $*
  else
    $* >/dev/null 2>/dev/null
  fi
}

quiet_err() {
  if [ $VERBOSE ]; then
    $*
  else
    $* 2>/dev/null
  fi
}

if [ "x$VERBOSE" == "x" ]; then
  QUIET_ERR="/dev/null"
else
  QUIET_ERR="&2"
fi

print_score() {
  echo "Part $PART score: $score/$TOTAL_SCORE"
}

safe_run() {
  if [ $VERBOSE ]; then
    ($*) || (echo "failed"; print_score; exit 1) || exit 1
  else
    ($* >/dev/null 2>&1) || (echo "failed"; print_score; exit 1) || exit 1
  fi
}

wait_port() {
  while (telnet $1 $2 </dev/null 2>&1 | grep "Connection refused" >/dev/null); do
    sleep 1
  done
}

wait_port_dead() {
  while ! (telnet $1 $2 </dev/null 2>&1 | grep "Connection refused" >/dev/null); do
    sleep 1
  done
}

stop_yfs_quiet() {
  $SSH name sudo iptables -F INPUT >/dev/null 2>&1
  $SSH data1 sudo iptables -F INPUT >/dev/null 2>&1
  $SSH data2 sudo iptables -F INPUT >/dev/null 2>&1
  fusermount -u yfs >/dev/null 2>&1
  rm -rf yfs >/dev/null 2>&1
  pkill -INT yfs_client >/dev/null 2>&1
  $SSH name pkill -INT namenode >/dev/null 2>&1
  $SSH name pkill -INT datanode >/dev/null 2>&1
  $SSH name pkill -INT extent_server >/dev/null 2>&1
  $SSH name pkill -INT lock_server >/dev/null 2>&1
  $SSH data1 pkill -INT datanode >/dev/null 2>&1
  $SSH data1 pkill -INT extent_server >/dev/null 2>&1
  $SSH data2 pkill -INT datanode >/dev/null 2>&1
  $SSH data2 pkill -INT extent_server >/dev/null 2>&1
}

stop_yfs() {
  echo "Stop YFS..."
  stop_yfs_quiet
}

stop_hdfs_quiet() {
  safe_run $SSH name $HADOOP/sbin/hadoop-daemon.sh stop namenode
  safe_run $SSH data1 $HADOOP/sbin/hadoop-daemon.sh stop datanode
  safe_run $SSH data2 $HADOOP/sbin/hadoop-daemon.sh stop datanode
}

stop_hdfs() {
  echo "Stop HDFS..."
  stop_hdfs_quiet
}

start_hdfs() {
  echo "Start HDFS..."
  safe_run $SSH name $HADOOP/sbin/hadoop-daemon.sh start namenode
  safe_run $SSH data1 $HADOOP/sbin/hadoop-daemon.sh start datanode
  safe_run $SSH data2 $HADOOP/sbin/hadoop-daemon.sh start datanode
}

stop_name() {
  $SSH name $HADOOP/sbin/hadoop-daemon.sh stop namenode >/dev/null
  return $?
}

stop_data1() {
  $SSH data1 $HADOOP/sbin/hadoop-daemon.sh stop datanode >/dev/null
  return $?
}

stop_data2() {
  $SSH data2 $HADOOP/sbin/hadoop-daemon.sh stop datanode >/dev/null
  return $?
}

generate_wordcount() {
  if [ -d wordcount.txt ]; then
    return
  fi
  echo -n "Generate test case... "
  $HADOOP/bin/hdfs dfs -rm -r /wordcount.txt >/dev/null 2>&1
  safe_run $HADOOP/bin/hadoop jar $HADOOP/share/hadoop/mapreduce/hadoop-mapreduce-examples-2.8.5.jar randomtextwriter -D mapreduce.randomtextwriter.totalbytes=$INPUT_SIZE -outFormat org.apache.hadoop.mapreduce.lib.output.TextOutputFormat /wordcount.txt
  rm -rf wordcount.txt >/dev/null 2>&1
  safe_run $HADOOP/bin/hdfs dfs -get /wordcount.txt
  $HADOOP/bin/hdfs dfs -rm -r /result.txt >/dev/null 2>&1
  safe_run $HADOOP/bin/hadoop jar $HADOOP/share/hadoop/mapreduce/hadoop-mapreduce-examples-2.8.5.jar wordcount /wordcount.txt /result.txt
  rm -rf result.txt >/dev/null 2>&1
  safe_run $HADOOP/bin/hdfs dfs -get /result.txt
  safe_run mv result.txt result.ans
  echo "done"
}

import_wordcount() {
  echo -n "Import sample text into HDFS... "
  $HADOOP/bin/hdfs dfs -rm -r /wordcount.txt >/dev/null 2>&1
  quiet $HADOOP/bin/hdfs dfs -put wordcount.txt /
  if [ $? -ne 0 ]; then
    echo "failed"
    return 1
  fi
  echo "ok (10 pts)"
  score=$(($score + 10))
  return 0
}

run_wordcount() {
  echo "Run word count..."
  quiet $HADOOP/bin/hdfs dfs -rm -r /result.txt
  quiet $HADOOP/bin/hadoop jar $HADOOP/share/hadoop/mapreduce/hadoop-mapreduce-examples-2.8.5.jar wordcount /wordcount.txt /result.txt
  if [ $? -ne 0 ]; then
    echo "failed"
    return 1
  fi
  quiet rm -rf result.txt
  quiet $HADOOP/bin/hdfs dfs -get /result.txt
  return 0
}

compare() {
  echo -n "Compare your output with standard answer... "
  if (diff -r result.txt result.ans >/dev/null 2>&1); then
    echo "correct (10 pts)"
    score=$(($score + 10))
  else
    echo "wrong"
    return
  fi
}

download_wordcount() {
  if [[ ! -d wordcount.txt || ! -d result.ans ]]; then
    echo -n "Download test case... "
    quiet wget
    if [ $? -ne 0 ]; then
      echo "failed"
      return 1
    fi
    echo "done"
  fi
  return 0
}

test_mapreduce() {
  if ! download_wordcount; then
    return
  fi
  if ! import_wordcount; then
    return
  fi
  if ! run_wordcount; then
    return
  fi
  compare
}

remote_grade() {
  $SSH -t cse@$(cat app_public_ip) bash -c "\"VERBOSE=$VERBOSE $1\""
  return $?
}
