#!/bin/bash

PART=3

. test-lab4-common.sh

TOTAL_SCORE=$((5 + 5 + 10 + 15))

atexit() {
  stop_yfs_quiet
  quiet $SSH name sudo iptables -F INPUT
  quiet $SSH data1 sudo iptables -F INPUT
  quiet $SSH data2 sudo iptables -F INPUT
  exit $1
}

send_yfs() {
  safe_run $SCP namenode lock_server yfs_client extent_server datanode name:
  safe_run $SCP datanode extent_server data1:
  safe_run $SCP datanode extent_server data2:
}

start_yfs() {
  echo "Start YFS..."
  $SSH name ./extent_server 1234 </dev/null >extent_server0.log 2>&1 &
  $SSH name ./lock_server 1235 </dev/null >lock_server.log 2>&1 &
  wait_port name 1234
  wait_port name 1235
  $SSH name ./namenode localhost:1234 localhost:1235 </dev/null >namenode.log 2>&1 &
  wait_port name 9000
  $SSH name ./datanode localhost:1234 name:9000 </dev/null >datanode0.log 2>&1 &
  $SSH data1 ./extent_server 1234 </dev/null >extent_server1.log 2>&1 &
  wait_port data1 1234
  $SSH data1 ./datanode localhost:1234 name:9000 </dev/null >datanode1.log 2>&1 &
  $SSH data2 ./extent_server 1234 </dev/null >extent_server2.log 2>&1 &
  wait_port data2 1234
  $SSH data2 ./datanode localhost:1234 name:9000 </dev/null >datanode2.log 2>&1 &
  (mkdir yfs; ./yfs_client yfs name:1234 name:1235) >yfs_client.log 2>&1 &
  sleep 2
}

stop_datanode() {
  $SSH $1 pkill -INT datanode >/dev/null 2>&1
  if [ "x$1" != "xname" ]; then
    $SSH $1 pkill -INT extent_server >/dev/null 2>&1
  fi
}

start_yfs_data1() {
  quiet $SSH name sudo iptables -F INPUT
  quiet $SSH data1 sudo iptables -F INPUT
  quiet $SSH data2 sudo iptables -F INPUT
  echo "Restart" >>extent_server1.log
  $SSH data1 ./extent_server 1234 </dev/null >>extent_server1.log 2>&1 &
  wait_port data1 1234
  echo "Restart" >>datanode1.log
  $SSH data1 ./datanode localhost:1234 name:9000 </dev/null >>datanode1.log 2>&1 &
}

start_yfs_data2() {
  quiet $SSH name sudo iptables -F INPUT
  quiet $SSH data1 sudo iptables -F INPUT
  quiet $SSH data2 sudo iptables -F INPUT
  echo "Restart" >>extent_server2.log
  $SSH data2 ./extent_server 1234 </dev/null >>extent_server2.log 2>&1 &
  wait_port data2 1234
  echo "Restart" >>datanode2.log
  $SSH data2 ./datanode localhost:1234 name:9000 </dev/null >>datanode2.log 2>&1 &
}

wait_datanode_ok() {
  retry=$2
  while [ 1 ]; do
    result=$(quiet_err $HADOOP/bin/hdfs dfsadmin -report)
    if (echo "$result" | grep "Hostname: $1" >/dev/null); then
      return 0
    fi
    if [ $retry -eq 0 ]; then
      return 1
    fi
    retry=$(($retry - 1))
    sleep 1
  done
}

wait_datanode_dead() {
  retry=$2
  while [ 1 ]; do
    result=$(quiet_err $HADOOP/bin/hdfs dfsadmin -report)
    if ! (echo "$result" | grep "Hostname: $1" >/dev/null); then
      return 0
    fi
    if [ $retry -eq 0 ]; then
      return 1
    fi
    retry=$(($retry - 1))
    sleep 1
  done
}

compile() {
  echo "Compile your code"
  make || atexit 1
}

check_report() {
  echo -n "Test basic datanode report... "
  result=$(quiet_err $HADOOP/bin/hdfs dfsadmin -report)
  if ! (echo "$result" | grep "Live datanodes (3)" >/dev/null); then
    echo "failed"
    echo "Wrong number of datanodes reported"
    return
  fi
  echo "ok (5 pts)"
  score=$(($score + 5))
}

