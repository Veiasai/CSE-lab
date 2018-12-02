#!/bin/bash

PART=1

. test-lab4-common.sh

TOTAL_SCORE=$((5 + 10))

check_hdfs_param_on() {
  result=$($SSH $1 $HADOOP/bin/hdfs getconf -confKey $2 2>/dev/null)
  if [ "x$result" != "x$3" ]; then
    echo "failed"
    echo "$2 on $1 is incorrect"
    exit 1
  fi
}

check_hdfs_param() {
  echo -n "Check HDFS configuration... "
  check_hdfs_param_on app fs.default.name hdfs://name:9000
  check_hdfs_param_on app dfs.block.size 16384
  check_hdfs_param_on app dfs.replication 2
  check_hdfs_param_on app dfs.client.max.block.acquire.failures 0

  check_hdfs_param_on name fs.default.name hdfs://name:9000
  check_hdfs_param_on name dfs.namenode.heartbeat.recheck-interval 0
  check_hdfs_param_on name dfs.name.dir /home/cse/hadoop-data
  check_hdfs_param_on name dfs.replication 2
  check_hdfs_param_on name dfs.namenode.fs-limits.min-block-size 16384
  check_hdfs_param_on name dfs.block.size 16384
  check_hdfs_param_on name dfs.heartbeat.interval 1

  check_hdfs_param_on data1 fs.default.name hdfs://name:9000
  check_hdfs_param_on data1 dfs.data.dir /home/cse/hadoop-data
  check_hdfs_param_on data1 dfs.namenode.fs-limits.min-block-size 16384
  check_hdfs_param_on data1 dfs.block.size 16384
  check_hdfs_param_on data1 dfs.heartbeat.interval 1

  check_hdfs_param_on data2 fs.default.name hdfs://name:9000
  check_hdfs_param_on data2 dfs.data.dir /home/cse/hadoop-data
  check_hdfs_param_on data2 dfs.namenode.fs-limits.min-block-size 16384
  check_hdfs_param_on data2 dfs.block.size 16384
  check_hdfs_param_on data2 dfs.heartbeat.interval 1
  echo "ok (5 pts)"
  exit 0
}

cleanup() {
  # Do cleanup
  echo -n "Cleanup... "
  for target in name data1 data2; do
    $SSH $target pkill java >/dev/null 2>&1
    $SSH $target rm -rf /home/cse/hadoop-data >/dev/null 2>&1
    $SSH $target rm -rf $HADOOP/logs/* >/dev/null 2>&1
  done

  # Format namenode
  result=$($SSH name bash -c "\"$HADOOP/bin/hdfs namenode -format && echo pass\"" 2>&1)
  if ! (echo "$result" | grep "^pass" >/dev/null); then
    echo "failed"
    echo "$result" | sed "s/^/  name:/"
    echo "Format namenode failed"
    echo "You should check hadoop-env.sh and hdfs-site.xml"
    print_score
    exit 1
  fi

  # Check namenode meta
  result=$($SSH name bash -c "\"if [ -f /home/cse/hadoop-data/current/VERSION ]; then echo pass; fi\"" 2>&1)
  if ! (echo "$result" | grep "^pass" >/dev/null); then
    echo "failed"
    echo "Namenode data directory not exist"
    echo "You forget to specify namenode data directory?"
    print_score
    exit 1
  fi
  echo "done"
}

wait_dead() {
  timeout=$(($2 + 1))
  while ! ($HADOOP/bin/hdfs dfsadmin -report -dead 2>&1 | grep $1 >/dev/null 2>&1); do
    timeout=$(($timeout - 1))
    if [ $timeout -eq 0 ]; then
      return 1
    fi
    sleep 1
  done
  return 0
}

crash_data1() {
  echo -n "Crash data1... "
  safe_run stop_data1
  safe_run $SSH data1 rm -rf /home/cse/hadoop-data
  if ! wait_dead data1 10; then
    echo "failed"
    return 1
  fi
  echo "done"
  return 0
}

recovery_data1() {
  echo "Restart data1..."
  safe_run $SSH data1 $HADOOP/sbin/hadoop-daemon.sh start datanode
}

crash_data2() {
  echo -n "Crash data2... "
  safe_run stop_data2
  safe_run $SSH data2 rm -rf /home/cse/hadoop-data
  if ! wait_dead data2 10; then
    echo "failed"
    return 1
  fi
  echo "done"
  return 0
}

recovery_data2() {
  echo "Restart data2..."
  safe_run $SSH data2 $HADOOP/sbin/hadoop-daemon.sh start datanode
}

wait_recovery() {
  timeout=$(($1 + 1))
  echo -n "Wait for recovery... "
  while [ 1 ]; do
    timeout=$(($timeout - 1))
    if [ $timeout -eq 0 ]; then
      echo "timeout"
      return 1
    fi
    result=$($HADOOP/bin/hdfs fsck / -blocks 2>/dev/null | grep -P -o "(?<=Under-replicated blocks:\t)[0-9]+")
    if [ $? -ne 0 ]; then
      echo "failed"
      return 1
    fi
    if [ "x$result" == "x0" ]; then
      break
    fi
    sleep 1
  done
  echo "done"
  return 0
}

check_recovery() {
  echo -n "Check whether HDFS recovered... "
  rm -rf random.get >/dev/null 2>&1
  quiet $HADOOP/bin/hdfs dfs -get /random random.get
  if [ $? -ne 0 ]; then
    echo "no"
    echo "Fetch data from HDFS failed"
    return
  fi
  if (diff random random.get >/dev/null); then
    echo "yes (10 pts)"
    score=$(($score + 10))
    return
  else
    echo "no"
    echo "Data in HDFS is corrupt"
    return
  fi
}

check_dead() {
  echo -n "Check whether HDFS is completely corrupted... "
  result=$($HADOOP/bin/hdfs fsck / -blocks 2>/dev/null | grep -P -o "(?<=Minimally replicated blocks:\t)[0-9]+")
  if [ "x$result" != "x0" ]; then
    echo "no"
    echo "Some blocks are still available after two datanodes crash?"
    score=0
    print_score
    exit 1
  fi
  echo "yes"
}

import_random() {
  echo -n "Import random file into HDFS... "
  quiet dd if=/dev/urandom of=random bs=4096 count=13
  quiet $HADOOP/bin/hdfs dfs -put random /random
  if [ $? -ne 0 ]; then
    echo "failed"
    return 1
  fi
  echo "ok"
  return 0
}

if [ -f app_public_ip ]; then
  safe_run $SCP test-lab4-part1.sh test-lab4-common.sh cse@$(cat app_public_ip): >/dev/null 2>&1
  remote_grade /home/cse/test-lab4-part1.sh
  ret=$?
  safe_run $SSH cse@$(cat app_public_ip) rm test-lab4-part1.sh test-lab4-common.sh >/dev/null 2>&1
  exit $ret
fi

stop_yfs
if (check_hdfs_param); then
  score=$(($score + 5))
fi
cleanup
start_hdfs
if import_random; then
  if crash_data2; then
    recovery_data2
    if wait_recovery 60; then
      if crash_data1; then
        check_recovery
      fi
      crash_data2
      recovery_data1
      recovery_data2
      check_dead
    fi
  fi
fi
rm -f random random.get >/dev/null 2>&1
stop_hdfs
print_score
