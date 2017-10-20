#ifndef __QSPI_LOADER_USBCDC_H__
#define __QSPI_LOADER_USBCDC_H__ 1

#include <stdbool.h>
#include <stdint.h>

extern void cdcacm_init(void);
extern uint8_t cdcacm_get_char(void);
extern void cdcacm_put_char(uint8_t);
extern void cdcacm_put_buf(uint8_t *, int);

#endif
