/******************************************************************************
 * dcrawHMod.c: CHIP-Wrapper für dcraw
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>

#include "CHIP/Misc/Defines.h"
#include "CHIP/Misc/Types.h"
#include "CHIP/Misc/pArray2d.h"
#include "CHIP/Misc/Precalcs.h"

#include "CHIP/CHIP/Hexint.h"
#include "CHIP/CHIP/Hexarray.h"
#include "CHIP/CHIP/Hexsamp.h"

#include "CHIP/HFBs/HFBs.h"

#include "dcrawHMod.h"


// Funktionalität aus dcraw (TODO?)

#define SQR(x) (x * x)

ushort HGC_c[0x10000];
float  HGC_g[6] = { 0.45f, 4.5f, 0.0f, 0.0f, 0.0f, 0.0f };

void HGC_c_init(float pwr, float ts, int i_max, bool mode) {
	float b[2] = { 0.0f, 0.0f };
	float g[6] = { pwr, ts, 0.0f, 0.0f, 0.0f, 0.0f }; // power, toe slope, (...)

	bool index = g[1] > 0;
	b[index]   = 1;

	if(g[1] && ((g[0] - 1) * (g[1] - 1)) < 1) {
		for(uchar i = 0; i < 48; i++) {
			g[2] = (b[0] + b[1]) / 2;

			if(g[0]) {
				index    = ((pow(g[2] / g[1], -g[0]) - 1) / g[0] - 1 / g[2]) > -1;
				b[index] = g[2];
			} else {
				index    = (g[2] / exp(1 - 1 / g[2])) < g[1];
				b[index] = g[2];
			}
		}

		g[3] = g[2] / g[1];

		if(g[0])
			g[4] = g[2] * (1 / g[0] - 1);
	}


/*
	if(g[0]) {
		g[5] = 1 / (g[1] * SQR(g[3]) / 2 - g[4] * (1 - g[3]) + (1 - pow(g[3], 1 + g[0])) * (1 + g[4]) / (1 + g[0])) - 1;
	} else {
		g[5] = 1 / (g[1] * SQR(g[3]) / 2 + 1 - g[2] - g[3] - g[2] * g[3] * (log(g[3]) - 1)) - 1;
	}

	// g -> HGC_g
	// if(!mode--) {
	if(!mode) {
		memcpy(HGC_g, g, sizeof(HGC_g));

		return;
	}
*/


	// c -> HGC_c
	for(u32 i = 0; i < 0x10000; i++) {
		const float r = (float)i / i_max;

		if(r < 1) {
			if(!mode) {
				if(r < g[2]) {
					HGC_c[i] = 0x10000 * r / g[1];
				} else {
					if(g[0]) {
						HGC_c[i] = 0x10000 * pow((r + g[4]) / (1 + g[4]), 1 / g[0]);
					} else {
						HGC_c[i] = 0x10000 * exp((r - 1) / g[2]);
					}
				}
			} else {
				if(r < g[3]) {
					HGC_c[i] = 0x10000 * r * g[1];
				} else {
					if(g[0]) {
						HGC_c[i] = 0x10000 * (pow(r, g[0]) * (1 + g[4]) - g[4]);
					} else {
						HGC_c[i] = 0x10000 * (log(r) * g[2] + 1);
					}
				}
			}
		} else {
			HGC_c[i] = 0xffff;
		}
	}
}


// Vorberechnungen

void dcrawHMod_init(uchar order, float scale) {
	precalcs_init(order, scale, 1.0f); // order, scale, radius
}

void dcrawHMod_free() {
	precalcs_free();
}


// Sinc-Funktion dcrawHMod
float sinc_dcrawHMod(float x) {
	return x ? sin(M_PI * x) / (M_PI * x) : 1.0f;
}

