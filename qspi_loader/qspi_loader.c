#include <stdlib.h>
#include "cdcacm.h"
#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/quadspi.h>
#include "../flashrom/serprog.h"
#include "../flashrom/flash.h"

#define S_CMD_MAP ( \
  (1 << S_CMD_NOP)       | \
  (1 << S_CMD_Q_IFACE)   | \
  (1 << S_CMD_Q_CMDMAP)  | \
  (1 << S_CMD_Q_PGMNAME) | \
  (1 << S_CMD_Q_SERBUF)  | \
  (1 << S_CMD_Q_BUSTYPE) | \
  (1 << S_CMD_SYNCNOP)   | \
  (1 << S_CMD_S_BUSTYPE) | \
  (1 << S_CMD_O_SPIOP)   | \
  (1 << S_CMD_S_SPI_FREQ) \
)

static void cdcacm_to_spi(int len) {
  if(len > 0) {
    while(QUADSPI_SR & QUADSPI_SR_BUSY);
    QUADSPI_DLR = len - 1;
    while(QUADSPI_SR & QUADSPI_SR_BUSY);
    QUADSPI_CCR &= ~(QUADSPI_CCR_FMODE_MASK << QUADSPI_CCR_FMODE_SHIFT);
    for(int i=0;i<len;i++) {
      QUADSPI_BYTE_DR = cdcacm_get_char();
    }
    while(QUADSPI_SR & QUADSPI_SR_BUSY);
  }
}

static void spi_to_cdcacm_fast(int len) {
  if(len > 0) {
    while(QUADSPI_SR & QUADSPI_SR_BUSY);
    QUADSPI_DLR = len - 1;
    while(QUADSPI_SR & QUADSPI_SR_BUSY);
    QUADSPI_CCR |= (1 & QUADSPI_CCR_FMODE_MASK) << QUADSPI_CCR_FMODE_SHIFT;
    uint8_t buf[16];
    for(int i=0;i<len/16;i++) {
      for(int j=0;j<16;j++) {
        buf[j]=QUADSPI_BYTE_DR;
      }
      cdcacm_put_buf(buf, 16);
    }
    for(int i=0;i<len%16;i++) {
      cdcacm_put_char(QUADSPI_BYTE_DR);
    }
    while(QUADSPI_SR & QUADSPI_SR_BUSY);
  }
}

static void setup_qspi(void) {
  // GPIO Clock
  rcc_periph_clock_enable(RCC_GPIOB);
  rcc_periph_clock_enable(RCC_GPIOC);
  rcc_periph_clock_enable(RCC_GPIOD);

  // QSPI Clock
  RCC_AHB3ENR |= RCC_AHB3RSTR_QSPIRST;

  // GPIO Alternate Functions
  gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO2);
  gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO6); // Need to control #CS manually
  gpio_mode_setup(GPIOC, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GPIO8); // #WR needs to be up
  gpio_mode_setup(GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO9);
  gpio_mode_setup(GPIOD, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO12);
  gpio_mode_setup(GPIOD, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GPIO13); // #HOLD needs to be up
  gpio_set_af(GPIOB, GPIO_AF9, GPIO2);
  gpio_set(GPIOB, GPIO6);
  gpio_set_af(GPIOC, GPIO_AF10, GPIO8);
  gpio_set_af(GPIOC, GPIO_AF9, GPIO9);
  gpio_set_af(GPIOD, GPIO_AF9, GPIO12);

  // QSPI Configuration
  while(QUADSPI_SR & QUADSPI_SR_BUSY);
  QUADSPI_CCR |= ((1 & QUADSPI_CCR_DMODE_MASK) << QUADSPI_CCR_DMODE_SHIFT);
  while(QUADSPI_SR & QUADSPI_SR_BUSY);
  QUADSPI_DCR |= ((0xff & QUADSPI_DCR_FSIZE_MASK) << QUADSPI_DCR_FSIZE_SHIFT);
  while(QUADSPI_SR & QUADSPI_SR_BUSY);
  QUADSPI_CR |= ((14 & QUADSPI_CR_PRESCALE_MASK) << QUADSPI_CR_PRESCALE_SHIFT) | QUADSPI_CR_EN;
}

