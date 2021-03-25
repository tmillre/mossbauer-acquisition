import serial
import numpy as np
import os

class Mossbauer:
    def __init__(self, COM):
        self.pic = serial.Serial(COM, baudrate=38400, timeout = 1)
        self.dataLength = 512
        self.data = np.zeros(self.dataLength)
        self.status = self.getStatus()
        if self.status == None:
            raise 'Pic is not connected. Check the connections'
        self.data = self.getData()
        if self.data == None:
            raise 'Pic is not connected. Check the connections'
        if len(self.data) == 1024:
            self.dataLength = 1024
        elif len(self.data) == 2048:
            self.dataLength = 2048
        self.spectrumName = ''
        self.spectrumNum = 'found'
        
    def getStatus(self):
        resp = b'placeholderText'
        counter = 0
        while(resp[0:3] != b'RUN' and resp[0:4] != b'STOP'):    #wait until valid response
            self.pic.write(b"STAT")                   #ask for the STATus
            resp = self.pic.readline()                 #grab the response
            if (resp == b'RUN'):
                return True
            elif (resp == b'STOP'):
                return False
            elif resp ==b'':
                return None
                
    def getData(self):
        data = None
        while(data == None):
            self.pic.write(b'SEND')
            data = self.pic.readline().decode('utf-8')
            if data == b'':
                return None
            elif data[0:3] == 'NOP': #if command misheard, loop again
                continue
            data.strip()
            data = data.split(',')
            #If any strange characters, assume its an error
            try:
                intlist = [float(x) for x in data]
            except ValueError:
                return None
        return intlist
    
    def saveSpectrum(self):
        '''
        Save spectrum as .R filetype, defined by 1 title line, then 8 numbers 
        per line.
        '''
        rFormIndex = 0
        datalist = ['[' + self.spectrumNum + ']' +  self.spectrumName + ' ' + '\n']
        for item in self.data:
            if rFormIndex < 8:
                rFormIndex += 1
                datalist.append("{:7.0f}".format(item) + '. ')
            else:
                rFormIndex = 0
                datalist.append("{:7.0f}".format(item) + '.\n')
        path = './DATA'
        savepath=os.path.join(path, self.specnum+'.R')
        savefile=open(savepath,'w')          #open a file to store the data. Overwrites existing file
        savefile.write(''.join(datalist))
        savefile.close()
        
        
    def changeName(self, name):
        self.spectrumName = name
        
    def changeNumber(self, number):
        self.spectrumNum = number
        
    def startSpectrum(self):
        if self.status:
            raise 'Error. Spectrum is already started'
        elif self.spectrumName == '' or self.spectrumName == 'found':
            raise 'Please provide name and number of spectrum'
        else:
            resp = b'NOP'
            while(resp == b'NOP'):
                self.pic.write(b'STAR')
                resp = self.pic.readline()
                print(resp)
                if resp == '':
                    raise 'Pic is not connected. Check the connections'
            self.status = True
            
    def stopSpectrum(self):
        if not self.status:
            raise 'Error. Spectrum is already stopped'
        else:
            resp = b'NOP'
            while(resp == b'NOP'):
                self.pic.write(b'STOP')
                resp = self.pic.readline()
                if resp == '':
                    raise 'Pic is not connected. Check the connections'
            self.status = False
     
    def clearSpectrum(self):
        if self.status:
            raise 'Error. You must stop measurement before clearing data'
        else:
            resp = b'NOP'
            while(resp == b'NOP'):
                self.pic.write(b'CLER')
                resp = self.pic.readline()
            self.data = np.zeros(512)
            
    def changeChannelNumber(self, chan):
        resp = b'NOP'
        if chan not in ['512', '1024']:
            raise 'Error. Must pick either 512 or 1024 for channels'
        while(resp == b'NOP'):
            if chan == '512':
                self.pic.write(b'0512')
                self.dataLength = 512
            elif chan == '1024':
                self.pic.write(b'1024')
                self.dataLength = 1024
