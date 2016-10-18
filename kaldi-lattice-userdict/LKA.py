#!/usr/bin/env python
# -*- coding: utf-8 -*-

'''
LKATPM: LatticeKeywordAlign TextPhoneMapping
'''

import sys
import jieba
import datetime

gWordMap = {}

AMSCORE_IMPROVE_STEP = 10.0 #lat rescore时，arc的声学提高幅度
KW_EARLY_STOP_THRESH = 20000 #当一个kw符合的次数超过一定量之后，就pass了。 only for lka

with open('words.txt', 'r') as wordsf:
    lines = wordsf.readlines()
    for l in lines:
        l = l.decode('utf-8')
        segs = l.split(' ')
        gWordMap[int(segs[1][:-1])] = segs[0] #remove \n
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

def text2simplePinyin(text):
    ret = []
    ww = userSegment2words(text)
    for w in ww:
        ret.extend(fullPinyin2simplePinyin(word2fullPinyin(w)))
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
            
def wordId2word(wordid):
    '''wordid string'''
    return gWordMap[wordid]

def wordId2simplePinyin(wordid):
    return fullPinyin2simplePinyin(word2fullPinyin(wordId2word(wordid)))

def simplePinyinComp(kwspy, wordspy):
    #头部是对齐match的，允许wordspy的长度超过kwspy #更正 不允许

    if len(kwspy) < len(wordspy):
        return False
    for p in xrange(len(wordspy)):
        if kwspy[p] != wordspy[p]:
            return False
    
    return True


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


def loadLat(latf):
    states = {}
    with open(latf, 'r') as f:
        lines = f.readlines()[1:] #去掉第一行的标题
        for l in lines:
            l = l[:-1] #去掉\n
            if len(l) < 1:
                continue
            ww = l.split('\t')
            # print ww
            start = int(ww[0])
            if len(ww) < 4:
                states[start] = {}
            else:
                des = int(ww[1])
                content = {}
                content['wordid'] = int(ww[2])
                lefts = ww[3].split(',')
                content['amscore'] = float(lefts[0])
                content['lmscore'] = float(lefts[1])
                content['aligns'] = lefts[2]
                content['improve'] = 0
                if not states.has_key(start):
                    states[start] = {}
                
                states[start][des] = content
    return states
            
def saveLat(lat, fn):
    with open(fn, 'w') as f:
        f.write('T00_001\n')
        for srcState, desObj in lat.iteritems():
            if len(desObj) < 1:
                f.write('%d\n' % srcState)
            else:
                items = desObj.items()
                items.sort()
                for desState, arcObj in items:
                    l = "%d\t%d\t%d\t%f,%f,%s\n" %(srcState, desState, arcObj['wordid'], arcObj['amscore'], arcObj['lmscore'], arcObj['aligns'])
                    f.write(l)
                
                



def rescoreLat(lat, state, kwobj, kwstat):
    # print "visit lat", state, kwspy, kwstat

    #early stop
    if kwobj['n'] > KW_EARLY_STOP_THRESH:
        return 0

    kwspy = kwobj['kwspy']
    currentStat = lat[state]
    preImproveLen = 0

    #对每个arc进行处理
    for nextStat in currentStat.keys():
        nextStatParam = currentStat[nextStat]
        
        needImproveThisArc = False
        nextKwStat = [0]

        #arc的spinyin
        wordspy = wordId2simplePinyin(nextStatParam['wordid']) 
        
        #在kwstat下，arc的spinyin与kwspy进行计算
        for pystart_pos in kwstat:
            if simplePinyinComp(kwspy[pystart_pos:], wordspy): 
                #[IMPORTANT]不考虑，word的尾与kw的前部Match的情况，主要考虑后缀match的时间花费大
                #支持 abc  abcd   不支持 abcd ffffabc这种在pystart_pos==0时应该支持得尾部部分match。
                new_pystart_pos = pystart_pos + len(wordspy)
                
                if new_pystart_pos >= len(kwspy):
                    #如果对于一个kwstat的position与wordspy 符合了
                    #如果这个kw不再有剩余，那么这个kw就完全hit了
                    needImproveThisArc = True #本arc要提升
                    preImproveLen = max(preImproveLen, pystart_pos) #之前还有要一些提升
                    kwobj['n'] = kwobj['n'] + 1
                    # global gHitTime
                    # gHitTime = gHitTime + 1
                
                else:
                    #还没有完全hit，要接下来继续比较
                    nextKwStat.append(new_pystart_pos)
            else:
                #不match，则废了
                pass
        
        #对arc之后的
        ret = rescoreLat(lat, nextStat, kwobj, nextKwStat)
        if needImproveThisArc or ret > 0:
            #本层要求或者之后面的要求 提升本arc
            # nextStatParam['amscore'] = nextStatParam['amscore'] - AMSCORE_IMPROVE_STEP
            nextStatParam['improve'] = nextStatParam['improve'] + 1
            #print "lat improve", state, nextStat, wordId2word(nextStatParam['wordid']), nextStatParam['improve']
            
            retleft = ret - len(wordspy)
            preImproveLen = max(preImproveLen, retleft)
    
    #告诉上层，还需要提升多少长度
    return preImproveLen


