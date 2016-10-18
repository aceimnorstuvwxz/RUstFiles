

./LKA.py $1 $1.newlat.txt $2
# lattice-1best --acoustic-scale=0.1 ark:$1 ark,t:-

lattice-1best --acoustic-scale=0.1 ark:$1.newlat.txt ark,t:a.txt
./lattice-id2word.py a.txt $1.lka.txt
cat $1.lka.txt
