/******************************************************************************
 * dcrawHMod.h: CHIP-Wrapper für dcraw
 ******************************************************************************
 * v1.0 - 18.07.2016
 *
 * Copyright (c) 2016 Tobias Schlosser
 *  (tobias.schlosser@informatik.tu-chemnitz.de)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************/


#ifndef DCRAWHMOD_H
#define DCRAWHMOD_H


#include <stdint.h>

#include "CHIP/Misc/Defines.h"


// "../dcraw.c"
#ifndef uchar
	#define uchar  unsigned char
#endif
#ifndef ushort
	#define ushort unsigned short
#endif

#ifndef u32
	#define u32 uint32_t
#endif


// Vorberechnungen
void dcrawHMod_init(uchar order, float scale);
void dcrawHMod_free();


float sinc_dcrawHMod(float x); // Sinc-Funktion dcrawHMod

// Modi: BL / BC / Lanczos / B-Splines (B_3)
float kernel_dcrawHMod(float x, float y, uchar technique);

/* Hexagonal Interpolation, Hexagonal White Balance (HWB),
    Hexagonal Gamma Correction (HGC) & Hexagonal Filter Banks (HFBs) */
void dcrawHMod(int argc, const char** argv, ushort(** image)[4],
 ushort width, ushort height, const char* ifname);


#endif