// Modi: BL / BC / Lanczos / B-Splines (B_3)
float kernel_dcrawHMod(float x, float y, uchar technique) {
	const fPoint2d abs = { .x = fabs(x), .y = fabs(y) };
	      fPoint2d f;

	// BL (Reichweite angepasst)
	if(!technique) {
		if(abs.x >= 0 && abs.x < 2) {
			f.x = (2 - abs.x) / 2;
		} else {
			f.x = 0.0f;
		}
		if(abs.y >= 0 && abs.y < 2) {
			f.y = (2 - abs.y) / 2;
		} else {
			f.y = 0.0f;
		}
	// BC (Reichweite und Polynom angepasst)
	} else if(technique == 1) {
		const fPoint2d abs_2 = { .x = abs.x * abs.x, .y = abs.y * abs.y };

		if(abs.x >= 0 && abs.x < 2) {
			f.x = 0.25f * abs_2.x * abs.x - 0.75f * abs_2.x + 1;
		} else {
			f.x = 0.0f;
		}
		if(abs.y >= 0 && abs.y < 2) {
			f.y = 0.25f * abs_2.y * abs.y - 0.75f * abs_2.y + 1;
		} else {
			f.y = 0.0f;
		}
	// Lanczos
	} else if(technique == 2) {
		if(abs.x >= 0 && abs.x < 2) {
			f.x = sinc_dcrawHMod(abs.x / 2) * sinc_dcrawHMod(abs.x / 4);
		} else {
			f.x = 0.0f;
		}
		if(abs.y >= 0 && abs.y < 2) {
			f.y = sinc_dcrawHMod(abs.y / 2) * sinc_dcrawHMod(abs.y / 4);
		} else {
			f.y = 0.0f;
		}
	// B-Splines (B_3)
	} else {
		const fPoint2d abs_2 = { .x = abs.x * abs.x, .y = abs.y * abs.y };

		if(abs.x < 1) {
			f.x = (3 * abs_2.x * abs.x - 6 * abs_2.x + 4) / 6;
		} else if(abs.x < 2) {
			f.x = (-abs_2.x * abs.x + 6 * abs_2.x - 12 * abs.x + 8) / 6;
		} else {
			f.x = 0.0f;
		}
		if(abs.y < 1) {
			f.y = (3 * abs_2.y * abs.y - 6 * abs_2.y + 4) / 6;
		} else if(abs.y < 2) {
			f.y = (-abs_2.y * abs.y + 6 * abs_2.y - 12 * abs.y + 8) / 6;
		} else {
			f.y = 0.0f;
		}
	}

	return f.x * f.y;
}

/* Hexagonal Interpolation, Hexagonal White Balance (HWB),
    Hexagonal Gamma Correction (HGC) & Hexagonal Filter Banks (HFBs) */
