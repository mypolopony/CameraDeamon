# Python3 Note: The line below is not needed
#from __future__ import print_function
 
import time
from random import choice
from random import randrange
 
import zmq
 
if __name__ == "__main__":
    stock_symbols = ['RAX', 'EMC', 'GOOG', 'AAPL', 'RHAT', 'AMZN']
 
    #publish to socket  4999
    context = zmq.Context()
    socket1 = context.socket(zmq.PUB)
    socket1.bind("tcp://*:4999")

    #listen to socket 4998
    socket2 = context.socket(zmq.SUB)
    socket2.setsockopt_string(zmq.SUBSCRIBE, u'')
    socket2.connect("tcp://169.254.34.2:4998")
 
    while True:
        time.sleep(3)
        # pick a random stock symbol
        stock_symbol = choice(stock_symbols)
        # set a random stock price
        stock_price = randrange(1, 100)
 
        # compose the message
        msg = "{0} ${1}".format(stock_symbol, stock_price)
 
        print("Sending Message: {0}".format(msg))
 
        # send the message
        socket1.send_string(msg)

        try:
            rec_mes = socket2.recv(flags=zmq.NOBLOCK)
            print("Rec Message: {0}".format(rec_mes))
        except zmq.Again as e:
            return({'result':e.message})
            pass
    
        # Python3 Note: Use the below line and comment
        # the above line out
        # socket.send_string(msg)