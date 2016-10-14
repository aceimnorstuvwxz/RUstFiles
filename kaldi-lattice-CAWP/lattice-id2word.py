#!/usr/bin/env python
# -*- coding: utf-8 -*-


import sys

gWordMap = {}

with open('words.txt', 'r') as wordsf:
    lines = wordsf.readlines()
    for l in lines:
        segs = l.split(' ')
        gWordMap[segs[1][:-1]] = segs[0] #remove \n
    print "load word map", len(gWordMap)

def id2word(ifn,ofn):
    with open(ifn, 'r') as fin:
        inlines = fin.readlines()
        with open(ofn, 'w') as fout:
            for line in inlines[1:]:
                segs = line.split('\t')
                outl = line
                if len(segs) > 2:
                    global gWordMap
                    wd = "OOV"
                    #print len(gWordMap), gWordMap['19600']
                    if gWordMap.has_key(segs[2]):
                        wd = gWordMap[segs[2]]
                    outl =  wd + '   ' + line
                fout.write(outl)



if __name__ == "__main__":
    USAGE = 'lattice-id2word.py input.fn output.fn'
    if len(sys.argv) != 3:
        print USAGE
    else:
        id2word(sys.argv[1], sys.argv[2])