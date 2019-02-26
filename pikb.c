#include <avr/io.h>
#include <avr/interrupt.h>

#ifndef F_CPU 
#define F_CPU 8000000UL
#endif
#define USART_BAUDRATE 9600
#define BAUD_PRESCALE (((F_CPU / (USART_BAUDRATE * 16UL))) - 1)

unsigned char keyState;            // state
unsigned char keyCode;             // key code
unsigned char keyDown;             // key is down
volatile unsigned char keyValue;   // key symbol
volatile unsigned char keyNew;     // got new key
volatile unsigned char rcvd;       // rcvd char

__flash const unsigned char keyboard[13][2] = {
    {0x81, '1'}, {0x41, '2'}, {0x21, '3'},
    {0x82, '4'}, {0x42, '5'}, {0x22, '6'},
    {0x84, '7'}, {0x44, '8'}, {0x24, '9'},
    {0x88, '*'}, {0x48, '0'}, {0x28, '#'}, {0x18, 'E'}
};

// send char to uart
void usart_send(unsigned char sym)
{
    while(!(UCSRA & (1 << UDRE)));
    UDR = sym;
}

// 1 - previous key pressed
unsigned char key_the_same(void)
{
    PORTD = (PORTD & 0x0f) | (~keyCode & 0xf0);
    asm("nop");
    return (~PINC & (keyCode & 0x0f));
}


// convert keycode
// set flags
unsigned char key_find(void) 
{
    unsigned char i;
    for(i=0; i<13; i++)
        if(keyboard[i][0] == keyCode) {
            keyValue = keyboard[i][1];
            keyDown = 1;
            keyNew = 1;
            return 1;
        }
    return 0;
}

// cycle 0 on D, get keycode from C
void key_scan(void) 
{
    unsigned char row = 0b10000000;
    while((row & 0xf0)) {
        PORTD = (PORTD & 0x0f) | (~row & 0xf0);
        asm("nop");  // required, checked, dunno why
        if((~PINC & 0x0f)) {
            keyCode = (~PINC & 0x0f);
            keyCode |= row;
            break;
        }
        row >>= 1;
    }
}

// 1 - any key pressed
unsigned char key_pressed(void)
{
    PORTD &= 0b00001111;  // set pd4-pd7 to 0
    asm("nop");
    return (~PINC & 0x0f); // check for 0s on pc1-pc3
}

// main cycle
void scan_cycle(void)
{
    switch(keyState) {
    case 0:
        if(key_pressed()) {
            key_scan();
            keyState = 1;
        }
        break;
    case 1:
        if(key_the_same()) {
            // still pressed
            key_find();
            keyState = 2;
        } else
            keyState = 0;
        break;
    case 2:
        if(key_the_same()) {
            // still pressed - wait
        } else
            keyState = 3;
        break;
    case 3:
        if (key_the_same()) {
            // still pressed - loop
            keyState = 2;
        } else {
            keyDown = 0;    // clear all
            keyState = 0;
        }
        break;

    default:
        break;
    }
}

// get key from table
unsigned char key_get(void)
{
    if(keyNew) {
        keyNew = 0;
        return keyValue;
    } else
        return 0;
}

ISR(TIMER1_COMPA_vect)
{
    scan_cycle();
}

ISR(USART_RXC_vect)
{
    rcvd = UDR;
}

int main( void )
{
    unsigned char key;
    keyState = 0;
    keyCode = 0;
    keyValue = 0;
    keyDown = 0;
    keyNew = 0;
    rcvd = 0;

    // usart init
    UBRRL = BAUD_PRESCALE;
    UBRRH = (BAUD_PRESCALE >> 8);
    UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0); // 8N1
    UCSRB = (1 << RXEN) | (1 << TXEN) | (1 << RXCIE); // enable TX & RX, rx interrupt

    //t1 timer
    //OCR1A = 30;  // ~250Hz  / 8mhz (8000000/1024)
    OCR1A = 38;  // ~200Hz  / 8mhz (8000000/1024)
    //OCR1A = 77;  // ~100Hz  / 8mhz (8000000/1024)
    //OCR1A = 156;  // ~50Hz  / 8mhz (8000000/1024)
    //OCR1A = 1562;  // ~5Hz  / 8mhz (8000000/1024)
    TCCR1B |= (1 << WGM12); // Mode 4, CTC on OCR1A
    TIMSK |= (1 << OCIE1A); //Set interrupt on compare match
    TCCR1B |= (1 << CS12) | (1 << CS10); // set prescaler to 1024 and start the timer

    // init C/D
    DDRD |=  0b11110000; // directions, 0/1 - in/out, out for pd4-pd7
    PORTD &= 0b00001111; // set pd4-pd7 to 0
    DDRC &=  0b11110000; // pc0-pc3 - in
    PORTC &= 0b11110000; // pc0-pc3 = 1

    sei(); // enabe ints

    while(1) {
        key = key_get();
        if (key) {
            usart_send('=');
            usart_send(key);
            usart_send(0xff);
        }
        if(rcvd == '?') {
            // reply to 0xff - send 'kbd'+0xff
            usart_send('k');
            usart_send('b');
            usart_send('d');
            usart_send(0xff);
            rcvd = 0;
        }
    }
    return 0;
}
