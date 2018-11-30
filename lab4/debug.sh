killall lock_server
if [ -d debug ]; then
echo ""
else
mkdir debug
fi
rm -f ./debug/*
for ((i=1;i<=50;i++)); do
    # export RPC_LOSSY=5
    # ./lock_server 7496 > ./debug/server_$i.log &
    # ./lock_tester 7496 > ./debug/client_$i.log
    # killall lock_server
    # echo "$i done!"
    ./stop.sh
    ./start.sh 5
    ./test-lab-3-a yfs1 yfs2
done