check_heartbeat() {
  echo -n "Test heartbeat... "
  stop_datanode data1
  sleep 5
  result=$(quiet_err $HADOOP/bin/hdfs dfsadmin -report)
  if (echo "$result" | grep "Hostname: data1" >/dev/null); then
    echo "failed"
    echo "death of data1 is not detected"
    return
  fi
  if ! (echo "$result" | grep "Hostname: name" >/dev/null); then
    echo "failed"
    echo "name is marked as dead"
    return
  fi
  if ! (echo "$result" | grep "Hostname: data2" >/dev/null); then
    echo "failed"
    echo "data2 is marked as dead"
    return
  fi
  start_yfs_data1
  sleep 2
  result=$(quiet_err $HADOOP/bin/hdfs dfsadmin -report)
  if ! (echo "$result" | grep "Hostname: data1" >/dev/null); then
    echo "failed"
    echo "data1 not recovered"
    return
  fi
  echo "ok (5 pts)"
  score=$(($score + 5))
}

check_replica() {
  echo -n "Test whether the blocks are well replicated... "
  quiet dd if=/dev/urandom of=random bs=4096 count=13
  quiet $HADOOP/bin/hdfs dfs -put random /random

  quiet $SSH name sudo iptables -F INPUT
  quiet $SSH data1 sudo iptables -F INPUT
  quiet $SSH data2 sudo iptables -F INPUT
  quiet $SSH data1 sudo iptables -A INPUT -s $(getent hosts app | head -1 | cut -d' ' -f1) -p tcp --dport 50010 -j REJECT
  quiet $SSH data2 sudo iptables -A INPUT -s $(getent hosts app | head -1 | cut -d' ' -f1) -p tcp --dport 50010 -j REJECT
  quiet rm random.get
  quiet $HADOOP/bin/hdfs dfs -get /random random.get
  if [ $? -ne 0 ]; then
    echo "failed"
    echo "Couldn't read file back from name"
    return
  fi
  if ! (diff random random.get >/dev/null 2>&1); then
    echo "failed"
    echo "The data on name is incorrect"
    return
  fi

  quiet $SSH name sudo iptables -F INPUT
  quiet $SSH data1 sudo iptables -F INPUT
  quiet $SSH data2 sudo iptables -F INPUT
  quiet $SSH name sudo iptables -A INPUT -s $(getent hosts app | head -1 | cut -d' ' -f1) -p tcp --dport 50010 -j REJECT
  quiet $SSH data2 sudo iptables -A INPUT -s $(getent hosts app | head -1 | cut -d' ' -f1) -p tcp --dport 50010 -j REJECT
  quiet rm random.get
  quiet $HADOOP/bin/hdfs dfs -get /random random.get
  if [ $? -ne 0 ]; then
    echo "failed"
    echo "Couldn't read file back from data1"
    return
  fi
  if ! (diff random random.get >/dev/null 2>&1); then
    echo "failed"
    echo "The data on data1 is incorrect"
    return
  fi

  quiet $SSH name sudo iptables -F INPUT
  quiet $SSH data1 sudo iptables -F INPUT
  quiet $SSH data2 sudo iptables -F INPUT
  quiet $SSH name sudo iptables -A INPUT -s $(getent hosts app | head -1 | cut -d' ' -f1) -p tcp --dport 50010 -j REJECT
  quiet $SSH data1 sudo iptables -A INPUT -s $(getent hosts app | head -1 | cut -d' ' -f1) -p tcp --dport 50010 -j REJECT
  quiet rm random.get
  quiet $HADOOP/bin/hdfs dfs -get /random random.get
  if [ $? -ne 0 ]; then
    echo "failed"
    echo "Couldn't read file back from data2"
    return
  fi
  if ! (diff random random.get >/dev/null 2>&1); then
    echo "failed"
    echo "The data on data2 is incorrect"
    return
  fi

  quiet $SSH name sudo iptables -F INPUT
  quiet $SSH data1 sudo iptables -F INPUT
  quiet $SSH data2 sudo iptables -F INPUT
  quiet $SSH name sudo iptables -A INPUT -s $(getent hosts app | head -1 | cut -d' ' -f1) -p tcp --dport 50010 -j REJECT
  quiet $SSH data1 sudo iptables -A INPUT -s $(getent hosts app | head -1 | cut -d' ' -f1) -p tcp --dport 50010 -j REJECT
  quiet $SSH data2 sudo iptables -A INPUT -s $(getent hosts app | head -1 | cut -d' ' -f1) -p tcp --dport 50010 -j REJECT
  quiet rm random.get
  quiet $HADOOP/bin/hdfs dfs -get /random random.get
  if [ $? -eq 0 ]; then
    echo "failed"
    echo "Should not read data when all datanodes is unreachable"
    return
  fi

  echo "ok (10 pts)"
  score=$(($score + 10))
}

