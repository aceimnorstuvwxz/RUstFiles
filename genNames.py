#!/usr/bin/env python
# -*- coding: utf-8 -*-
import sys

'''
generate domain names for select
'''
import random, string
def make(num):
    return ''.join(random.sample('aceimnoistuvwxz', num))

if __name__ == "__main__":
    num = 6
    cnt = 10
    lnt = 2
    for j in range(lnt):
        for i in range(cnt):
            print make(num),
        print ""
