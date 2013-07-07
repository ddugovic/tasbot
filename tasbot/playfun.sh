ARGS="--game mario --movie mario.fm2 --fastforward 200"
./learnfun $ARGS
./playfun --helper 8000 $ARGS &
./playfun --helper 8001 $ARGS &
./playfun --helper 8002 $ARGS &
./playfun --helper 8003 $ARGS &
sleep 1
./playfun --master 8000 8001 8002 8003 $ARGS
