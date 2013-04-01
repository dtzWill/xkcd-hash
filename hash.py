#!/usr/bin/env python3

import skein
from multiprocessing import Process, Value
import string
import random

import sys

def hamming_distance(s1, s2):
    assert len(s1) == len(s2)
    return sum(ch1 != ch2 for ch1, ch2 in zip(s1, s2))

best = Value('i',1000000000)

def test(b):
    goal = '5b4da95f5fa08280fc9879df44f418c8f9f12ba424b7757de02bbdfbae0d4c4fdf9317c80cc5fe04c6429073466cf29706b8c25999ddd2f6540d4475cc977b87f4757be023f19b8f4035d7722886b78869826de916a79cf9c94cc79cd4347d24b567aa3e2390a573a373a48a5e676640c79cc70197e1c5e7f902fb53ca1858b6'

    binary_goal = bin(int(goal, 16))[2:].zfill(1024)

    x=skein.skein1024(b)
    hex_x = x.hexdigest()
    binary = bin(int(hex_x,16))[2:].zfill(1024)

    dist = hamming_distance(binary, binary_goal)

    if dist < best.value:
        best.value = dist

        print("{} bits - {} - hashes to {}".format(dist,b,hex_x))

    if hex_x == goal:
        print('omg')
        print(b)
        print(repr(b))
        quit()

import itertools

def id_generator(size=25, chars=string.ascii_uppercase + string.ascii_lowercase + string.digits):
    return ''.join(random.choice(chars) for x in range(size))

class Thread(Process):
    def __init__(self):
        Process.__init__(self)

    def run(self):
        while True:
            t = id_generator()
            test(bytes(t,'ascii'))

        #while True:
        #    try:
        #        t = self.queue.get_nowait()
        #        test(t)
        #    except:
        #        pass

            #self.queue.task_done()

for i in range(16):
    f = Thread()
    f.start()


#def bruteforce(charset, maxlength):
#    return (''.join(candidate)
#        for candidate in itertools.chain.from_iterable(itertools.product(charset, repeat=i)
#        for i in range(1, maxlength + 1)))
#
#for attempt in bruteforce(string.ascii_lowercase+string.ascii_uppercase, 30):
#    queue.put(bytes(attempt, 'ascii'))