void dcrawHMod(int argc, const char** argv, ushort(** image)[4],
 ushort width, ushort height, const char* ifname) {
	bool  enable_HMod = false;
	bool  enable_HWB  = false;
	bool  enable_HGC  = false;
	bool  enable_HFBs = false;

	// Parameter
	uchar mode_HMod;
	uchar mode_HWB;
	uchar order;
	float scale;
	float HWB_Y;    // Luminanz
	float HFBs_a;   // Größe des Trägers
	float HFBs_mod; // Intensität


	for(uchar i = 0; i < argc; i++) {
		if(!strcmp(argv[i], "HMod")) {
			enable_HMod = true;

			mode_HMod = (uchar)atoi(argv[++i]);
			order     = (uchar)atoi(argv[++i]);
			scale     = (float)atof(argv[++i]);
		}
		if(!strcmp(argv[i], "HWB")) {
			enable_HWB = true;


			mode_HWB = (uchar)atoi(argv[++i]);

			if(mode_HWB == 8)
				HWB_Y = (float)atof(argv[++i]);
		}
		if(!strcmp(argv[i], "HGC")) {
			enable_HGC = true;
		}
		if(!strcmp(argv[i], "HFBs")) {
			enable_HFBs = true;

			HFBs_a   = (float)atof(argv[++i]);
			HFBs_mod = (float)atof(argv[++i]);
		}
	}


	if(enable_HMod) {
		printf(
			"\nHMod [info]: width = %u, height = %u,\n"
			" enable_HMod = %u, enable_HWB = %u, enable_HGC = %u, enable_HFBs = %u,\n"
			" mode_HMod = %u, order = %u, scale = %.2f",
			 width, height,
			 enable_HMod, enable_HWB, enable_HGC, enable_HFBs,
			 mode_HMod, order, scale);

		if(enable_HWB) {
			printf("\n -> HWB: mode_HWB = %u", mode_HWB);

			if(mode_HWB == 8)
				printf(", HWB_Y = %.2f", HWB_Y);
		}

		if(enable_HFBs)
			printf("\n -> HFBs: HFBs_a = %.2f, HFBs_mod = %.2f", HFBs_a, HFBs_mod);

		puts("\n");


		dcrawHMod_init(order, scale);


		/*
		 * Hexagonal Interpolation
		 */

		/**
		 * 0:  Nearest: reals
		 * 1:  Nearest: spatials
		 * 2:  CHIP: bilinear
		 * 3:  CHIP: bicubic
		 * 4:  CHIP: Lanczos
		 * 5:  CHIP: B-Splines (B_3)
		 * 6:  Pixel averaging
		 * 7:  Bilinear
		 * 8:  Bicubic
		 * 9:  Lanczos
		 * 10: B-Splines (B_3)
		 * 11: Hex. AVG (2R4G2B)
		 */

		RGB_Hexarray rgb_hexarray;

		// Nearest: reals / spatials
		if(!mode_HMod || mode_HMod == 1) {
			const fPoint2d mid = { .x = width / 2.0f, .y = height / 2.0f };

			Hexarray_init(&rgb_hexarray, order, 0);

			for(u32 i = 0; i < rgb_hexarray.size; i++) {
				ushort row;
				ushort col;

				if(!mode_HMod) {
					row = (ushort)roundf(mid.y - scale * pc_reals[i][1]);
					col = (ushort)roundf(mid.x + scale * pc_reals[i][0]);
				} else {
					const fPoint2d ps = getSpatial(Hexint_init(i, 0));

					row = (ushort)roundf(mid.y - scale * ps.y);
					col = (ushort)roundf(mid.x + scale * ps.x);
				}

				if(row < height && col < width) {
					const u32 p = row * width + col;

					rgb_hexarray.p[i][0] = (int)(*image)[p][0] >> 8;
					rgb_hexarray.p[i][1] = (int)(*image)[p][1] >> 8; // G1 / G2
					rgb_hexarray.p[i][2] = (int)(*image)[p][2] >> 8;
				}
			}
		// CHIP: bilinear / bicubic / Lanczos / B-Splines (B_3)
		} else if(mode_HMod == 2 || mode_HMod == 3 || mode_HMod == 4 || mode_HMod == 5) {
			RGB_Array rgb_array;

			pArray2d_init(&rgb_array, width, height);

			for(ushort h = 0; h < height; h++) {
				for(ushort w = 0; w < width; w++) {
					const u32 p = h * width + w;

					rgb_array.p[w][h][0] = (int)(*image)[p][0] >> 8;
					rgb_array.p[w][h][1] = (int)(*image)[p][1] >> 8; // G1 / G2
					rgb_array.p[w][h][2] = (int)(*image)[p][2] >> 8;
				}
			}

			hipsampleColour(rgb_array, &rgb_hexarray, order, scale, mode_HMod - 2);

			pArray2d_free(&rgb_array);
		// Pixel averaging
		} else if(mode_HMod == 6) {
			const fPoint2d mid = { .x = width / 2.0f, .y = height / 2.0f };

			Hexarray_init(&rgb_hexarray, order, 0);

			for(u32 i = 0; i < rgb_hexarray.size; i++) {
				const ushort r = (ushort)roundf(mid.y - scale * pc_reals[i][1]);
				const ushort c = (ushort)roundf(mid.x + scale * pc_reals[i][0]);

				if(r && r < height - 1 && c && c < width - 1) {
					const uchar color = (r % 2) * 2 + (c % 2); // 0-3 = R, G1, G2, B
					float RGB[3];

					// R
					if(!color) {
						RGB[0] = (float)(*image)[r * width + c][0];
						RGB[1] = ( (*image)[(r - 1) * width + c][1] + (*image)[r * width + c - 1][1] + \
						           (*image)[(r + 1) * width + c][1] + (*image)[r * width + c + 1][1] ) / 4.0f;
						RGB[2] = ( (*image)[(r - 1) * width + c - 1][2] + (*image)[(r - 1) * width + c + 1][2] + \
						           (*image)[(r + 1) * width + c - 1][2] + (*image)[(r + 1) * width + c + 1][2] ) / 4.0f;
					// G1
					} else if(color == 1) {
						RGB[0] = ( (*image)[r * width + c - 1][0]   + (*image)[r * width + c + 1][0] ) / 2.0f;
						RGB[1] = (float)(*image)[r * width + c][1];
						RGB[2] = ( (*image)[(r - 1) * width + c][2] + (*image)[(r + 1) * width + c][2] ) / 2.0f;
					// G2
					} else if(color == 2) {
						RGB[0] = ( (*image)[(r - 1) * width + c][0] + (*image)[(r + 1) * width + c][0] ) / 2.0f;
						RGB[1] = (float)(*image)[r * width + c][1];
						RGB[2] = ( (*image)[r * width + c - 1][2]   + (*image)[r * width + c + 1][2] ) / 2.0f;
					// B
					} else {
						RGB[0] = ( (*image)[(r - 1) * width + c - 1][0] + (*image)[(r - 1) * width + c + 1][0] + \
						           (*image)[(r + 1) * width + c - 1][0] + (*image)[(r + 1) * width + c + 1][0] ) / 4.0f;
						RGB[1] = ( (*image)[(r - 1) * width + c][1] + (*image)[r * width + c - 1][1] + \
						           (*image)[(r + 1) * width + c][1] + (*image)[r * width + c + 1][1] ) / 4.0f;
						RGB[2] = (float)(*image)[r * width + c][2];
					}

					rgb_hexarray.p[i][0] = (int)roundf(RGB[0]) >> 8;
					rgb_hexarray.p[i][1] = (int)roundf(RGB[1]) >> 8; // G1 / G2
					rgb_hexarray.p[i][2] = (int)roundf(RGB[2]) >> 8;
				}
			}
		// Bilinear / bicubic / Lanczos / B-Splines (B_3)
		} else if(mode_HMod == 7 || mode_HMod == 8 || mode_HMod == 9 || mode_HMod == 10) {
			const fPoint2d mid = { .x = width / 2.0f, .y = height / 2.0f };

			Hexarray_init(&rgb_hexarray, order, 0);

			for(u32 i = 0; i < rgb_hexarray.size; i++) {
				const ushort row      = (ushort)roundf(mid.y - scale * pc_reals[i][1]);
				const ushort col      = (ushort)roundf(mid.x + scale * pc_reals[i][0]);
				const float  row_hex  =                mid.y - scale * pc_reals[i][1];
				const float  col_hex  =                mid.x + scale * pc_reals[i][0];
				      float  RGB[3]   = { 0.0f, 0.0f, 0.0f };
				      float  RGB_n[3] = { 0.0f, 0.0f, 0.0f };

				if(row > 1 && row < height - 2 && col > 1 && col < width - 2) {
					// 6×6 = 36
					for(u32 x = roundf(col - 2); x < roundf(col + 3); x++) {
						for(u32 y = roundf(row - 2); y < roundf(row + 3); y++) {
							const uchar color = (y % 2) * 2 + (x % 2); // 0-3 = R, G1, G2, B
							const float xh    = fabs(col_hex - x);
							const float yh    = fabs(row_hex - y);
							      float k     = kernel_dcrawHMod(xh, yh, mode_HMod - 7);


							// Patch Transformation: Flächeninhalt
							if((xh > 1.5f && xh < 2.0f) || (yh > 1.5f && yh < 2.0f)) {
								float factor = 1.0f;

								if(xh > 1.5f)
									factor *= (xh - (u32)xh);

								if(yh > 1.5f)
									factor *= (yh - (u32)yh);

								k *= factor;
							}


							// R
							if(!color) {
								RGB[0]   += k * (*image)[y * width + x][0];
								RGB_n[0] += k;
							// G1 / G2
							} else if(color == 1 || color == 2) {
								RGB[1]   += k * (*image)[y * width + x][1];
								RGB_n[1] += k;
							// B
							} else {
								RGB[2]   += k * (*image)[y * width + x][2];
								RGB_n[2] += k;
							}
						}
					}

					rgb_hexarray.p[i][0] = (int)roundf(RGB[0] / RGB_n[0]) >> 8;
					rgb_hexarray.p[i][1] = (int)roundf(RGB[1] / RGB_n[1]) >> 8; // G1 / G2
					rgb_hexarray.p[i][2] = (int)roundf(RGB[2] / RGB_n[2]) >> 8;
				}
			}
		// Hex. AVG (2R4G2B)
		} else {
			uPoint2d mid = { .x = width / 2, .y = height / 2 };
			uPoint2d ps, ps_bak; fPoint2d tmp;

			// mid -> G1
			if(!(mid.x % 2)) mid.x += 1;
			if(  mid.y % 2 ) mid.y += 1;


			Hexarray_init(&rgb_hexarray, order, 0);


			for(u32 p = 0; p < rgb_hexarray.size; p++) {
				u32 RGB[3] = { 0, 0, 0 };

				// Mittelpunkt des aktuellen Blocks
				tmp      = getSpatial(Hexint_init(p, 0));
				ps.x     = (u32)tmp.x;
				ps.y     = (u32)tmp.y;
				ps_bak.x = mid.x + ps.x * 3 - ps.y * 2;
				ps_bak.y = mid.y - ps.x     - ps.y * 2;

				for(uchar subp = 0; subp < 8; subp++) {
					ps = ps_bak;

					switch(subp) {
						case 0:            ps.y -= 1; break; // z. B. B
						case 1: ps.x -= 1;            break; // R
						case 2:                       break; // G1
						case 3: ps.x += 1;            break; // R
						case 4: ps.x -= 1; ps.y += 1; break; // G2
						case 5:            ps.y += 1; break; // B
						case 6: ps.x += 1; ps.y += 1; break; // G2
						case 7:            ps.y += 2; break; // G1
						default:                      break;
					}


					const uchar color = ((uchar)ps.y % 2) * 2 + ((uchar)ps.x % 2); // 0-3 = R, G1, G2, B

					if(ps.y * width + ps.x < height * width) {
						// R
						if(!color) {
							RGB[0] += (*image)[ps.y * width + ps.x][0];
						// G1 / G2
						} else if(color == 1 || color == 2) {
							RGB[1] += (*image)[ps.y * width + ps.x][1];
						// B
						} else {
							RGB[2] += (*image)[ps.y * width + ps.x][2];
						}
					} else {
						break;
					}
				}

				rgb_hexarray.p[p][0] = (int)roundf(RGB[0] / 2) >> 8;
				rgb_hexarray.p[p][1] = (int)roundf(RGB[1] / 4) >> 8; // G1 / G2
				rgb_hexarray.p[p][2] = (int)roundf(RGB[2] / 2) >> 8;
			}
		}


		/*
		 * Hexagonal Gamma Correction (HGC) (TODO?)
		 */

		if(enable_HGC) {
			HGC_c_init(HGC_g[0], HGC_g[1], 30000, 1);

			for(u32 i = 0; i < rgb_hexarray.size; i++) {
				rgb_hexarray.p[i][0] = HGC_c[rgb_hexarray.p[i][0] << 8] >> 8;
				rgb_hexarray.p[i][1] = HGC_c[rgb_hexarray.p[i][1] << 8] >> 8;
				rgb_hexarray.p[i][2] = HGC_c[rgb_hexarray.p[i][2] << 8] >> 8;
			}
		}


		/*
		 * Hexagonal White Balance (HWB)
		 */

		/**
		 * 0: Y *=  1.1f;
		 * 1: Y  = <ALL_MAX>
		 * 2: Y  = <ALL_AVG>
		 * 3: Y  = <49P_MAX>
		 * 4: Y  = <49P_AVG>
		 * 5: Y  =  <7P_MAX>
		 * 6: Y  =  <7P_AVG>
		 * 7: Y  = 64.0f;
		 * 8: Y  = <HWB_Y>
		 */

		if(enable_HWB) {
			int R_new, G_new, B_new;

			for(u32 i = 0; i < rgb_hexarray.size; i++) {
				if(rgb_hexarray.p[i][0] + rgb_hexarray.p[i][1] + rgb_hexarray.p[i][2] > R_new + G_new + B_new) {
					R_new = rgb_hexarray.p[i][0];
					G_new = rgb_hexarray.p[i][1];
					B_new = rgb_hexarray.p[i][2];
				}
			}

			// 192 = (2^8 + 2^7) / 2
			for(u32 i = 0; i < rgb_hexarray.size; i++) {
				rgb_hexarray.p[i][0] = (int)roundf(rgb_hexarray.p[i][0] * 192.0f / R_new);
				rgb_hexarray.p[i][1] = (int)roundf(rgb_hexarray.p[i][1] * 192.0f / G_new);
				rgb_hexarray.p[i][2] = (int)roundf(rgb_hexarray.p[i][2] * 192.0f / B_new);
			}


			const uchar Y_size = (mode_HWB == 5 || mode_HWB == 6) ? 7 : 49;
			float Y, Y_new = 0.0f;

			// ALL_MAX / ALL_AVG
			if(mode_HWB == 1 || mode_HWB == 2) {
				for(u32 i = 0; i < rgb_hexarray.size; i++) {
					Y = 0.256788f * rgb_hexarray.p[i][0] + \
					    0.504129f * rgb_hexarray.p[i][1] + \
					    0.097906f * rgb_hexarray.p[i][2] + 16;

					// ALL_MAX
					if(mode_HWB == 1) {
						if(Y > Y_new)
							Y_new = Y;
					// ALL_AVG
					} else {
						Y_new += Y;
					}
				}

				// ALL_AVG
				if(mode_HWB == 2)
					Y_new /= rgb_hexarray.size;
			}

			for(u32 i = 0; i < rgb_hexarray.size; i++) {
				int R = rgb_hexarray.p[i][0];
				int G = rgb_hexarray.p[i][1];
				int B = rgb_hexarray.p[i][2];

				if(mode_HWB < 3)
					Y = 0.256788f * R + 0.504129f * G + 0.097906f * B + 16;

				const float Cb = -0.148223f * R - 0.290993f * G + 0.439216f * B + 128;
				const float Cr =  0.439216f * R - 0.367788f * G - 0.071427f * B + 128;


				// Y = ALL_MAX / ALL_AVG
				if(mode_HWB == 1 || mode_HWB == 2) {
					Y *= (192 / Y_new);
				// Y = 49P_MAX / 49P_AVG / 7P_MAX / 7P_AVG
				} else if(mode_HWB > 2 && mode_HWB < 7) {
					if(!(i % Y_size)) {
						for(u32 j = i; j < i + Y_size; j++) {
							Y = 0.256788f * rgb_hexarray.p[j][0] + \
							    0.504129f * rgb_hexarray.p[j][1] + \
							    0.097906f * rgb_hexarray.p[j][2] + 16;

							// 49P_MAX / 7P_MAX
							if(mode_HWB == 3 || mode_HWB == 5) {
								if(Y > Y_new)
									Y_new = Y;
							// 49P_AVG / 7P_AVG
							} else {
								Y_new += Y;
							}
						}

						if(mode_HWB == 3 || mode_HWB == 5) {
							Y *= (192 /  Y_new);
						} else {
							Y *= (192 / (Y_new / Y_size));
						}
					}
				} else if(mode_HWB == 7) {
					Y  = 64.0f;
				} else if(mode_HWB == 8) {
					Y  = HWB_Y;
				} else {
					Y *=  1.1f;
				}

				if(mode_HWB > 2)
					Y_new = 0.0f;


				const float C = 1.164383f * (Y  -  16);
				const float D =              Cb - 128;
				const float E =              Cr - 128;

				R = (int)roundf( C                 + 1.596027f * E );
				G = (int)roundf( C - 0.391762f * D - 0.812968f * E );
				B = (int)roundf( C + 2.017232f * D                 );

				if(R < 0) {
					rgb_hexarray.p[i][0] =   0;
				} else if(R > 255) {
					rgb_hexarray.p[i][0] = 255;
				} else {
					rgb_hexarray.p[i][0] = R;
				}
				if(G < 0) {
					rgb_hexarray.p[i][1] =   0;
				} else if(G > 255) {
					rgb_hexarray.p[i][1] = 255;
				} else {
					rgb_hexarray.p[i][1] = G;
				}
				if(B < 0) {
					rgb_hexarray.p[i][2] =   0;
				} else if(B > 255) {
					rgb_hexarray.p[i][2] = 255;
				} else {
					rgb_hexarray.p[i][2] = B;
				}
			}
		}


		/*
		 * Hexagonal Filter Banks (HFBs)
		 */

		/**
		 * <HFBs_a=(>0.0f)> (Größe des Trägers): typischerweise 2 oder 3,
		 *  <HFBs_mod=(>0.0f)> (Intensität): (<1.0f) = blurring*, sonst = un-
		 */

		if(enable_HFBs)
			filter_lanczos(&rgb_hexarray, HFBs_a, HFBs_mod);


		char ofname[64]; // out file name

		strcpy(ofname, ifname);
		ofname[strlen(ofname) - 4] = '\0';
		strcat(ofname, "-HIP.dat");
		Hexarray2file(&rgb_hexarray, ofname, 0);

		// Weitere Ausgaben als PNG
		strcpy(ofname, ifname);
		ofname[strlen(ofname) - 4] = '\0';
		strcat(ofname, "-HIP_1D.png");
		Hexarray2PNG_1D(rgb_hexarray, ofname);
		strcpy(ofname, ifname);
		ofname[strlen(ofname) - 4] = '\0';
		strcat(ofname, "-HIP_2D.png");
		Hexarray2PNG_2D(rgb_hexarray, ofname);
		strcpy(ofname, ifname);
		ofname[strlen(ofname) - 4] = '\0';
		strcat(ofname, "-HIP_2D_directed.png");
		Hexarray2PNG_2D_directed(rgb_hexarray, ofname);

		Hexarray_free(&rgb_hexarray, 0);


		dcrawHMod_free();
	}
}

