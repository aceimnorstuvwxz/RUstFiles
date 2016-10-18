./LKA.py lat.txt lat.out.txt 嘉兴 柯季诚 科技城 爱情 嘉呼信息
lattice-1best --acoustic-scale=0.1 ark:lat.out.txt ark,t:a.txt
./lattice-id2word.py a.txt b.txt
cat b.txt
