#pragma once
#define PROGMEM
#define PSTR(s) (s)
#define pgm_read_byte(addr) (*(const uint8_t *)(addr))