check_replica_clean() {
  rm random random.get >/dev/null 2>&1
  quiet $HADOOP/bin/hdfs dfs -rm /random
  quiet $SSH name sudo iptables -F INPUT
  quiet $SSH data1 sudo iptables -F INPUT
  quiet $SSH data2 sudo iptables -F INPUT
}

test_recovery() {
  echo "Test recovery..."

  echo -n "Kill data1... "
  stop_datanode data1
  if ! wait_datanode_dead data1 5; then
    echo "failed"
    echo "Namenode didn't detect the death"
    return
  fi
  echo "ok"

  echo -n "Kill data2... "
  stop_datanode data2
  if ! wait_datanode_dead data2 5; then
    echo "failed"
    echo "Namenode didn't detect the death"
    return
  fi
  echo "ok"

  echo -n "Restart data1... "
  start_yfs_data1
  if ! wait_datanode_ok data1 5; then
    echo "failed"
    echo "data1 didn't recover"
    return
  fi
  echo "ok"

  echo -n "Import random file into HDFS... "
  quiet dd if=/dev/urandom of=random bs=4096 count=13
  quiet $HADOOP/bin/hdfs dfs -put random /random
  echo "ok"

  echo -n "Try read data from data1... "
  quiet $SSH name sudo iptables -F INPUT
  quiet $SSH data1 sudo iptables -F INPUT
  quiet $SSH data2 sudo iptables -F INPUT
  quiet $SSH name sudo iptables -A INPUT -s $(getent hosts app | head -1 | cut -d' ' -f1) -p tcp --dport 50010 -j REJECT
  quiet $SSH data2 sudo iptables -A INPUT -s $(getent hosts app | head -1 | cut -d' ' -f1) -p tcp --dport 50010 -j REJECT
  quiet rm random.get
  quiet $HADOOP/bin/hdfs dfs -get /random random.get
  if [ $? -ne 0 ]; then
    echo "failed"
    echo "Couldn't read file back from data1"
    return
  fi
  if ! (diff random random.get >/dev/null 2>&1); then
    echo "failed"
    echo "The data on data1 is incorrect"
    return
  fi
  echo "ok"

  echo -n "Restart data2... "
  start_yfs_data2
  if ! wait_datanode_ok data2 5; then
    echo "failed"
    echo "data2 didn't recover"
    return
  fi
  echo "ok"

  echo -n "Try read data from data2... "
  quiet $SSH name sudo iptables -F INPUT
  quiet $SSH data1 sudo iptables -F INPUT
  quiet $SSH data2 sudo iptables -F INPUT
  quiet $SSH name sudo iptables -A INPUT -s $(getent hosts app | head -1 | cut -d' ' -f1) -p tcp --dport 50010 -j REJECT
  quiet $SSH data1 sudo iptables -A INPUT -s $(getent hosts app | head -1 | cut -d' ' -f1) -p tcp --dport 50010 -j REJECT
  quiet rm random.get
  quiet $HADOOP/bin/hdfs dfs -get /random random.get
  if [ $? -ne 0 ]; then
    echo "failed"
    echo "Couldn't read file back from data2"
    return
  fi
  if ! (diff random random.get >/dev/null 2>&1); then
    echo "failed"
    echo "The data on data2 is incorrect"
    return
  fi
  echo "ok"

  echo "Recovery OK (15 pts)"
  score=$(($score + 15))
}

test_recovery_clean() {
  rm random random.get >/dev/null 2>&1
  quiet $HADOOP/bin/hdfs dfs -rm /random
  quiet $SSH name sudo iptables -F INPUT
  quiet $SSH data1 sudo iptables -F INPUT
  quiet $SSH data2 sudo iptables -F INPUT
}

if [ -f app_public_ip ]; then
  compile
  safe_run $SCP namenode datanode lock_server extent_server yfs_client test-lab4-part3.sh test-lab4-common.sh cse@$(cat app_public_ip): >/dev/null 2>&1
  remote_grade /home/cse/test-lab4-part3.sh
  ret=$?
  safe_run $SCP cse@$(cat app_public_ip):*.log ./ >/dev/null 2>&1
  safe_run $SSH cse@$(cat app_public_ip) rm namenode datanode yfs_client lock_server extent_server test-lab4-part3.sh test-lab4-common.sh *.log >/dev/null 2>&1
  exit $ret
fi

trap "atexit 1" INT

stop_hdfs
stop_yfs
send_yfs
start_yfs
check_report
check_heartbeat
check_replica
check_replica_clean
test_recovery
test_recovery_clean
stop_yfs_quiet
print_score