static void process_command(uint8_t command) {
  uint32_t slen, rlen, freq, prescaler;
  switch(command) {
    case S_CMD_NOP:
      cdcacm_put_char(S_ACK);
      break;

    case S_CMD_Q_IFACE:
      cdcacm_put_char(S_ACK);
      cdcacm_put_char(0x01);
      cdcacm_put_char(0x00);
      break;

    case S_CMD_Q_CMDMAP:
      cdcacm_put_char(S_ACK);
      cdcacm_put_char(S_CMD_MAP & 0xff);
      cdcacm_put_char((S_CMD_MAP >> 8) & 0xff);
      cdcacm_put_char((S_CMD_MAP >> 16) & 0xff);
      cdcacm_put_char((S_CMD_MAP >> 24) & 0xff);
      for(int i=0;i<28;i++) {
        cdcacm_put_char(0x00);
      }
      break;

    case S_CMD_Q_PGMNAME:
      cdcacm_put_char(S_ACK);
      cdcacm_put_buf((uint8_t *) "zardam's serprog", 16);
      break;

    case S_CMD_Q_SERBUF:
      cdcacm_put_char(S_ACK);
      cdcacm_put_char(0x10);
      cdcacm_put_char(0x00);
      break;

    case S_CMD_Q_BUSTYPE:
      cdcacm_put_char(S_ACK);
      cdcacm_put_char(BUS_SPI);
      break;

    case S_CMD_SYNCNOP:
      cdcacm_put_char(S_NAK);
      cdcacm_put_char(S_ACK);
      break;

    case S_CMD_O_SPIOP:
      slen  = (uint32_t) cdcacm_get_char();
      slen |= (uint32_t) cdcacm_get_char() << 8;
      slen |= (uint32_t) cdcacm_get_char() << 16;
      rlen  = (uint32_t) cdcacm_get_char();
      rlen |= (uint32_t) cdcacm_get_char() << 8;
      rlen |= (uint32_t) cdcacm_get_char() << 16;
      cdcacm_put_char(S_ACK);
      gpio_clear(GPIOB, GPIO6);
      cdcacm_to_spi(slen);
      spi_to_cdcacm_fast(rlen);
      gpio_set(GPIOB, GPIO6);
      break;

    case S_CMD_S_BUSTYPE:
      if(cdcacm_get_char() == BUS_SPI) {
        cdcacm_put_char(S_ACK);
      } else {
        cdcacm_put_char(S_NAK);
      }
      break;

    case S_CMD_S_SPI_FREQ:
      cdcacm_put_char(S_ACK);
      freq  = (uint32_t) cdcacm_get_char();
      freq |= (uint32_t) cdcacm_get_char() << 8;
      freq |= (uint32_t) cdcacm_get_char() << 16;
      freq |= (uint32_t) cdcacm_get_char() << 24;
      prescaler = 120000000/freq-1;
      if(prescaler > 255) prescaler = 255;
      freq = 120000000/(prescaler+1);
      while(QUADSPI_SR & QUADSPI_SR_BUSY);
      QUADSPI_CR = (QUADSPI_CR & ~(QUADSPI_CR_PRESCALE_MASK << QUADSPI_CR_PRESCALE_SHIFT)) | ((prescaler & QUADSPI_CR_PRESCALE_MASK) << QUADSPI_CR_PRESCALE_SHIFT);
      cdcacm_put_char(freq & 0xff);
      cdcacm_put_char((freq >> 8) & 0xff);
      cdcacm_put_char((freq >> 16) & 0xff);
      cdcacm_put_char((freq >> 24) & 0xff);
      break;
    
    case 'q':
      scb_reset_system();
      break;

    default:
      cdcacm_put_char(S_NAK);
      break;
  }
}

int main(void)
{
  // Relocate vector table as we run from SRAM
  extern int vector_table;
  SCB_VTOR = (uint32_t)&vector_table;

  rcc_clock_setup_hse_3v3(&rcc_hse_25mhz_3v3[RCC_CLOCK_3V3_120MHZ]);

  setup_qspi();

  cdcacm_init();

  while(1) {
    process_command(cdcacm_get_char());
  }
}
