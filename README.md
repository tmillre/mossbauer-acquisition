# mossbauer-acquisition
Cheap microcontroller-based replacement for acquisition card for mossbauer spectroscopy.

Originally written and built to be a cheap replacement (~$5) for expensive (~$5000) acquisition cards inside of 30 year old (486's?) computers. 

Python Requirements:
Serial
PyQT5
pyqtgraph
numpy

Microcontroller code is written for Microchip's PIC-24HJ128GP502. Other versions of this hardware may work, but some modification to the code may be necessary.

# Contents

guitest.py -- This is the code that will start up the GUI for real-time plotting and control of the microcontroller
simulator.py -- Utility for testing the GUI without access to hardware (to see what it's like). Run with --help option to see options. User must specify spectrum to simulate and must specify the correct serial port to talk over.
moss.py -- Defines mossbauer object to control microcontroller and hold the spectrum.

MossbauerMain.c -- Microcontroller code that interfaces with detector, etc. I wrote this when I was new to coding and am unable to update it since I no longer have access to hardware. 
