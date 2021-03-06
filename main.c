#include "main.h"
#include "driverlib/driverlib.h"
#include "hal_LCD.h"
#include "driverlib/timer_a.h"
#include "driverlib/rtc.h"

void TriggerAlarm(void);
void StopAlarm(void);
void SetAlertOnZone(int danger_port, int danger_port_pin, int armedOk_port, int armedOk_port_pin);
void ResetSystem(void);
void Init_RTC(void);
void Init_ADC(void);
void Init_time(void);
void Count_down(void);
void Danger_func(void);
void display_four_zone(void);
void GreenLEDOn(void);
void YellowLEDOn(void);
int listenMicAndAmp(void);
void initializeZones(char status);
void displayPBTimerMsg(void);

/*
 * This project contains some code samples that may be useful.
 *
 */
int danger_existence = 0;
char ADCState = 0; //Busy state of the ADC
int16_t ADCResult = 0; //Storage for the ADC conversion result
char time_val_char[]={'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
volatile int one_sec=0;
char four_zone[4];
int unarmed_time=0;

void ADC_ISR(void);
void RTC_ISR(void);

enum zone_states {ZONE_UNARMED, ZONE_ARMED, WATCHING};

volatile unsigned int HRS;
volatile unsigned int MINS;
volatile unsigned int SEC;

void main(void)
{
    int buttonState = 0; //Current button press state (to allow edge detection)
//    int sensor_state = 0;

    /*
     * Functions with two underscores in front are called compiler intrinsicbs.
     * They are documented in the compiler user guide, not the IDE or MCU guides.
     * They are a shortcut to insert some assembly code that is not really
     * expressible in plain C/C++. Google "MSP430 Optimizing C/C++ Compiler
     * v18.12.0.LTS" and search for the word "intrinsic" if you want to know
     * more.
     * */

    //Turn off interrupts during initialization
    __disable_interrupt();

    //Stop watchdog timer unless you plan on using it
    WDT_A_hold(WDT_A_BASE);

    // Initializations - see functions for more detail
    Init_GPIO();    //Sets all pins to output low as a default
    Init_PWM();     //Sets up a PWM output
    Init_ADC();     //Sets up the ADC to sample
    Init_Clock();   //Sets up the necessary system clocks
//    Init_UART();    //Sets up an echo over a COM port
    Init_LCD();     //Sets up the LaunchPad LCD display
    Init_RTC();
     /*
     * The MSP430 MCUs have a variety of low power modes. They can be almost
     * completely off and turn back on only when an interrupt occurs. You can
     * look up the power modes in the Family User Guide under the Power Management
     * Module (PMM) section. You can see the available API calls in the DriverLib
     * user guide, or see "pmm.h" in the driverlib directory. Unless you
     * purposefully want to play with the power modes, just leave this command in.
     */
    PMM_unlockLPM5(); //Disable the GPIO power-on default high-impedance mode to activate previously configured port settings

    //All done initializations - turn interrupts back on.
    __enable_interrupt();

//    displayStaticText("ECE 298");

    // J2 Pin Settings
    GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN7);       // Zone 1 Danger
    GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN6);       // Zone 1 Armed, but OK
    GPIO_setAsOutputPin(GPIO_PORT_P5, GPIO_PIN0);       // Zone 2 Danger
    GPIO_setAsOutputPin(GPIO_PORT_P8, GPIO_PIN3);       // Zone 2 Armed, but OK
    GPIO_setAsOutputPin(GPIO_PORT_P5, GPIO_PIN2);       // Zone 3 Danger
    GPIO_setAsOutputPin(GPIO_PORT_P5, GPIO_PIN3);       // Zone 3 Armed, but OK
    GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN3);       // Zone 4 Danger
    GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN4);       // Zone 4 Armed, but OK
    GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN5);       // Alarm Signal for Transducer

    // J1 Pin Settings
    GPIO_setAsInputPin(GPIO_PORT_P8, GPIO_PIN0);          // Reed Switch 4 Input Signal
    GPIO_setAsInputPin(GPIO_PORT_P1, GPIO_PIN1);          // Reed Switch 3 Input Signal
    GPIO_setAsInputPin(GPIO_PORT_P2, GPIO_PIN7);          // Reed Switch 1 Input Signal
