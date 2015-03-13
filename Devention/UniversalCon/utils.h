#ifndef Utils_h
#define Utils_h
#include <stdio.h>
#include <Arduino.h>
#include <avr/pgmspace.h>
#include "Common.h"

// Bit vector from bit position
#define BV(bit) (1 << bit)

u32  rand32_r(u32 *seed, u8 update);
u32  rand32();
void printf(char *fmt, ... );
void printf(const __FlashStringHelper *fmt, ... );

#endif