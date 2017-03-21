#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "pinDefines.h"
#include "macros.h"

#define PROTOCOL_VERSION 0x01

#define CMD_GET_PROTOCOL_VERSION 0x01
#define CMD_SET_SENSOR_COUNT     0x02
#define CMD_GET_SENSOR_COUNT     0x03
#define CMD_GET_SENSOR_READING   0x04
#define CMD_SET_INTERVAL         0x05
#define CMD_TOGGLE_LED           0x06

#define MAX_SENSOR_COUNT 8
unsigned int sensor_count = MAX_SENSOR_COUNT;

volatile unsigned int sensor_data[MAX_SENSOR_COUNT];

// how long to sleep between taking readins, in multiples of 10 ms, so default is 5 x 10ms = 50 ms
volatile unsigned int sleep_between_readings = 5;


void spi_init_slave (void)
{
  // inputs
  SPI_SCK_DDR   &= ~(1 << SPI_SCK);                  /* input on SCK */
  SPI_SS_DDR    &= ~(1 << SPI_SS);                   /* input on SS */
  SPI_MOSI_DDR  &= ~(1 << SPI_MOSI);                 /* input on MOSI */

  // outputs
  SPI_MISO_DDR  |= (1 << SPI_MISO);                  /* output on MISO */

  // set pullup on MOSI
  SPI_MOSI_PORT |= (1 << SPI_MOSI);                  /* pullup on MOSI */

  // enable SPI and SPI interrupt
  SPCR |= ((1 << SPE) | (1 << SPIE));

  SPDR = 0;

  DDRB |= (1 << PB0); // PB0 = output (LED)
}

ISR(SPI_STC_vect) {

  unsigned int data_in = SPDR;

  // first 4 bits are the command number
  unsigned int command = (data_in & 0xF0) >> 4;
  unsigned int index = 0;

  switch (command) {

    case 0x00:
      // after the master sends a valid command, it receives the response on the next 
      // call and sends a zero request for that call
      SPDR = 0x00;
      break;

    case CMD_GET_PROTOCOL_VERSION:
      // get protocol version
      SPDR = PROTOCOL_VERSION;
      break;

    case CMD_SET_SENSOR_COUNT: 
      // set_sensor_count to the value specified in the last 4 bits
      sensor_count = data_in & 0x0F;
      // return the value 0 in response
      SPDR = 0x00;
      break;

    case CMD_GET_SENSOR_COUNT: 
      // get_sensor_count - no parameters, return the current sensor count in response
      SPDR = sensor_count;
      break;

    case CMD_GET_SENSOR_READING:
      // get_sensor_reading - last 4 bits indicates sensor number 0 through 7
      index = data_in & 0x0F;
      if (index<0 || index>=MAX_SENSOR_COUNT) {
        // return error code of 0xFF (255) if the sensor number is not valid
        SPDR = 0xFF;
      } else {
        SPDR = sensor_data[index];
      }
      break;

    case CMD_SET_INTERVAL:
      // set interval between activating each sensor
      sleep_between_readings = data_in & 0x0F;
      SPDR = 0x00;
      break;

    case CMD_TOGGLE_LED:
      // toggle LED
      PORTB ^= (1 << PB0);
      break;

    default:
      // unsupported command so return 0xFF to indicate error condition
      SPDR = 0xFF;
      break;
  }

}

unsigned int poll_sensor(unsigned int i) {
  // trigger
  DDRD |= (1 << i);
  PORTD |= (1 << i);
  _delay_us(10);
  PORTD &= ~(1 << i);

  // set pin to input
  DDRD &= ~(1 << i);

  // loop while echo is LOW
  unsigned int count = 0;
  do {
    if (++count > 1000) {
      break;
    }
  } while (!(PIND & (1 << i)));

  // loop while echo is HIGH
  count = 0;
  do {
    count = count + 1;
    _delay_us(1);

    // stop measuring after some timeout value
    //  5,800 = 100 cm 
    // 14,500 = 250 cm
    if (count > 14500) {
      break;
    }

  } while (PIND & (1 << i));

  return count / 58;
}

int main(void)
{
  // initialize slave SPI
  spi_init_slave();                             
  sei();

  // blink LED to show that the slave is alive
  while (1) {
    PORTB ^= (1 << PB0);
    _delay_ms(100);
  }

/*
  // init all sensors readings to zero
  for (int i=0; i<MAX_SENSOR_COUNT; i++) {
    sensor_data[i] = 0;
  }


  // loop forever, taking readings, and sleeping between each reading
  while(1) {
    for (int i=0; i<MAX_SENSOR_COUNT; i++) {
      sensor_data[i] = poll_sensor(i);

      // sleep for between 0 and 150 ms (intervals of 10 ms)
      for (int i=0; i<sleep_between_readings; i++) {
        _delay_ms(10);
      }
    }
  }
  */
}