def lka(inlat, outlat, kws):
    '''直接在state图上遍历，很慢，比lka2慢10x'''
    print inlat, outlat, kws
    now = datetime.datetime.now()
    latin = loadLat(inlat)
    delta = datetime.datetime.now() - now
    now = datetime.datetime.now()
    print 'loadlat', delta

    for kw in kws:
        kwspy = text2simplePinyin(kw)
        rescoreLat(latin, 0, {'kwspy':kwspy,'n': 0}, [0])

    delta = datetime.datetime.now() - now
    now = datetime.datetime.now()
    print 'rescore', delta

    saveLat(latin, outlat)

    delta = datetime.datetime.now() - now
    now = datetime.datetime.now()
    print 'savelat', delta




def latNetSearch(lat, state, kwspy, kwpos):
    currentStatObj = lat[state]
    preImproveLen = 0

    for nextStat, nextStatObj in currentStatObj.iteritems():
        
        needImproveThisArc = False
        arcspy = wordId2simplePinyin(nextStatObj['wordid'])

        if simplePinyinComp(kwspy[kwpos:], arcspy):
            new_kw_pos = kwpos + len(arcspy)

            if new_kw_pos >= len(kwspy):
                needImproveThisArc = True
                preImproveLen = max(preImproveLen, kwpos)
            else:
                ret = latNetSearch(lat, nextStat, kwspy, new_kw_pos)
                if ret > 0:
                    needImproveThisArc = True
                    preImproveLen = max(preImproveLen, ret - len(arcspy))
        
        if needImproveThisArc:
            nextStatObj['improve'] = nextStatObj['improve'] + 1
            # print 'improve', state, nextStat, wordId2word(nextStatObj['wordid'])
        
    return preImproveLen


def arcSearch(lat, kwspy):
    '''先直接在arc序列上遍历，出现部分match之后，再到lat state net上核实'''
    for srcState, desObj in lat.iteritems():
        for desState, arcObj in desObj.iteritems():
            #arc 的spy
            arcSpy =  wordId2simplePinyin(arcObj['wordid'])
            needImproveThisArc = False
            if simplePinyinComp(kwspy, arcSpy): #simplePyinComp的情况同上解释
                #如果在arc的遍历中出现了部分的Match，转到lat state上运行
                if len(kwspy) <= len(arcSpy):
                    #该arc的word已经cover了kw， kw: abc arc:abcde
                    #就不需要再去lat state net上了
                    needImproveThisArc = True
                else:
                    leftn = latNetSearch(lat, desState, kwspy, len(arcSpy))
                    if leftn > 0:
                        needImproveThisArc = True
            
            if needImproveThisArc:
                arcObj['improve'] = arcObj['improve'] + 1
                # print 'improve', srcState, desState, wordId2word(arcObj['wordid'])

def latBatchImprove(lat):
    '''对每个arc，针对improve的值的大小，进行rescoring'''
    for srcState, desObj in lat.iteritems():
        for desState, arcObj in desObj.iteritems():
            imp = arcObj['improve']
            if imp > 0:
                arcObj['amscore'] = arcObj['amscore'] - AMSCORE_IMPROVE_STEP*len(wordId2word(arcObj['wordid'])) #不同长度不同比重，不然会趋向于 散的词
                print 'improve', srcState, desState, wordId2word(arcObj['wordid']), imp

def lka2(inlat, outlat, kws):
    print 'lka2', inlat, outlat, kws
    lat = loadLat(inlat)

    now = datetime.datetime.now()

    for kw in kws:
        kwspy = text2simplePinyin(kw)
        arcSearch(lat, kwspy)
    
    latBatchImprove(lat)

    delta = datetime.datetime.now() - now
    now = datetime.datetime.now()
    print 'arcSearch', delta

    saveLat(lat, outlat)

if __name__ == "__main__":
    load_lexicon_dict()

    print text2simplePinyin(u'你好啊我的哥哥')
    print wordId2simplePinyin(18137)
    print simplePinyinComp([u'a',u'b', u'd'],[u'a',u'b',u'c'])

    USAGE = 'lka.py input.lat output.lat kw0 kw1 kw2 kw3 ...'
    if len(sys.argv) < 4:
        print USAGE
    else:
        # lka(sys.argv[1], sys.argv[2], sys.argv[3:])
        lka2(sys.argv[1], sys.argv[2], sys.argv[3:])

        
        pass
    # showUniqueShengmu()


