#!/usr/bin/env python
# -*- coding: utf-8 -*-

'''
LKATPM: LatticeKeywordAlign TextPhoneMapping
'''

import sys
import jieba

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

def userSegment2words(text):
    '''
    用户输入的字符串，分词，如果词在lexicon中，则，否则分成单个字。
    unicode
    '''
    ww = jieba.cut(text, cut_all=False)
    leftww = []
    for w in ww:
        if gLexiconDict.has_key(w):
            leftww.append(w)
        else:
            for c in w:
                if gLexiconDict.has_key(c):
                    leftww.append(c)
                else:
                    print("ERROR word not exist in lexicon %s" % c)
    
    return leftww

def word2fullPinyin(word):
    '''
    返回 浙江[u'zh', u'e4', u'j', u'iang1']
    '''
    pinyin = gLexiconDict[word]
    return gLexiconDict[word].split(' ')

gShengmuMap = {
'aa':'aa',
'ch':'c',#
'zh':'z',#
'ee':'ee',
'ii':'ii',
'vv':'vv',
'r':'r',
'oo':'oo',
'c':'c',
'b':'b',
'd':'d',
'g':'g',
'f':'f',
'uu':'uu',
'h':'h',
'k':'k',
'j':'j',
'm':'m',
'l':'l',
'n':'n',
'q':'q',
'p':'p',
's':'s',
'sh':'s',#
't':'t',
'x':'x',
'z':'z'
}

gYunmuMap = {
'iy':'iy',
'ix':'iy',#
'en':'en',
'ei':'ei',
've':'ve',
'ai':'ai',
'uan':'uan',
'iu':'iu',
'ong':'ong',
'ao':'ao',
'an':'an',
'uai':'uai',
'ang':'ang',
'iong':'iong',
'in':'in',
'ia':'ia',
'ing':'in',#
'ie':'ie',
'er':'er',
'iao':'iao',
'ian':'ian',
'van':'van',
'eng':'en',#
'iang':'iang',
'ui':'ui',
'iz':'iy',#
'vn':'vn',
'uang':'uang',
'a':'a',
'ueng':'en',#
'e':'e',
'i':'iy',#
'o':'o',
'uo':'uo',
'un':'un',
'u':'u',
'v':'v',
'ou':'ou',
'ua':'ua',
}

def shengmuMapping(shengmu):
    '''
    声母的简化
    '''
    return gShengmuMap[shengmu]


def yunmuMapping(yunmu):
    '''
    韵母的简化
    '''
    return gYunmuMap[yunmu]

def fullPinyin2simplePinyin(pinyinlist):
    '''
    浙江 [u'zhe', u'jiang']
    '''
    ret = []
    for i in xrange(len(pinyinlist)/2):
        sm = pinyinlist[i*2]
        ym = pinyinlist[i*2 + 1]
        sm = shengmuMapping(sm)
        ym = yunmuMapping(ym[:-1])
        ret.append(sm + ym) #去掉声调数字
    return ret



def showUniqueShengmu():
    '''查看 声母 韵母 表'''
    sms = {}
    yms = {}
    for k,v in gLexiconDict.iteritems():
        pys = v.split(' ')
        if len(pys) % 2 != 0:
            print pys
        for i in  xrange(len(pys)/2):
            sm = pys[i*2]
            ym = pys[i*2+1]
            ym = ym[:-1]  #去掉声调数字
            if sms.has_key(sm):
                sms[sm] = sms[sm] + 1
            else:
                sms[sm] = 1

            if yms.has_key(ym):
                yms[ym] = yms[ym] + 1
            else:
                yms[ym] = 1
    

    for k,v in sms.iteritems():
        print k

    print '-----------' 
    for k,v in yms.iteritems():
        print "'%s':''," % k
            



#音素
gLexiconDict = {}

def load_lexicon_dict():
	with open('lexicon.txt', 'r') as fp:
		cc = fp.readlines()
		cc = [c.decode('utf-8') for c in cc]
		for c in cc :
			k = c.find(' ')
			k, v= c[0:k], c[k+1:-1] #-1去掉换行
			gLexiconDict[k] = v



if __name__ == "__main__":
    USAGE = 'lattice-id2word.py input.fn output.fn'

    load_lexicon_dict()
    # showUniqueShengmu()



    ws = userSegment2words(u'你好啊我的哥浙江')
    for w in ws:
        fpy =  word2fullPinyin(w)
        spy = fullPinyin2simplePinyin(fpy)
        print fpy, spy

    # if len(sys.argv) != 3:
        # print USAGE
    # else:
        # id2word(sys.argv[1], sys.argv[2])