//    GPIO_setAsInputPin(GPIO_PORT_P8, GPIO_PIN0);          // Mic and Amp Analog Input Signal

    initializeZones('U');
//    four_zone[0] = 'A';
//    display_four_zone();

    Timer_A_clear(TIMER_A0_BASE);
    enum zone_states zoneState = ZONE_UNARMED;
    displayPBTimerMsg();

    int pb_12=0;
    int both_push=0;
    int _first = 0;

    while (1){

        switch (zoneState) {
        case ZONE_UNARMED:
            // if rtc is not setup yet, then the system is in unarmed neutral state
            displayPBTimerMsg();
            Init_time();
            GreenLEDOn();
            __delay_cycles(500000);
            clearLCD();
            Count_down();
            initializeZones('W');
            if(unarmed_time == 0){
                zoneState = ZONE_ARMED;
                break;
            }

        case ZONE_ARMED:
            YellowLEDOn();
            zoneState = WATCHING;
            break;

        case WATCHING:
            // after the RTC is set up, we want to arm the system
            // after turning on the Yellow LEDs, we now want to check the zones and the reed switches
            if (danger_existence == 1) {
                TriggerAlarm();
            }

            if (_first == 0) {
                _first++;
                initializeZones('W');
            }

            display_four_zone();
            Danger_func();

            // PB Button Handling
//            pb_12 = (GPIO_getInputPinValue(SW1_PORT, SW1_PIN)==0);
//            both_push = pb_12 && pb_26;
            if ((GPIO_getInputPinValue(SW1_PORT, SW1_PIN)==0)) {
                ResetSystem();
                showChar('R', pos1);
                showChar('E', pos2);
                showChar('S', pos3);
                showChar('E', pos4);
                showChar('T', pos5);
                __delay_cycles(1000000);
                zoneState = ZONE_UNARMED;
            } else {
                zoneState = WATCHING; // keep system in  mode if it hasn't been reset
            }

            break;
        }
    }

}


// Zone Unarmed, Neutral State
void GreenLEDOn(void) {
    GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN6);
    GPIO_setOutputLowOnPin(GPIO_PORT_P8, GPIO_PIN3);
    GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN3);
    GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN4);
}

// Zone Armed
void YellowLEDOn(void) {
    GPIO_setOutputHighOnPin(GPIO_PORT_P1, GPIO_PIN6);
    GPIO_setOutputHighOnPin(GPIO_PORT_P8, GPIO_PIN3);
    GPIO_setOutputHighOnPin(GPIO_PORT_P5, GPIO_PIN3);
    GPIO_setOutputHighOnPin(GPIO_PORT_P1, GPIO_PIN4);
}

// RTC interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=RTC_VECTOR
__interrupt void RTC_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(RTC_VECTOR))) RTC_ISR (void)
#else
#error Compiler not supported!
#endif
{

    switch (__even_in_range(RTCIV,2)) {
        case 0: break;  //No interrupts
        case 2:         //RTC overflow
            one_sec = 1;
            break;
        default: break;
    }

//    if(RTCIV & RTCIV_RTCIF)    {                  // RTC Overflow
//        P1OUT ^= BIT0;
//        SEC++;
//        if(SEC==60) {
//            MINS++;
//            SEC=0;
//        }
//        if(MINS==60) {
//            HRS++;
//            MINS=0;
//        }
//        if(HRS==24) {
//            HRS=0;
//        }
//     }
}

// Triggers audio transducer by setting outputs on associated pin
void TriggerAlarm(void)
{
    GPIO_setOutputHighOnPin(GPIO_PORT_P1, GPIO_PIN5);
//    __delay_cycles(160);
    GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN5);
}

void StopAlarm(void) {
    GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN5);
}

// Sets danger pin output to high and the
void SetAlertOnZone(
        int danger_port,
        int danger_port_pin,
        int armedOk_port,
        int armedOk_port_pin)
{
    GPIO_setOutputHighOnPin(danger_port, danger_port_pin);
//    GPIO_setOutputLowOnPin(armedOk_port, armedOk_port_pin);
    danger_existence = 1;
    TriggerAlarm();
    display_four_zone();
}

