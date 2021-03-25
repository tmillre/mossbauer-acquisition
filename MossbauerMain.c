#include <stdio.h>
#include <stdlib.h>
#include <p24HJ128GP502.h>              //This is the only relevant library

#define FP 3690000                    //Actual clock freq must be used here
                                        //Was found by plugging pin 10 into
                                        //Oscilloscope

#define BAUDRATE 38400                  //Desired Baudrate
#define BRGVAL ((FP/BAUDRATE)/16)-1     //Function to determine BRG value
                                        //which is the value relevant to the PIC
                                        //in setting the baud rate
#define MAXDATALENGTH 1024

#pragma config FWDTEN = OFF             // Watchdog Timer Disabled
#pragma config JTAGEN = OFF             // JTAG Enable bit (JTAG is disabled)

#define DELAY_57uS asm volatile ("REPEAT, #2100"); Nop(); // 57uS delay in assembly

void Putstring(char *s);                //Function prototype for a function to write to UART
void Senddata(void);                    //Function to send the pulse data via UART
void Cleardata(void);                   //Function to clear data array


char received[]="4car";                 //this is a 5-char string. all op-codes are 4 char

int recindex=0;                         //preserves spacing through different interrupts
int pulsindex=0;                        //index of channel currently counting
long data[1024];
int datalength=512;


int flag=1;                             //flag to bring change from status quo
int recflag=0;                          //flag to say that there is a message
int startflag=0;                        //flag to say that program will start
int stopflag=0;                         //flag to say that program will stop

int status=0;                           //1 means the program is running


char ack[]="ACK";                       //Tell computer you have received a real message
char nack[]="NOP";                      //Tell computer you have received a bad message
char bdcmd[]="BDCMD";                   //Tell computer the command could not be performed
char run[]="RUN";
char stop[]="STOP";


