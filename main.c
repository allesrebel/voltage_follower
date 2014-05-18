/*
 * main.c - Voltage Follower Demo
 * Uses MSP430G2553 for the controller and MCP4921 for the DAC
 * A LM393 is used for a comparator, and a CD4016 for transmission gate
 * Project 3 for CPE 329
 *
 * Written By: Alles Rebel and Evan Manrique
 *	Version  : 0.9
 *
 */

#include <msp430.h> 

// Pin config (have to be on port 1)
#define sampleIn	BIT2	//Turn on TransmissionGate
#define sampleClk	BIT0	//sample clk pin
#define compOut		BIT1	//Comparator output pin
#define rNetOut		P2OUT	//Resistor network out
#define rNetPort	P2DIR

/*
 * Pin Defines
 */
#define CS		BIT4
#define CSPOut	P1OUT
#define CSPort	P1DIR

//USCI General Port Info
#define USCIPS1	P1SEL	//USCI Pin mode select
#define USCIPS2	P1SEL2	//USCI Pin mode select 2
//UCB0 PORT Defs
#define UCBSOMI	BIT6	//isn't really used to communicate with DAC
#define UCBSIMO	BIT7
#define UCBCLK	BIT5

void setupClock();
void setupTimer();
void setupPorts();
void takeSample();
void makeRamp();
void Drive_DAC(unsigned int);
int average(int, int);

int opPending = 0;
int inputFound = 0;
int previnput = 0;

/*
 * Global Variables
 */
int count = 0;

int main(void) {
	WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer

	setupClock();
	//setupTimer();
	setupPorts();
	while (1) {
		takeSample();
		makeRamp();
	}
}

void makeRamp() {
	while (count < 256) {
		rNetOut = count;

		//check if we got it...
		if (P1IN & compOut) {
			//This is where we output to the DAC cause we found it.
			Drive_DAC((average(count,previnput)*5)+30);
			previnput = count;
			break;
		}

		__delay_cycles(21);
		count++;
	}
	count = 0;
	rNetOut = count;
}

void setupPorts() {
	//set sample clock as output
	//set sampleIn to low
	P1OUT &= ~sampleIn;
	P1DIR |= (sampleClk + sampleIn);
	P1DIR &= ~compOut;

	//do a special init for 2.6 and 2.7
	P2SEL &= ~(BIT6 + BIT7); // needs to be preped cause they aren't by default
	P2SEL2 &= ~(BIT6 + BIT7);
	rNetPort |= 0xFF;
	rNetOut |= 0xFF;

	//Initialize Ports for DAC
	CSPort |= CS;
	USCIPS1 = UCBSIMO + UCBCLK;	// These two lines select UCB0CLK for P1.5
	USCIPS2 = UCBSIMO + UCBCLK;	// and select UCB0SIMO for P1.7
	UCB0CTL0 |= UCCKPL + UCMSB + UCMST + UCSYNC;
	UCB0CTL1 |= UCSSEL_2; 	// sets UCB0 will use SMCLK as the bit shift clock
	UCB0CTL1 &= ~UCSWRST;	//SPI enable
}

void setupTimer() {
	//Set up the timer to fire as a sample clock
	TA0CCTL0 = CCIE;
	TA0CCR0 = 21;
	//Set up timer to work off the main clock with upcount mode
	TA0CTL = TASSEL_2 + MC_1;
}

void setupClock() {
	//16Mhz
	if (CALBC1_16MHZ == 0xFF) {
		while (1)
			;                               // do not load, trap CPU!!
	}
	DCOCTL = 0;                          // Select lowest DCOx and MODx settings
	BCSCTL1 = CALBC1_16MHZ;                   // Set range
	DCOCTL = CALDCO_16MHZ;                    // Set DCO step + modulation*/
}

void takeSample() {
	//load the transmission gate's cap
	P1OUT |= sampleIn;
	__delay_cycles(2000);
	P1OUT &= ~sampleIn;
}

int average(int a,int b){
	return (a+b)>>1;
}

void Drive_DAC(unsigned int level) {
	//Create the container for the total data that's going to be sent (16 bits total)
	unsigned int DAC_Word = 0;	//default is DAC off
	//check if level and duty is non-zero
	DAC_Word = (0x1000) | (level & 0x0FFF); // 0x1000 sets DAC for Write

	CSPOut &= ~CS; //Select the DAC
	UCB0TXBUF = (DAC_Word >> 8); // Send in the upper 8 bits

	//would normally wait for buffer to flush, but don't need to at 16Mhz

	UCB0TXBUF = (unsigned char) (DAC_Word & 0x00FF); // Transmit lower byte to DAC

	//roughly the the time needed to process everything
	__delay_cycles(10);	//any less and we don't get anything

	//Clock the data in
	CSPOut |= CS;
}