void ResetSystem(void)
{
    // Essentially want to turn everything off which is how the system starts - set output low on all pins
    GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN1|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN0|GPIO_PIN2|GPIO_PIN3);
    GPIO_setOutputLowOnPin(GPIO_PORT_P8, GPIO_PIN3);
    danger_existence = 0;
    StopAlarm();
    displayPBTimerMsg();
}

void Init_RTC(void)
{
    // Initialize RTC
    // Source = 32kHz crystal, divided by 1024
//    RTCCTL = RTCSS__XT1CLK | RTCSR | RTCPS__1024 | RTCIE;
    // RTC count re-load compare value at 32.
    // 1024/32768 * 32 = 1 sec.
//    RTCMOD = 32-1;

    RTC_init(RTC_BASE, 32768, RTC_CLOCKPREDIVIDER_1);
    RTC_clearInterrupt(RTC_BASE, RTC_OVERFLOW_INTERRUPT_FLAG);
    RTC_enableInterrupt(RTC_BASE, RTC_OVERFLOW_INTERRUPT);
    RTC_start(RTC_BASE, RTC_CLOCKSOURCE_SMCLK);
}


void Init_GPIO(void)
{
    // Set all GPIO pins to output low to prevent floating input and reduce power consumption
    GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setOutputLowOnPin(GPIO_PORT_P3, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setOutputLowOnPin(GPIO_PORT_P4, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setOutputLowOnPin(GPIO_PORT_P6, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setOutputLowOnPin(GPIO_PORT_P7, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setOutputLowOnPin(GPIO_PORT_P8, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);

    GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setAsOutputPin(GPIO_PORT_P2, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setAsOutputPin(GPIO_PORT_P3, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setAsOutputPin(GPIO_PORT_P4, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setAsOutputPin(GPIO_PORT_P5, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setAsOutputPin(GPIO_PORT_P6, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setAsOutputPin(GPIO_PORT_P7, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setAsOutputPin(GPIO_PORT_P8, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);

    //Set LaunchPad switches as inputs - they are active low, meaning '1' until pressed
    GPIO_setAsInputPinWithPullUpResistor(SW1_PORT, SW1_PIN);
    GPIO_setAsInputPinWithPullUpResistor(SW2_PORT, SW2_PIN);

    //Set LED1 and LED2 as outputs
    //GPIO_setAsOutputPin(LED1_PORT, LED1_PIN); //Comment if using UART
//    GPIO_setAsOutputPin(LED2_PORT, LED2_PIN);
}

/* Clock System Initialization */
void Init_Clock(void)
{
    /*
     * The MSP430 has a number of different on-chip clocks. You can read about it in
     * the section of the Family User Guide regarding the Clock System ('cs.h' in the
     * driverlib).
     */

    /*
     * On the LaunchPad, there is a 32.768 kHz crystal oscillator used as a
     * Real Time Clock (RTC). It is a quartz crystal connected to a circuit that
     * resonates it. Since the frequency is a power of two, you can use the signal
     * to drive a counter, and you know that the bits represent binary fractions
     * of one second. You can then have the RTC module throw an interrupt based
     * on a 'real time'. E.g., you could have your system sleep until every
     * 100 ms when it wakes up and checks the status of a sensor. Or, you could
     * sample the ADC once per second.
     */
    //Set P4.1 and P4.2 as Primary Module Function Input, XT_LF
    GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P4, GPIO_PIN1 + GPIO_PIN2, GPIO_PRIMARY_MODULE_FUNCTION);

    // Set external clock frequency to 32.768 KHz
    CS_setExternalClockSource(32768);
    // Set ACLK = XT1
    CS_initClockSignal(CS_ACLK, CS_XT1CLK_SELECT, CS_CLOCK_DIVIDER_1);
    // Initializes the XT1 crystal oscillator
    CS_turnOnXT1LF(CS_XT1_DRIVE_1);
    // Set SMCLK = DCO with frequency divider of 1
    CS_initClockSignal(CS_SMCLK, CS_DCOCLKDIV_SELECT, CS_CLOCK_DIVIDER_1);
    // Set MCLK = DCO with frequency divider of 1
    CS_initClockSignal(CS_MCLK, CS_DCOCLKDIV_SELECT, CS_CLOCK_DIVIDER_1);
}

/* UART Initialization */
//void Init_UART(void)
//{
//    /* UART: It configures P1.0 and P1.1 to be connected internally to the
//     * eSCSI module, which is a serial communications module, and places it
//     * in UART mode. This let's you communicate with the PC via a software
//     * COM port over the USB cable. You can use a console program, like PuTTY,
//     * to type to your LaunchPad. The code in this sample just echos back
//     * whatever character was received.
//     */
//
//    //Configure UART pins, which maps them to a COM port over the USB cable
//    //Set P1.0 and P1.1 as Secondary Module Function Input.
//    GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P1, GPIO_PIN1, GPIO_PRIMARY_MODULE_FUNCTION);
//    GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P1, GPIO_PIN0, GPIO_PRIMARY_MODULE_FUNCTION);
//
//    /*
//     * UART Configuration Parameter. These are the configuration parameters to
//     * make the eUSCI A UART module to operate with a 9600 baud rate. These
//     * values were calculated using the online calculator that TI provides at:
//     * http://software-dl.ti.com/msp430/msp430_public_sw/mcu/msp430/MSP430BaudRateConverter/index.html
//     */
//
//    //SMCLK = 1MHz, Baudrate = 9600
//    //UCBRx = 6, UCBRFx = 8, UCBRSx = 17, UCOS16 = 1
//    EUSCI_A_UART_initParam param = {0};
//        param.selectClockSource = EUSCI_A_UART_CLOCKSOURCE_SMCLK;
//        param.clockPrescalar    = 6;
//        param.firstModReg       = 8;
//        param.secondModReg      = 17;
//        param.parity            = EUSCI_A_UART_NO_PARITY;
//        param.msborLsbFirst     = EUSCI_A_UART_LSB_FIRST;
//        param.numberofStopBits  = EUSCI_A_UART_ONE_STOP_BIT;
//        param.uartMode          = EUSCI_A_UART_MODE;
//        param.overSampling      = 1;
//
//    if(STATUS_FAIL == EUSCI_A_UART_init(EUSCI_A0_BASE, &param))
//    {
//        return;
//    }
//
//    EUSCI_A_UART_enable(EUSCI_A0_BASE);
//
//    EUSCI_A_UART_clearInterrupt(EUSCI_A0_BASE, EUSCI_A_UART_RECEIVE_INTERRUPT);
//
//    // Enable EUSCI_A0 RX interrupt
//    EUSCI_A_UART_enableInterrupt(EUSCI_A0_BASE, EUSCI_A_UART_RECEIVE_INTERRUPT);
//}

/* EUSCI A0 UART ISR - Echoes data back to PC host */
#pragma vector=USCI_A0_VECTOR
__interrupt
void EUSCIA0_ISR(void)
{
    uint8_t RxStatus = EUSCI_A_UART_getInterruptStatus(EUSCI_A0_BASE, EUSCI_A_UART_RECEIVE_INTERRUPT_FLAG);

    EUSCI_A_UART_clearInterrupt(EUSCI_A0_BASE, RxStatus);

    if (RxStatus)
    {
        EUSCI_A_UART_transmitData(EUSCI_A0_BASE, EUSCI_A_UART_receiveData(EUSCI_A0_BASE));
    }
}

/* PWM Initialization */
void Init_PWM(void)
{
    /*
     * The internal timers (TIMER_A) can auto-generate a PWM signal without needing to
     * flip an output bit every cycle in software. The catch is that it limits which
     * pins you can use to output the signal, whereas manually flipping an output bit
     * means it can be on any GPIO. This function populates a data structure that tells
     * the API to use the timer as a hardware-generated PWM source.
     *
     */
    //Generate PWM - Timer runs in Up-Down mode
    param.clockSource           = TIMER_A_CLOCKSOURCE_SMCLK;
    param.clockSourceDivider    = TIMER_A_CLOCKSOURCE_DIVIDER_1;
    param.timerPeriod           = TIMER_A_PERIOD; //Defined in main.h
    param.compareRegister       = TIMER_A_CAPTURECOMPARE_REGISTER_1;
    param.compareOutputMode     = TIMER_A_OUTPUTMODE_RESET_SET;
    param.dutyCycle             = HIGH_COUNT; //Defined in main.h

    //PWM_PORT PWM_PIN (defined in main.h) as PWM output
    GPIO_setAsPeripheralModuleFunctionOutputPin(PWM_PORT, PWM_PIN, GPIO_PRIMARY_MODULE_FUNCTION);
}

void Init_ADC(void)
{
    /*
     * To use the ADC, you need to tell a physical pin to be an analog input instead
     * of a GPIO, then you need to tell the ADC to use that analog input. Defined
     * these in main.h for A9 on P8.1.
     */

    //Set ADC_IN to input direction
    //GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P8, GPIO_PIN0, GPIO_PRIMARY_MODULE_FUNCTION);
    GPIO_setAsPeripheralModuleFunctionInputPin(ADC_IN_PORT, ADC_IN_PIN, GPIO_PRIMARY_MODULE_FUNCTION);
    //Initialize the ADC Module
    /*
     * Base Address for the ADC Module
     * Use internal ADC bit as sample/hold signal to start conversion
     * USE MODOSC 5MHZ Digital Oscillator as clock source
     * Use default clock divider of 1
     */
    ADC_init(ADC_BASE,
             ADC_SAMPLEHOLDSOURCE_SC,
             ADC_CLOCKSOURCE_ADCOSC,
             ADC_CLOCKDIVIDER_1);

    ADC_enable(ADC_BASE);

    /*
     * Base Address for the ADC Module
     * Sample/hold for 16 clock cycles
     * Do not enable Multiple Sampling
     */
    ADC_setupSamplingTimer(ADC_BASE,
                           ADC_CYCLEHOLD_16_CYCLES,
                           ADC_MULTIPLESAMPLESDISABLE);

    //Configure Memory Buffer
    /*
     * Base Address for the ADC Module
     * Use input ADC_IN_CHANNEL
     * Use positive reference of AVcc
     * Use negative reference of AVss
     */
    ADC_configureMemory(ADC_BASE,
                        ADC_IN_CHANNEL,
                        ADC_VREFPOS_AVCC,
                        ADC_VREFNEG_AVSS);

    ADC_clearInterrupt(ADC_BASE,
                       ADC_COMPLETED_INTERRUPT);

    //Enable Memory Buffer interrupt
    ADC_enableInterrupt(ADC_BASE,
                        ADC_COMPLETED_INTERRUPT);
}

//ADC interrupt service routine
#pragma vector=ADC_VECTOR
__interrupt
void ADC_ISR(void)
{
    uint8_t ADCStatus = ADC_getInterruptStatus(ADC_BASE, ADC_COMPLETED_INTERRUPT_FLAG);
    ADC_clearInterrupt(ADC_BASE, ADCStatus);

    if (ADCStatus)
    {
        ADCState = 0; //Not busy anymore
        ADCResult = ADC_getResults(ADC_BASE);
    }
}


void showTime(int total_time) {
    if (total_time < 10 && total_time >= 0) {
        showChar(' ', pos1);
        showChar(' ', pos2);
        showChar(' ', pos3);
        showChar(time_val_char[total_time], pos4);
    } else if (total_time < 100 && total_time > 9) {
        int tens_digit = total_time/10;
        int ones_digit = (total_time % 10) % 10;
        showChar(' ', pos1);
        showChar(' ', pos2);
        showChar(time_val_char[tens_digit], pos3);
        showChar(time_val_char[ones_digit], pos4);
    } else if (total_time < 1000 && total_time > 99) {
        int hundreds_digit = total_time/100;
        int tens_digit = (total_time % 100) / 10;
        int ones_digit = (total_time % 100) % 10;
        showChar(' ', pos1);
        showChar(time_val_char[hundreds_digit], pos2);
        showChar(time_val_char[tens_digit], pos3);
        showChar(time_val_char[ones_digit], pos4);
    }

    showChar(' ', pos5);
}

void Init_time() {
    int pb_push = 0;
    int time_less;
    int time_more;
    int both_push;
    int time_init = 1;
    int time_val = 0;

    // time_val is for an individual digit
    // unarmed time will keep track of the total
    while (pb_push == 0) {
        time_less = GPIO_getInputPinValue(SW1_PORT, SW1_PIN)==0;
        time_more = GPIO_getInputPinValue(SW2_PORT, SW2_PIN)==0;
        both_push=time_less && time_more;

        if (both_push) {
            time_val=time_init*10;
           // unarmed_time=time_init*10;
            break;
        } else {
            __delay_cycles(100000);
            if (time_less == 1 && unarmed_time > 0) {
                unarmed_time -= 1;
                showTime(unarmed_time);
            } else if (time_more == 1 && unarmed_time < 1000) {
                unarmed_time += 1;
                showTime(unarmed_time);
            }

        }

    }
}

void Count_down(void){
    char count_down_char[10];
        //one sec pass
        one_sec = 0;
        while (unarmed_time >=0) {
            __delay_cycles(1059000);
    //                sprintf(count_down_char, "%d", unarmed_time);
//            showChar(' ', pos1);
//            showChar(' ', pos2);
//            showChar(time_val_char[unarmed_time], pos3);
//            showChar('0', pos4);

            if (unarmed_time > 99) {
                int hundreds_digit = unarmed_time/100;
                int tens_digit = (unarmed_time % 100) / 10;
                int ones_digit = (unarmed_time % 100) % 10;
                showChar(' ', pos1);
                showChar(time_val_char[hundreds_digit], pos2);
                showChar(time_val_char[tens_digit], pos3);
                showChar(time_val_char[ones_digit], pos4);
            } else if (unarmed_time > 9 && unarmed_time < 100) {
                int tens_digit = unarmed_time/10;
                int ones_digit = (unarmed_time % 10) % 10;
                showChar(' ', pos1);
                showChar(' ', pos2);
                showChar(time_val_char[tens_digit], pos3);
                showChar(time_val_char[ones_digit], pos4);
            } else if (unarmed_time > 0 && unarmed_time < 10) {
                showChar(' ', pos1);
                showChar(' ', pos2);
                showChar(' ', pos3);
                showChar(time_val_char[unarmed_time], pos4);
            }

            unarmed_time--;
        }

}

void Danger_func(){

    if (GPIO_getInputPinValue(GPIO_PORT_P2, GPIO_PIN7) == 0) {                              // Reed Switch 1 Triggered - Correspond with Zone 1
        four_zone[0] = 'A';
        SetAlertOnZone(GPIO_PORT_P1, GPIO_PIN7, GPIO_PORT_P1, GPIO_PIN6);
    }

    GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P1, GPIO_PIN0, GPIO_PRIMARY_MODULE_FUNCTION);
    GPIO_setAsInputPin(GPIO_PORT_P1, GPIO_PIN0);
    if (GPIO_getInputPinValue(GPIO_PORT_P1, GPIO_PIN0) == 0) {                       // Reed Switch 2 Triggered - Zone 2
        GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P1, GPIO_PIN0, GPIO_PRIMARY_MODULE_FUNCTION);
        four_zone[1] = 'A';
        SetAlertOnZone(GPIO_PORT_P5, GPIO_PIN0, GPIO_PORT_P8, GPIO_PIN3);
    }
    if (GPIO_getInputPinValue(GPIO_PORT_P1, GPIO_PIN1) == 0) {                       // Reed Switch 3 Triggered - Zone 3
        four_zone[2] = 'A';
        SetAlertOnZone(GPIO_PORT_P5, GPIO_PIN2, GPIO_PORT_P5, GPIO_PORT_P3);
    }
    if (GPIO_getInputPinValue(GPIO_PORT_P8, GPIO_PIN0) == 0) {                       // Reed Switch 4 Triggered - Zone 4
        four_zone[3] = 'A';
        SetAlertOnZone(GPIO_PORT_P1, GPIO_PIN3, GPIO_PORT_P1, GPIO_PIN4);
    }

    ADCState = 1;
    ADC_startConversion(ADC_BASE, ADC_SINGLECHANNEL);
    if( (int)ADCResult > 800) {
        four_zone[0] = 'A';
        SetAlertOnZone(GPIO_PORT_P1, GPIO_PIN7, GPIO_PORT_P1, GPIO_PIN6);
    }

}

void displayPBTimerMsg(void) {
    showChar('T', pos1);
    showChar('I', pos2);
    showChar('M', pos3);
    showChar('E', pos4);
    showChar('R', pos5);
}

void initializeZones(char status) {
    int i=0;
    for(i =0; i<4; i++){
        four_zone[i]= status; // Unarmed
    }
    display_four_zone();
}

void display_four_zone(){
    showChar(four_zone[0], pos1);
    showChar(four_zone[1], pos2);
    showChar(four_zone[2], pos3);
    showChar(four_zone[3], pos4);
}


