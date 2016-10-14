这份材料用来实现“KWS用户词典”的功能预研

我在家闲的科技城的工作.wav的原始录音内容是“我在嘉兴的科技城工作”，而“我在家闲的科技城的工作”是原始识别结果。
lattice-id2word.py 可以text版本的lattice中的word-id转成 汉字。
lat.JOB是原始的音频"我在家闲的科技城的工作.wav"的识别结果lattice的bin版。
lat.txt是lat.JOB用kaldi的lattice-copy转换成text版本的结果。
lat_manual.txt是在lat.txt中修改”加薪“的cost，减去7，之后的版本。
1best_manual.txt是用kaldi的lattice-1best，对lat_manual.txt计算的结果。
1best_manual.cv.txt是1best_manual.txt经过lattice-id2.word.py转换的结果，结果是“我在加薪的科技城的工作”。
words.txt是word/word-id列表。

经过调整lattice中部分word的cost之后，识别结果改变了。
因为嘉兴不在Lattice的覆盖里面，并且嘉兴也不在words.txt的词表中，因而我们可以用叫高的”加薪“来做Word Proxy。
在调整Lattice的word cost和Word Proxy的共同作用下，最终的识别结果变成了”我在嘉兴的科技城的工作“，与实际语音内容仅多了最后一个“的”字，是较好的结果。
当然要把Lattice的cost-adjust + word-proxy方法做到实际的工程项目中，还有一些问题需要结果。 