int main(void)                          //that puts a string onto UART as ASCII
{
    Cleardata();
    
    AD1PCFGLbits.PCFG0	 =	1;	 //Set A0 to digital (This pin is used to say that program runs)
    TRISAbits.TRISA0=0;         //Set A0 as output
    LATAbits.LATA0=0;           //drive A0 low
    
    AD1PCFGLbits.PCFG1= 1;    //Set A1 to digital (This pin says count runnning)
    TRISAbits.TRISA1=0;       //Set A1 to output
    LATAbits.LATA1=0;       //Drive low (as count is stopped)

    TRISBbits.TRISB12=1;        //Set B12 to input (for INT1)
    AD1PCFGLbits.PCFG12=1;      //Set B12 to digital (for INT1)
    
    TRISBbits.TRISB3=1;        //Set B3 to input (for INT2)
    AD1PCFGLbits.PCFG5=1;      //Set B3 to digital (for INT2)
    
    TRISBbits.TRISB2=1;         //Set B2 to input (for UART RX)
    AD1PCFGLbits.PCFG4=1;       //Set B2 to digital (for UART RX)
    LATAbits.LATA0=1;           //drive A0 low
    
    
    __builtin_write_OSCCONL(OSCCON & ~(1<<6));  //Unlock PPS registers, necessary
                                            //to set remappable pin (RPx)

            
    CLKDIVbits.PLLPOST=0;                       //set processor to 40MIPS
                                                //(requires unlock too)
    
    RPOR7bits.RP15R = 3;                         //Put UART1 TX onto pin 26 (RP15/RB15)
    RPINR18bits.U1RXR = 2;                       //UART1 receive set to RP2 (RB2) Pin 6

    RPINR0bits.INT1R = 12;                       //Set External Interrupt 1 to RP12 (Pin 23/RB12)
    RPINR1bits.INT2R = 3;                       //Set External Interrupt 2 to RP3 (Pin 7/RB3)
    /*INT0 is a dedicated function of pin 16 (RB7)*/

    __builtin_write_OSCCONL(OSCCON | (1<<6));   //Lock PPS registers


    /*SETUP UART*/
    SRbits.IPL=3;                       //set CPU priority to 3
    U1MODEbits.STSEL = 0;                       // 1-Stop bit
    U1MODEbits.PDSEL = 0;                       // No Parity, 8-Data bits
    U1MODEbits.ABAUD = 0;                       // Auto-Baud disabled
    U1MODEbits.BRGH = 0;                        // Standard-Speed mode
    U1BRG = BRGVAL;                             // Baud Rate setting for 19200
    
    U1MODEbits.UARTEN = 1;                      // Enable UART
    U1STAbits.UTXEN = 1;                        // Enable UART TX
    U1STAbits.URXISEL=0;                        // Interrupt when anything is rx buffer
    
    IFS0bits.U1RXIF = 0;                // clear RX interrupt flag
    IPC2bits.U1RXIP=4;                  //setup priority for UART RX interrupt
    IEC0bits.U1RXIE = 1;                //allow for RX interrupt

    /*SETUP EXTERNAL INTERRUPTS*/
    //This is the channel reset interrupt
    IFS1bits.INT1IF = 0; /*Reset INT1 interrupt flag */
    IPC5bits.INT1IP = 5; //set INT1 to priority 5
    IEC1bits.INT1IE = 0; /*Disable INT1 Interrupt Service Routine */

    //This interrupt counts a pulse on from the detector
    IFS0bits.INT0IF = 0; /*Reset INT0 interrupt flag */
    IPC0bits.INT0IP = 7; //set INT0 to priority 6 (this has higher priority)
    IEC0bits.INT0IE = 0; /*Disable INT0 Interrupt Service Routine */

    //Increment Channel number interrupt
    IFS1bits.INT2IF = 0; /*Reset INT0 interrupt flag */
    IPC7bits.INT2IP = 6; //set INT0 to priority 6 (this has higher priority)
    IEC1bits.INT2IE = 0; /*Disable INT0 Interrupt Service Routine */


    /*THIS IS THE PROGRAM LOOP. IT JUST WAITS FOR USER INPUT AND TESTS IT.
    It will also prepare the controller for stop/start operations as well as sending
    data. It is of the lowest priority, where getting pulses and channel adjusts
    are of much more time critical and have greater priority.*/
    while(1)
    {                                        
        /*Since START/STOP need to start at channel 0, flags are set here
         and then read when there is the 0 channel interrupt. There is one
         flag, just called flag, that tells the interrupt that there is some change
         which is to allow for quicker execution when there is no change*/
        
        if (recflag==1)             //check if there's a new message
        {
            recflag=0;             //reset flag
            if (received[0]=='S' && received[1]=='T' && received[2]=='A'  && received[3]=='R')
            { /*If receiving the start command*/
                flag=1;
                startflag=1;         //put up start flag for when INT1 is triggered to start
                IFS1bits.INT1IF = 0; /*Reset INT1 interrupt flag */
                IEC1bits.INT1IE = 1; /*Enable INT1 Interrupt Service Routine */
                Putstring(ack);      //tell computer that the command has been acknowledged
            }
            else if (received[0]=='S' && received[1]=='T' && received[2]=='O'  && received[3]=='P')
            {
                flag=1;
                stopflag=1;         //take no action here and wait for 0 reset ISR to come to end
                Putstring(ack);     //tell the computer the instruction was received
            }
            else if (received[0]=='S' && received[1]=='E' && received[2]=='N'  && received[3]=='D')
            {
                Senddata();         //send the data over UART
                //don't send an ACK here because it's unnecessary.
            }
            else if (received[0]=='C' && received[1]=='L' && received[2]=='E'  && received[3]=='R')
            {
                if (status==0)      //if it's not running
                {
                    Cleardata();    //clear the data
                    Putstring(ack); //tell the computer the command was acknowledged
                }
                else Putstring(bdcmd); //tell computer that it was a bad command
            }
            else if (received[0]=='0' && received[1]=='5' && received[2]=='1'  && received[3]=='2')
            {
                if (status==0)      //if it's not running
                {
                    datalength=512;
                    Putstring(ack); //tell the computer the command was acknowledged
                }
                else Putstring(bdcmd); //tell computer that it was a bad command
            }
            else if (received[0]=='1' && received[1]=='0' && received[2]=='2'  && received[3]=='4')
            {
                if (status==0)      //if it's not running
                {
                    datalength=1024;
                    Putstring(ack); //tell the computer the command was acknowledged
                }
                else Putstring(bdcmd); //tell computer that it was a bad command
            }
            /*These commands require more a chip with more memory*/
//            else if (received[0]=='2' && received[1]=='0' && received[2]=='4'  && received[3]=='8')
//            {
//                if (status==0)      //if it's not running
//                {
//                    datalength=2048;
//                    Putstring(ack); //tell the computer the command was acknowledged
//                }
//                else Putstring(bdcmd); //tell computer that it was a bad command
//            }
//            else if (received[0]=='4' && received[1]=='0' && received[2]=='9'  && received[3]=='6')
//            {
//                if (status==0)      //if it's not running
//                {
//                    datalength=4096;
//                    Putstring(ack); //tell the computer the command was acknowledged
//                }
//                else Putstring(bdcmd); //tell computer that it was a bad command
//            }
            else if (received[0]=='S' && received[1]=='T' && received[2]=='A'  && received[3]=='T')
            {
                if (status==0) Putstring(stop); //if not running, tell computer it isn't
                else Putstring(run);            //otherwise, say it is running
            }
            else 
            {
                Putstring(nack);                //if string could not be matched,
                                            //it's a bad command, so let the user know
                Putstring(received);
                recindex=0;                     //reset index in case of a framing error
            }
        }       
    }
}                                           

