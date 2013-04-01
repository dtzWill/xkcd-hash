#!/usr/bin/env python3

import skein
from multiprocessing import Process, Value
import string
import random
import threading

from gmpy import hamdist

goal = '5b4da95f5fa08280fc9879df44f418c8f9f12ba424b7757de02bbdfbae0d4c4fdf9317c80cc5fe04c6429073466cf29706b8c25999ddd2f6540d4475cc977b87f4757be023f19b8f4035d7722886b78869826de916a79cf9c94cc79cd4347d24b567aa3e2390a573a373a48a5e676640c79cc70197e1c5e7f902fb53ca1858b6'
int_goal=int(goal,16)

best_global = Value('i',100000)

def randstr():
  return '%040x' % random.randrange(256**20)

class Thread(Process):
    def __init__(self):
        Process.__init__(self)
        self.best = 1000000000

    def test(self,b):
        x=skein.skein1024(b)
        hex_x = x.hexdigest()
        int_x = int(hex_x,16)
        dist = hamdist(int_x, int_goal)

        if dist < self.best:
            self.best = dist

            if dist < best_global.value:
                best_global.value = dist
                print("{} bits - {} - hashes to {}".format(dist,b,hex_x))

        if hex_x == goal:
            print('omg')
            print(b)
            print(repr(b))
            quit()

    def testiter(self):
        t = randstr()
        self.test(bytes(t,'ascii'))

    def run(self):
        while True:
            self.testiter()



for i in range(16):
    f = Thread()
    f.start()
