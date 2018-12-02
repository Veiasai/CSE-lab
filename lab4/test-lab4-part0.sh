#!/bin/bash

. test-lab4-common.sh

check_network() {
  # Check hostname
  echo "Check hostname..."
  myname=$(hostname)
  if [[ "x$myname" != "xapp" && "x$myname" != "xname" && "x$myname" != "xdata1" && "x$myname" != "xdata2" ]]; then
    echo "You didn't setup host name correctly"
    exit 1
  fi

  # Check hosts
  echo "Check /etc/hosts..."
  if ! (grep -v "^#" /etc/hosts | grep app >/dev/null); then
    echo "You didn't setup /etc/hosts correctly"
    exit 1
  fi
  if ! (grep -v "^#" /etc/hosts | grep name >/dev/null); then
    echo "You didn't setup /etc/hosts correctly"
    exit 1
  fi
  if ! (grep -v "^#" /etc/hosts | grep data1 >/dev/null); then
    echo "You didn't setup /etc/hosts correctly"
    exit 1
  fi
  if ! (grep -v "^#" /etc/hosts | grep data2 >/dev/null); then
    echo "You didn't setup /etc/hosts correctly"
    exit 1
  fi

  # Check self ip
  echo "Check self IP resolution..."
  myip=$(grep -v "^#" /etc/hosts | grep "$myname" | grep -o -E "[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+")
  mynic=$(ip link | grep -o -P "(?<=: ).*(?=:)" | grep -v lo)
  mynicip=""
  for nic in $mynic; do
    mynicip="$mynicip "$(ip addr show dev $nic | grep -o -P "(?<=inet )[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+")
  done
  if ! (echo $mynicip | grep -F "$myip" >/dev/null); then
    echo "Resolved IP ($myip) of your host name ($myname) is not your IP"
    echo "You should check your /etc/hosts"
    exit 1
  fi

  # Check connectivity
  echo "Check connectivity..."
  for target in app name data1 data2; do
    if ! (ping -c 1 -w 3 $target >/dev/null 2>&1); then
      echo "Can't connect to $target"
      echo "You should check $myname and $target's network configuration"
      exit 1
    fi
  done

  if [ "x$myname" == "xapp" ]; then
    # Check passwordless SSH
    echo "Check passwordless SSH..."
    for target in app name data1 data2; do
      if ! ($SSH $target /bin/bash -c exit); then
        echo "Can't ssh to $target without password"
        echo "You should check the ssh key on $myname and authorized_keys on $target"
        exit 1
      fi
    done

    # Check TA key
    echo "Check TA's public key..."
    if ! (grep "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQDPYzrRrs4AKIaEK8AxZxqCHRb1LwxBpRz61zfsw3U9zPcyyPxruiz8sK7ph9+r7Z0dA6rfJY6suO/uQosC5X7NtW0zvplePm4GzxEPKBWtGYC9H3lwU8hZBXhgcNRphRPCITImC4d7/TB+qib7cBzlRO2F9y4k2OcQRnNAiHdq8vmdvxptiT/345F732Ijqyi5hjKE66bIufcmiJi/GugqeBgs8oPnWRMCedzVs17E59nbOUIYqJLQgsz1HD/KGR9Pu3niuTd6djze9c7sFfLQRNZL5TqeWGCPXbYAQYgQMQ/angtsLdKwnh+6Cn5xgwSPBlblbSKiRfD9qkOtx2AH" ~/.ssh/authorized_keys >/dev/null 2>&1); then
      echo "You didn't add TA's public key to your authorized_keys."
      exit 1
    fi

    # Check IP correctness
    echo "Check IP correctness..."
    for target in app name data1 data2; do
      report_name=$($SSH $target /bin/hostname 2>/dev/null)
      if [ "x$report_name" != "x$target" ]; then
        echo "SSH to $target but it reports \"I'm $report_name\""
        exit 1
      fi
    done
  fi

  if [ "x$myname" != "xapp" ]; then
    echo "$myname pass"
    exit 0
  else
    # Check on other host
    echo "Check basic configuration on other hosts..."
    for target in name data1 data2; do
      quiet $SCP test-lab4-common.sh test-lab4-part0.sh $target:
      result=$(quiet_err $SSH $target /home/cse/test-lab4-part0.sh)
      quiet $SSH $target rm test-lab4-common.sh test-lab4-part0.sh
      if ! (echo "$result" | grep "$target pass" >/dev/null); then
        echo "$result" | sed "s/^/$target: /"
        exit 1
      fi
    done
  fi
}

if [ -f app_public_ip ]; then
  safe_run $SCP test-lab4-common.sh test-lab4-part0.sh cse@$(cat app_public_ip):
  remote_grade /home/cse/test-lab4-part0.sh
  ret=$?
  safe_run $SSH cse@$(cat app_public_ip) rm test-lab4-common.sh test-lab4-part0.sh
  exit $ret
fi

check_network
