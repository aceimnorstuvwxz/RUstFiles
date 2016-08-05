#!/usr/bin/env python
# -*- coding: utf-8 -*-
import socket  
  
address = ('127.0.0.1', 8080)  
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)  
s.connect(address)  

s.send('hihi\n')  
data = s.recv(512)  
print 'the data received is',data  
  
s.send('hihi')  
  
s.close()  