/*Program will reset randomly in loop if Watchdog Timer is enabled
 
  Apparently Watchdog timer is a good thing for when projects are near completion*/



/*USER DEFINED FUNCTIONS*/

void Putstring(char *s)                     //function definition
{
                                            //pointer sent points to first
    char c;                                 //item in array (teststring[0])
    int i;                                  //and since array stored sequentially
                                            //s++ goes to teststring[i+1]

    for(i=0;(c=*s)!='\0';s++)               //C strings are terminated by '\0'
    {                                       //So can detect end by for condition
        
        while (U1STAbits.UTXBF==1);         //Wait for TXREG Buffer to become available
        U1TXREG = c;                        //Transmit one character
    }
}

void Cleardata(void)
{
    for(pulsindex=0;pulsindex<MAXDATALENGTH;pulsindex++) data[pulsindex]=0;
}


//this function will send an the global array data as ascii values with 
//no leading or trailing digits.
void Senddata(void)
{
    int i;
    long divis=1;
    long hold;
    int digit;
    for(i=0;i<datalength;i++)               //go through each datapoint
    {   
        if (i!=0)
        {
            while (U1STAbits.UTXBF==1);     //Wait for TXREG Buffer to become available
            U1TXREG = ',';                  //Send the leading digit of data[i]
        }
        divis=1;                            //set divisor to 1
        while(data[i]/divis!=0) divis*=10;  //find power of 10 that is greater than number
        if (divis!=1) divis=divis/10;       //if number is 0, do not divide as divis->0.
        hold=data[i];                       //do not actually edit the data.
        while(divis!=0)
        {
            digit=hold/divis;               //get the leading digit of data[i]
            while (U1STAbits.UTXBF==1);     //Wait for TXREG Buffer to become available
            U1TXREG = digit+'0';            //Send the leading digit of data[i] as ascii
            hold=hold-divis*digit;          //remove the leading digit
            divis=divis/10;                 //and get temp ready for next digit
        }
    }
}

/*INTERRUPT FUNCTIONS*/

//External Interrupt 0 Routine: will increment pulse count in current bin
void __attribute__((__interrupt__)) _INT0Interrupt(void);
void __attribute__((__interrupt__, no_auto_psv)) _INT0Interrupt(void)
{
    if (pulsindex<MAXDATALENGTH)    //don't record anything outside of array
        data[pulsindex]++;
    else if (pulsindex==datalength) data[0]++;   //since start pulse comes late
    IFS0bits.INT0IF = 0; /*Reset INT0 interrupt flag */
}

//External Interrupt 1 Routine: will reset channel to 0/is needed to stop/start system
void __attribute__((__interrupt__)) _INT1Interrupt(void);
void __attribute__((__interrupt__, no_auto_psv)) _INT1Interrupt(void)
{
   if (flag==1)
   {
       flag=0;
       if (startflag==1)
       {
           startflag=0;
           status=1;
           IFS0bits.INT0IF = 0; /*Reset INT0 interrupt flag */
           IFS1bits.INT2IF = 0; /*Reset INT2 interrupt flag */
           IEC0bits.INT0IE = 1; /*Enable INT0 Interrupt Service Routine */
           IEC1bits.INT2IE = 1; /*Enable INT2 Interrupt Service Routine */
           LATAbits.LATA1=1;       //Drive A1 low (as count is started)
       }
       if (stopflag==1)
       {
           stopflag=0;
           status=0;
           IEC0bits.INT0IE = 0; /*Enable INT0 Interrupt Service Routine */
           IEC1bits.INT2IE = 0; /*Enable INT2 Interrupt Service Routine */
           IEC1bits.INT1IE = 0; /*Enable INT1 Interrupt Service Routine */
           LATAbits.LATA1=0;       //Drive A1 low (as count is stopped)
       }
           
   }
   pulsindex=0;
   IFS1bits.INT2IF = 0; //ignore the increment channel interrupt
   IFS1bits.INT1IF = 0; /*Reset INT1 interrupt flag */
}

//External Interrupt 2 Routine: will increment channel number
void __attribute__((__interrupt__)) _INT2Interrupt(void);
void __attribute__((__interrupt__, no_auto_psv)) _INT2Interrupt(void)
{
    if (pulsindex<MAXDATALENGTH)    //required to prevent overflow when start pulses die
        pulsindex++;
    IFS1bits.INT2IF = 0; /*Reset INT2 interrupt flag */
}

//UART RX Interrupt Routine: will read off UART and save string.
void __attribute__((__interrupt__)) _U1RXInterrupt(void);
void __attribute__((__interrupt__,no_auto_psv)) _U1RXInterrupt(void)
{ 
    received[recindex] = U1RXREG;            //save received character
    if (received[recindex]!='\0') recindex++;
    if(recindex==4)
    {
        recindex=0;
        recflag=1;
    }
    IFS0bits.U1RXIF = 0;                // clear RX interrupt flag
}

