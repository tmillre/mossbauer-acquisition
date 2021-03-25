import serial
import time

import random
import numpy
import numpy.random

import sys
import argparse

class Simulator():
    def __init__(self, COM, stepsize, ratio, failure_rate):
        self.ser = serial.Serial(COM, baudrate=38400, timeout = 1)
        self.start = False
        self.stepSize = stepsize
        self.ratio = ratio
        self.length = 512
        self.data = numpy.zeros(self.length)
        self.target = numpy.zeros(self.length)
        self.fail = failure_rate
        self.sampleMean = 1
        
    def step(self):
        noise = numpy.random.normal(self.stepSize * self.sampleMean, self.stepSize * self.sampleMean, size = (self.length))
        for i in range(self.data.shape[0]):
            self.data[i] += int(self.ratio * int(max(noise[i], 0)) + (1 - self.ratio) * self.stepSize * self.target[i])

    def getData(self, filename):
        dfile = open('DATA/00000.R', 'r')
        self.target = []
        first = True
        for line in dfile:
            if first:
                first = False
                continue
            spl = line.strip().split()
            for num in spl:
                self.target.append(int(float(num)))
        self.target = numpy.asarray(self.target)
        self.sampleMean = max(numpy.median(self.target), 1)
    
    def loop(self):
        while(1):
            c = self.ser.read(4).strip()
            if len(c) > 2:
                print(c)
                p = numpy.random.random()
                if p < self.fail:
                    self.ser.write(b'NOP')
                    continue
                    
                if c == b'STOP':
                    self.start = False
                    self.ser.write(b'ACK')
                    
                elif c == b'SEND':
                    first = True
                    for i in range(self.length):
                        if first:
                            first = False
                        else:
                            self.ser.write(b',')
                        self.ser.write(str(self.data[i]).encode())
                    
                elif c == b'STAT':
                    if self.start:
                        self.ser.write(b'RUN')
                    else:
                        self.ser.write(b'STOP')
                        
                elif c == b'STAR':
                    self.start = True
                    self.ser.write(b'ACK')
                    
                elif c == b'CLER':
                    for i in range(self.length):
                        self.data[i] = 0
                        
                elif c == '0512':
                    if self.start:
                        self.ser.write(b'NACK')
                    else:
                        self.length = 512
                        self.data = numpy.zeros(self.length)
                        self.target = numpy.zeros(self.length)
                        
                    
                elif c == '1024':
                    if self.start:
                        self.ser.write(b'NOP')
                    else:
                        self.length = 1024
                        self.data = numpy.zeros(self.length)
                        self.target = numpy.zeros(self.length)
                else:
                    self.ser.write(b'NOP')
            if self.start:
                self.step()
            time.sleep(0.1)
        
if __name__ == "__main__":

    parser = argparse.ArgumentParser(description='Mossbauer Spectrometer Simulator')
    parser.add_argument('--serialPort', dest="serialPort", default = '/dev/pts/1',
                        help='serialPort: path serial port to talk on')
    parser.add_argument('--filename', dest="filename", default = '',
                        help='filename: path to spectrum to simulate')
    parser.add_argument('--stepsize', dest="stepsize", type=float, default = 0.001,
                        help='stepsize: what percentage of counts are used at each update - default: 0.001')
    parser.add_argument('--fail_rate', dest="failrate", type=float, default = 0,
                        help='fail_rate: how often communication yeilds a NACK reply - default: 0')
    parser.add_argument('--ratio', dest="ratio", type=float, default = 0.3,
                        help='ratio: how strong noise is compared to signal - default: 0.3')


    args = parser.parse_args()
    sim = Simulator('/dev/pts/1', args.stepsize, args.ratio, args.failrate)
    if args.filename!='':
        sim.getData(args.filename)
    sim.loop()
