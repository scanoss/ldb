// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * src/hex.c
 *
 * Hexadecimal and numeric conversions
 *
 * Copyright (C) 2018-2020 SCANOSS.COM
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

void ldb_hexprint(uint8_t *data, uint32_t len, uint8_t width)
{
	uint8_t b16[] = "0123456789abcdef";
	for (int i = 0; i <= width * (int)((len + width) / width); i++)
		if (i && !(i % width))
		{
			printf("%04d  ", i - width);
			for (int t = i - width; t < i; t++)
				printf("%c%c", t < len ? b16[(data[t] & 0xF0) >> 4] : 32, t < len ? b16[data[t] & 0x0F] : 32);
			printf("  ");
			for (int t = i - width; t < i; t++)
				printf("%c", t < len ? ((data[t] > 31 && data[t] < 127) ? data[t] : 46) : 32);
			printf("\n");
			if (i == len)
				break;
		}
}

bool ldb_hexprint16(uint8_t *key, uint8_t *subkey, int subkey_ln, uint8_t *data, uint32_t len, int iteration, void *ptr)
{
	for (int i = 0; i < LDB_KEY_LN; i++) printf("%02x", key[i]);
	for (int i = 0; i < subkey_ln; i++)  printf("%02x", subkey[i]);
	printf("\n");
	ldb_hexprint(data, len, 16);
	printf("\n");
	return false;
}

/* Converts hex to bin */
void ldb_hex_to_bin(char *hex, int len, uint8_t *out)
{
	uint32_t ptr = 0;
	char pair[] = "\0\0\0";
	for (uint32_t i = 0; i < len; i += 2)
	{
		pair[0] = hex[i];
		pair[1] = hex[i + 1];
		out[ptr++] = strtol(pair, NULL, 16);
	}
}

/* Converts bin to hex */
void ldb_bin_to_hex(uint8_t *bin, uint32_t len, char *out)
{
	*out = 0;
	for (uint32_t i = 0; i < len; i++)
		sprintf(out + strlen(out), "%02x", bin[i]);
}

bool ldb_valid_hex(char *str)
{
	if (strlen(str) % 2) return false;
	if (strlen(str) < 2) return false;
	for (int i = 0; i < strlen(str); i++) 
	{
		char h = str[i];
		if (h < '0' || (h > '9' && h < 'a') || h > 'f') return false;
	}
	return true;
}

/* Write an unsigned long integer (40-bit) in the provided ldb_sector at the current location */
void ldb_uint40_write(FILE *ldb_sector, uint64_t value)
{
	fwrite((uint8_t*)&value, 1, 5, ldb_sector);
}

/* Write an unsigned long integer (32-bit) in the provided ldb_sector at the current location */
void ldb_uint32_write(FILE *ldb_sector, uint32_t value)
{
	fwrite((uint8_t*)&value, 1, 4, ldb_sector);
}

/* Read an unsigned long integer (32-bit) from the provided ldb_sector at the current location */
uint32_t ldb_uint32_read(FILE *ldb_sector)
{
	uint32_t out = 0;
	if (!fread((uint8_t*)&out, 1, 4, ldb_sector)) printf("Warning: cannot read LDB sector\n");
	return out;
}

/* Read an unsigned long integer (40-bit) from the provided ldb_sector at the current location */
uint64_t ldb_uint40_read(FILE *ldb_sector)
{
	uint64_t out = 0;
	if (!fread((uint8_t*)&out, 1, 5, ldb_sector)) printf("Warning: cannot read LDB sector\n");
	return out;
}

/* Read an unsigned integer (16-bit) from the provided ldb_sector at the current location */
uint16_t ldb_uint16_read(FILE *ldb_sector)
{
	uint16_t out = 0;
	if (!fread((uint8_t*)&out, 1, 2, ldb_sector)) printf("Warning: cannot read LDB sector\n");
	return out;
}

/* Read an unsigned integer (16-bit) from the provided pointer */
uint16_t uint16_read(uint8_t *pointer)
{
	uint16_t out;
	memcpy((uint8_t*)&out, pointer, 2);
	return out;
}

/* Write an unsigned integer (16-bit) in the provided location */
void uint16_write(uint8_t *pointer, uint16_t value)
{
	memcpy(pointer, (uint8_t*)&value, 2);
}

/* Read an unsigned integer (16-bit) from the provided pointer */
uint32_t uint32_read(uint8_t *pointer)
{
	uint32_t out;
	memcpy((uint8_t*)&out, pointer, 4);
	return out;
}

/* Write an unsigned integer (32-bit) in the provided location */
void uint32_write(uint8_t *pointer, uint32_t value)
{
	memcpy(pointer, (uint8_t*)&value, 4);
}

/* Read an unsigned integer (40-bit) from the provided pointer */
uint64_t uint40_read(uint8_t *pointer)
{
	uint64_t out = 0;
	memcpy((uint8_t*)&out, pointer, 5);
	return out;
}

/* Write an unsigned integer (40-bit) in the provided location */
void uint40_write(uint8_t *pointer, uint64_t value)
{
	memcpy(pointer, (uint8_t*)&value, 5);
}

bool uint32_is_zero(uint8_t *n)
{
	if (*n == 0)
		if (*(n+1) == 0)
			if (*(n+2) == 0)
				if (*(n+3) == 0) return true;
	return false;
}

