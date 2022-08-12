#include <avr/io.h>
#include "usb_serial.h"

#define LED_CONFIG (DDRD |= (1<<6))
#define LED_ON (PORTD |= (1<<6))
#define LED_OFF (PORTD &= ~(1<<6))
#define CPU_PRESCALE(n) (CLKPR = 0x80, CLKPR = (n))

int8_t detect_rotation(uint8_t NEW, uint8_t OLD) {
  // spinners use a 2 bit number read from the JAMMA pins for left and
  // right cycles through states 0-1-2-0 etc. to identify rotation
  // note: function assumes SOCD cleaning of input parameters so 3 is
  // never encountered
  if (NEW == OLD){
    return 0;
  } else if ((NEW == 0 && OLD == 1) ||
    (NEW == 1 && OLD == 2) ||
    (NEW == 2 && OLD == 0)) {
    return -1;
  } else if ((NEW == 2 && OLD == 1) ||
    (NEW == 1 && OLD == 0) ||
    (NEW == 0 && OLD == 2)) {
    return 1;
  }
  return 128;
}

int main(void) {
  CPU_PRESCALE(0);
  usb_init();
  LED_CONFIG;
  LED_OFF;

  DDRA = 0x00;
  DDRB = 0x00;
  DDRC = 0x00;
  PORTA = 0xFF;
  PORTB = 0xFF;
  PORTC = 0xFF;

  uint8_t PINA_NEW = 0x00;
  uint8_t PINB_NEW = 0x00;
  uint8_t PINC_NEW = 0x00;
  uint8_t PINA_OLD = 0x00;
  uint8_t PINB_OLD = 0x00;
  uint8_t PINC_OLD = 0x00;

  uint32_t ANGLE1 = 0x00000000;
  uint32_t ANGLE2 = 0x00000000;

  uint8_t SPIN1_NEW = 0x00;
  uint8_t SPIN2_NEW = 0x00;
  uint8_t SPIN1_OLD = 0x00;
  uint8_t SPIN2_OLD = 0x00;

  int HANDSHAKED = 0;
  int INPUT_CHANGED = 0;

  while (1) {
    LED_ON;

    // initiate a handshake on Data Terminal Ready (DTR) signal
    if ((!HANDSHAKED && (usb_serial_get_control() & USB_SERIAL_DTR))) {
      if (usb_serial_getchar() == '6') {
        usb_serial_putchar('9');
        usb_serial_flush_output();
        HANDSHAKED = 1;
        INPUT_CHANGED = 1; // or rather, it's new to the Data Terminal
      }
    }
    // invalidate handshake if DTR signal no longer present
    else if (!(usb_serial_get_control() & USB_SERIAL_DTR)) {
        HANDSHAKED = 0;
    }

    // take a snapshot of the pins
    PINA_NEW = ~PINA; // inverted bits
    PINB_NEW = PINB;
    PINC_NEW = ~PINC; // inverted bits

    // SOCD cleaning
    if ((PINB_NEW & 0x03) == 0x03) { // p1-ud
      PINB_NEW -= 0x03;
      PINB_NEW += (PINB_OLD & 0x03);
    }
    if ((PINB_NEW & 0x0C) == 0x0C) { // p1-lr
      PINB_NEW -= 0x0C;
      PINB_NEW += (PINB_OLD & 0x0C);
    }
    if ((PINB_NEW & 0x30) == 0x30) { // p2-up
      PINB_NEW -= 0x30;
      PINB_NEW += (PINB_OLD & 0x30);
    }
    if ((PINB_NEW & 0xC0) == 0xC0) { // p2-lr
      PINB_NEW -= 0xC0;
      PINB_NEW += (PINB_OLD & 0xC0);
    }

    // change detected
    if (PINA_NEW != PINA_OLD || PINB_NEW != PINB_OLD || PINC_NEW != PINC_OLD) {
      INPUT_CHANGED = 1;
    }

    if (INPUT_CHANGED && HANDSHAKED) {

      // update spinner states
      if (PINB_NEW != PINB_OLD) {
        // p1 spinner
        SPIN1_NEW = ((PINB_OLD >> 2) & 0x03);
        ANGLE1 += detect_rotation(SPIN1_NEW, SPIN1_OLD);
        if (ANGLE1 == 1458) {
          ANGLE1 = 0;
        } else if (ANGLE1 == -1) {
          ANGLE1 = 1457;
        }
        // p2 spinner
        SPIN2_NEW = ((PINB_OLD >> 6) & 0x03);
        ANGLE2 += detect_rotation(SPIN2_NEW, SPIN2_OLD);
        if (ANGLE2 == 1458) {
          ANGLE2 = 0;
        } else if (ANGLE2 == -1) {
          ANGLE2 = 1457;
        }
      }

      // send state to PC
      if (usb_configured()) {
        usb_serial_putchar(PINA_NEW); // buttons: p1-abcd p2-abcd
        usb_serial_putchar(PINB_NEW); // sticks: p1-udlr p2-udlr
        usb_serial_putchar(PINC_NEW); // misc: start1 start2 coin1 coin2 service test
        usb_serial_putchar((uint8_t)0x00); // kick: p1-def p2-def
        usb_serial_putchar((uint8_t)((ANGLE1 & 0x00FF0000) >> 16)); // spinner 1: oriention ranging 0-1457, sent over 3 bytes
        usb_serial_putchar((uint8_t)((ANGLE1 & 0x0000FF00) >> 8));
        usb_serial_putchar((uint8_t)(ANGLE1 & 0x000000FF));
        usb_serial_putchar((uint8_t)((ANGLE2 & 0x00FF0000) >> 16)); // spinner 2: oriention ranging 0-1457, sent over 3 bytes
        usb_serial_putchar((uint8_t)((ANGLE2 & 0x0000FF00) >> 8));
        usb_serial_putchar((uint8_t)(ANGLE2 & 0x000000FF));
        usb_serial_putchar('\n'); // terminator
        usb_serial_flush_output();
      }

      // update reference values
      PINA_OLD = PINA_NEW;
      PINB_OLD = PINB_NEW;
      PINC_OLD = PINC_NEW;
      SPIN1_OLD = SPIN1_NEW;
      SPIN2_OLD = SPIN2_NEW;
      INPUT_CHANGED = 0;
    }
  }
}
