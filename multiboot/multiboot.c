#include <stdlib.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/quadspi.h>

static void setup_qspi(void) {
  // QSPI Clock
  RCC_AHB3ENR |= RCC_AHB3RSTR_QSPIRST;

  // GPIO Alternate Functions
  gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO2 | GPIO6);
  gpio_mode_setup(GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO8 | GPIO9);
  gpio_mode_setup(GPIOD, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO12 | GPIO13);
  gpio_set_af(GPIOB, GPIO_AF9, GPIO2);
  gpio_set_af(GPIOB, GPIO_AF10, GPIO6);
  gpio_set_af(GPIOC, GPIO_AF9, GPIO8 | GPIO9);
  gpio_set_af(GPIOD, GPIO_AF9, GPIO12 | GPIO13);

  // QSPI Configuration
  QUADSPI_CCR = ((QUADSPI_CCR_FMODE_MEMMAP & QUADSPI_CCR_FMODE_MASK) << QUADSPI_CCR_FMODE_SHIFT) |
                ((QUADSPI_CCR_MODE_4LINE & QUADSPI_CCR_DMODE_MASK) << QUADSPI_CCR_DMODE_SHIFT) |
                ((8 & QUADSPI_CCR_DCYC_MASK) << QUADSPI_CCR_DCYC_SHIFT) |
                ((2 & QUADSPI_CCR_ADSIZE_MASK) << QUADSPI_CCR_ADSIZE_SHIFT) |
                ((QUADSPI_CCR_MODE_1LINE & QUADSPI_CCR_ADMODE_MASK) << QUADSPI_CCR_ADMODE_SHIFT) |
                ((QUADSPI_CCR_MODE_1LINE & QUADSPI_CCR_IMODE_MASK) << QUADSPI_CCR_IMODE_SHIFT) |
                ((0x6b & QUADSPI_CCR_INST_MASK) << QUADSPI_CCR_INST_SHIFT);
  QUADSPI_DCR = ((22 & QUADSPI_DCR_FSIZE_MASK) << QUADSPI_DCR_FSIZE_SHIFT);
  QUADSPI_CR = ((14 & QUADSPI_CR_PRESCALE_MASK) << QUADSPI_CR_PRESCALE_SHIFT) | QUADSPI_CR_EN;
}

static void setup_leds(void) {
  gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO7); // red
  gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO1); // green
  gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO0); // blue
}

static void setup_keyboard(void) {
  // Keyboard columns 1 to 4
  gpio_mode_setup(GPIOC, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GPIO0 | GPIO1 | GPIO2 | GPIO3);

  // Open Drain on used rows
  gpio_mode_setup(GPIOE, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO1 | GPIO7);
  gpio_set_output_options(GPIOE, GPIO_OTYPE_OD, GPIO_OSPEED_2MHZ, GPIO1 | GPIO7);
  gpio_set(GPIOE, GPIO1 | GPIO7);
}

static void set_leds(bool r, bool g, bool b) {
  if(r) gpio_set(GPIOC, GPIO7); else gpio_clear(GPIOC, GPIO7);
  if(g) gpio_set(GPIOB, GPIO1); else gpio_clear(GPIOB, GPIO1);
  if(b) gpio_set(GPIOB, GPIO0); else gpio_clear(GPIOB, GPIO0);
}

static uint8_t read_keyboard(void) {
  gpio_clear(GPIOE, GPIO7);
  uint8_t result = (uint8_t)((~gpio_port_read(GPIOC)) & (GPIO0 | GPIO1 | GPIO2 | GPIO3));
  gpio_set(GPIOE, GPIO7);
  return result;
}

static bool read_home(void) {
  gpio_clear(GPIOE, GPIO1);
  bool result = (bool)((~gpio_port_read(GPIOC)) & GPIO0);
  gpio_set(GPIOE, GPIO1);
  return result;
}

static void program_firmware(int index) {
  uint8_t * my_reset_vector = (uint8_t *) 0x080E0000;
  uint8_t * firmware = (uint8_t *) 0x90000000 + 4096 * 1024 + 1024 * 1024 * index;

  set_leds(true, false, false);

  flash_unlock();

  // erase all but last sector
  for(int i=0; i<11; i++) {
    flash_erase_sector(i, FLASH_CR_PROGRAM_X8);
  }

  set_leds(false, true, false);

  // stack pointer
  for(int i=0;i<4;i++) {
    flash_program_byte(0x08000000+i, firmware[i]);
  }
  // patched reset vector
  for(int i=4;i<8;i++) {
    flash_program_byte(0x08000000+i, my_reset_vector[i]);
  }
  // program all but last sector
  for(int i=8;i<(1024-128)*1024;i++) {
    flash_program_byte(0x08000000+i, firmware[i]);
  }

  // keep original reset vector
  for(int i=4;i<8;i++) {
    flash_program_byte((uint32_t)my_reset_vector - 8 + i, firmware[i]);
  }
}

static void run_firmware(void) {
  void (*entrypoint)(void);
  entrypoint = (void (*)(void)) (*((uint32_t *) 0x080E0000-1));
  entrypoint();
}

int main(void)
{
  rcc_periph_clock_enable(RCC_GPIOB);
  rcc_periph_clock_enable(RCC_GPIOC);
  rcc_periph_clock_enable(RCC_GPIOD);
  rcc_periph_clock_enable(RCC_GPIOE);

  setup_keyboard();

  if(!read_home()) {
    run_firmware();
  } else {
    rcc_clock_setup_hse_3v3(&rcc_hse_25mhz_3v3[RCC_CLOCK_3V3_120MHZ]);

    setup_qspi();
    setup_leds();

    set_leds(false, false, true);

    uint8_t keys = 0;
    do {
      keys = read_keyboard();
    } while(keys==0);

    if(keys & 1) {
      program_firmware(0);
    } else if (keys & 2) {
      program_firmware(1);
    } else if (keys & 4) {
      program_firmware(2);
    } else if (keys & 8) {
      program_firmware(3);
    }

    scb_reset_system();
  }
}
