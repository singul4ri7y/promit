#/bin/python

import time

print("Enter the number of fibonacci: ", end = '')

x = int(input())

def fib(n):
    if n < 2: 
        return n

    return fib(n - 1) + fib(n - 2)

def getPosition(n): 
    if n == 1: 
        return 'st'
    elif n == 2:
        return 'nd'
    elif n == 3:
        return 'rd'

    return 'th'

start_time = time.time()

print("The %s%s fibonacci number is: %s!" % (x, getPosition(x), fib(x)))
print("Time took: %s seconds." % (time.time() - start_time))
