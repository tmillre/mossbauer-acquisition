from pyqtgraph.Qt import QtGui, QtCore
from PyQt5 import QtGui
import PyQt5.QtWidgets as QWid
import pyqtgraph as pg
import numpy as np

from moss import Mossbauer

def Errorbox(text):
    alert = QWid.QMessageBox()
    alert.setText(text)
    alert.exec_()
    
    
moss = Mossbauer('/dev/pts/2')


## Always start by initializing Qt (only once per application)
app = QtGui.QApplication([])

window = QWid.QWidget()


#Plotting Window Widget
spectrumPlot = pg.PlotWidget()

spectrumPlot.setWindowTitle(''+'found')
spectrumPlot.setRange(QtCore.QRectF(0, 0, 512, 5))

if moss.data[0] != 0 and moss.status:
    curve = spectrumPlot.plot(pen=None,symbol='x')
    curve.setData(y = moss.data)
    spectrumPlot.setRange(QtCore.QRectF(0, int(min(moss.data)), 512, max(moss.data) - min(moss.data)))
else:
    curve = spectrumPlot.plot(pen=None,symbol='x')
    curve.setData(y = np.zeros(512))
#set axes names
spectrumPlot.setLabel('bottom', 'Channel Number')
spectrumPlot.setLabel('left', 'Counts')


#Channel Widget Box
radio512 = QWid.QRadioButton("512")
radio1024 = QWid.QRadioButton("1024")

channelGroupBox = QWid.QGroupBox()
channelGroupBox.setTitle('Channel Options')

channelLayoutBox = QWid.QVBoxLayout()
channelLayoutBox.addWidget(radio512)
channelLayoutBox.addWidget(radio1024)

channelGroupBox.setLayout(channelLayoutBox)

channelButtons = QtGui.QButtonGroup()
channelButtons.addButton(radio512)
channelButtons.addButton(radio1024)

if moss.dataLength == 512:
    radio512.click()
elif moss.dataLength == 1024:
    radio1024.click()


#Naming and Numbering of spectrum
spectrumNameTextBox = QtGui.QLineEdit(placeholderText = 'Spectrum name')
spectrumNumberTextBox = QtGui.QLineEdit(placeholderText = 'Spectrum number')
spectrumNameButton = QtGui.QPushButton('Rename')

nameGroupBox = QtGui.QGroupBox()
nameGroupBox.setTitle('Name Options')

nameLayoutBox = QtGui.QVBoxLayout()
nameLayoutBox.addWidget(spectrumNameTextBox)
nameLayoutBox.addWidget(spectrumNumberTextBox)
nameLayoutBox.addWidget(spectrumNameButton)

nameGroupBox.setLayout(nameLayoutBox)


#Control Buttons
startRadioButton = QtGui.QRadioButton("Start")
stopRadioButton = QtGui.QRadioButton("Stop")
clearButton = QtGui.QPushButton('Clear')

'''Here we make start and stop mutually exclusive'''
statusButtonGroup = QtGui.QButtonGroup()
statusButtonGroup.addButton(startRadioButton)
statusButtonGroup.addButton(stopRadioButton)

controlGroupBox=QtGui.QGroupBox()
controlGroupBox.setTitle('Control Options')

controlLayoutBox=QtGui.QVBoxLayout()
controlLayoutBox.addWidget(startRadioButton)
controlLayoutBox.addWidget(stopRadioButton)
controlLayoutBox.addWidget(clearButton)

controlGroupBox.setLayout(controlLayoutBox)

if moss.status:
    startRadioButton.click()
else:
    stopRadioButton.click()

#Status Bar
statusMsgBox = QtGui.QTextEdit()
statusMsgBox.setReadOnly(True)
statusMsgBox.setLineWrapMode(0)
statusMsgBox.setText('Status: \n')



grid = QWid.QGridLayout()

## Add widgets to the layout in their proper positions
grid.addWidget(nameGroupBox, 0, 0)   # text edit goes in middle-left
grid.addWidget(controlGroupBox, 1, 0)   # text edit goes in middle-left
grid.addWidget(channelGroupBox, 2, 0)   # button goes in upper-left
grid.addWidget(statusMsgBox, 3, 0)   # button goes in upper-left
grid.addWidget(spectrumPlot, 0, 1, 5, 1)  # plot goes on right side, spanning 3 rows

window.setLayout(grid)
window.setWindowTitle('Mossbauer Spectrum')

dataTimer = QtCore.QTimer()

def clearSpectrum():
    if moss.status == True:
        Errorbox('Error! Measurement must be stopped before clearing data!')
    else:
        moss.clearSpectrum()

def startSpectrum():
    if moss.spectrumName != '' and moss.spectrumNum != 'found':
        moss.startSpectrum()
        dataTimer.start(5000)
    else:
        Errorbox('Please Enter a Name and Number for the Spectrum First')
        stopRadioButton.setChecked(True)

def stopSpectrum():
    moss.stopSpectrum()
    moss.saveSpectrum()
    dataTimer.stop(5000)

def getData():
    newData = moss.getData()
    if newData[10] >= moss.data[10]:
        moss.data = newData
    curve.setData(y = moss.data)
    spectrumPlot.setRange(QtCore.QRectF(0, int(min(moss.data)), 512, max(moss.data) - min(moss.data)))

def renameSpectrum():
    if spectrumNameTextBox.text() != '' and spectrumNumberTextBox.text() != '':
        moss.changeName(spectrumNameTextBox.text())
        moss.changeNumber(spectrumNumberTextBox.text())
        print(moss.spectrumName, moss.spectrumNum)
    else:
        Errorbox('Please Insert a valid spectrum name and spectrum number')
        
def changeChan512():
    moss.changeChannelNumber(512)    #initialize the plot so that the xrange is the same
    curve.setData(y = np.zeros(512))
    spectrumPlot.setRange(QtCore.QRectF(0, 0, moss.datalength, 5)) # sets plot x range to current limits

def changeChan1024():
    moss.changeChannelNumber(1024)    #initialize the plot so that the xrange is the same
    curve.setData(y = np.zeros(1024))
    spectrumPlot.setRange(QtCore.QRectF(0, 0, moss.datalength, 5)) # sets plot x range to current limits



dataTimer.timeout.connect(getData)
if moss.status:
    dataTimer.start(5000)
    
clearButton.clicked.connect(clearSpectrum)
startRadioButton.clicked.connect(startSpectrum)
stopRadioButton.clicked.connect(stopSpectrum)
spectrumNameButton.clicked.connect(renameSpectrum)




window.show()

app.exec_()
