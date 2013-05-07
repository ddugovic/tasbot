./playfun --helper 8000 &
./playfun --helper 8001 &
./playfun --helper 8002 &
./playfun --helper 8003 &
./playfun --helper 8004 &
sleep 1
./playfun --master 8000 8001 8002 8003 8004 &
