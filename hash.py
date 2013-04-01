#!/usr/bin/env python3

import skein
from multiprocessing import Process, Value
import string
import random



from gmpy import hamdist


best = Value('i',1000000000)

goal = '5b4da95f5fa08280fc9879df44f418c8f9f12ba424b7757de02bbdfbae0d4c4fdf9317c80cc5fe04c6429073466cf29706b8c25999ddd2f6540d4475cc977b87f4757be023f19b8f4035d7722886b78869826de916a79cf9c94cc79cd4347d24b567aa3e2390a573a373a48a5e676640c79cc70197e1c5e7f902fb53ca1858b6'
int_goal=int(goal,16)

def test(b):
    x=skein.skein1024(b)
    hex_x = x.hexdigest()
    int_x = int(hex_x,16)
    dist = hamdist(int_x, int_goal)

    if dist < best.value:
        best.value = dist

        print("{} bits - {} - hashes to {}".format(dist,b,hex_x))

    if hex_x == goal:
        print('omg')
        print(b)
        print(repr(b))
        quit()


def randstr():
  return '%040x' % random.randrange(256**20)

def testiter():
    t = randstr()
    test(bytes(t,'ascii'))

class Thread(Process):
    def __init__(self):
        Process.__init__(self)

    def run(self):
        while True:
            testiter()


for i in range(16):
    f = Thread()
    f.start()
