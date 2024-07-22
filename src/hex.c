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

 /**
  * @file hex.c
  * @date 12 Jul 2020 
  * @brief Contains HEX print and process utilities
  * 
  * //TODO Long description
  *
  * @see https://github.com/scanoss/ldb/blob/master/src/hex.c
  */
#include "ldb.h"
#include "logger.h"
/**
 * @brief Prints a hexdump of 'len' bytes from 'data' organized 'width' columns
 * 
 * 
 * Example:
 * 0000  54686973206973206120  This is a
 * 0010  737472696e6720746861  string tha
 * 0020  74207761732068657864  t was hexd
 * 0030  756d706564202e2e2e2e  umped ....
 * 0040  2e2e2e2e2e2e2e2e2e2e  ..........
 * 0050  2e2e2e2e2e2e2e2e2e2e  ..........
 * 
 * 
 * @param data Buffer to print
 * @param len Length of buffer
 * @param width width of columns
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

/**
 * @brief Fixed width recordset handler for hexdump
 * 
 * For Example: See in function ldb_hexprint();
 * 
 * @param key block key
 * @param subkey block subkey
 * @param subkey_ln block subkey lenght
 * @param data Buffer to print
 * @param len Length of buffer
 * @param iteration Not used
 * @param ptr Pointer to integer. Stores the number of columns to be printed.
 * @return false Always return false. It 
 */
bool ldb_hexprint16(uint8_t *key, uint8_t *subkey, int subkey_ln, uint8_t *data, uint32_t len, int iteration, void *ptr)
{
	int *width = ptr;
	for (int i = 0; i < LDB_KEY_LN; i++) printf("%02x", key[i]);
	for (int i = 0; i < subkey_ln; i++)  printf("%02x", subkey[i]);
	printf("\n");
	ldb_hexprint(data, len, *width);
	printf("\n");
	return false;
}

/**
 * @brief Converts binary to a string of hex digits
 * Does the opposite of ldb_hex_to_bin();
 * 
 * @param bin binary data to convert
 * @param len Length in bytes of the binary data
 * @param out Buffer to write the hex string
 */
void ldb_bin_to_hex(uint8_t *bin, uint32_t len, char *out)
{
	*out = 0;
	for (uint32_t i = 0; i < len; i++)
		sprintf(out + strlen(out), "%02x", bin[i]);
}


/**
 * @brief Converts a string of hex digits to binary.
 * Does the opposite of ldb_hexprint() and ldb_bin_to_hex();
 * 
 * Example: ldb_hex_to_bin("48656c6c6f20576f726c6421", 24, out);
 *          48656c6c6f20576f726c6421 -> Hello World!
 * 
 * @param hex String representing hex data
 * @param len Lenght of the string in bytes
 * @param out Buffer to write the binary
 */
void ldb_hex_to_bin(char *hex, int len, uint8_t *out)
{

    for (int i = 0; i < len; i += 2) {
        sscanf(&hex[i], "%2hhx", &out[i/2]);
    }
 }


/**
 * @brief Verify if a string only contains hexadecimal characters
 * 
 * @param str String to verify
 * @return true If the string only contains hexadecimal characters. False otherwise.
 */
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

/**
 * @brief Write an unsigned long integer (40-bit) in the provided ldb_sector at the current location
 * 
 * @param ldb_sector LDB sector to write to
 * @param value Value to write
 */
void ldb_uint40_write(FILE *ldb_sector, uint64_t value)
{
	fwrite((uint8_t*)&value, 1, 5, ldb_sector);
}

/**
 * @brief Write an unsigned long integer (32-bit) in the provided ldb_sector at the current location
 * 
 * @param ldb_sector LDB sector to write to
 * @param value Value to write
 */
void ldb_uint32_write(FILE *ldb_sector, uint32_t value)
{
	fwrite((uint8_t*)&value, 1, 4, ldb_sector);
}

/**
 * @brief Read an unsigned long integer (32-bit) from the provided ldb_sector at the current location
 * 
 * @param ldb_sector LDB sector to read from
 * @return uint32_t Value readed
 */
uint32_t ldb_uint32_read(FILE *ldb_sector)
{
	uint32_t out = 0;
	if (!fread((uint8_t*)&out, 1, 4, ldb_sector))
	{
		log_debug("Warning: cannot read LDB sector\n");
		ldb_read_failure = true;

	}
	return out;
}

/**
 * @brief Read an unsigned long integer (40-bit) from the provided ldb_sector at the current location
 * 
 * @param ldb_sector LDB sector to read from
 * @return uint64_t Value readed
 */
uint64_t ldb_uint40_read(FILE *ldb_sector)
{
	uint64_t out = 0;
	if (!fread((uint8_t*)&out, 1, 5, ldb_sector))
	{
		log_debug("Warning: cannot read LDB sector\n");
		ldb_read_failure = true;
	}
	return out;
}

/**
 * @brief Read an unsigned integer (16-bit) from the provided ldb_sector at the current location
 * 
 * @param ldb_sector LDB sector to read from
 * @return uint16_t Value readed
 */
uint16_t ldb_uint16_read(FILE *ldb_sector)
{
	uint16_t out = 0;
	if (!fread((uint8_t*)&out, 1, 2, ldb_sector))
	{ 
		log_debug("Warning: cannot read LDB sector\n");
		ldb_read_failure = true;
	}
	return out;
}

/**
 * @brief Read an unsigned integer (16-bit) from the provided pointer.
 * Copy two bytes starting from the provided pointer into an unsigned short.
 * 
 * @param pointer Pointer to read from
 * @return uint16_t Value readed
 */
uint16_t uint16_read(uint8_t *pointer)
{
	uint16_t out;
	memcpy((uint8_t*)&out, pointer, 2);
	return out;
}

/**
 * @brief Write an unsigned integer (16-bit) in the provided location
 * Copy two bytes from the provided value into the provided pointer.
 * pointer must point to a memory allocated of at least 2 bytes long.
 * 
 * @param pointer Pointer to write to
 * @param value Value to write
 */
void uint16_write(uint8_t *pointer, uint16_t value)
{
	memcpy(pointer, (uint8_t*)&value, 2);
}

/**
 * @brief Read an unsigned integer (32-bit) from the provided pointer
 * 
 * @param pointer Pointer to read from
 * @return uint32_t Value readed
 */
uint32_t uint32_read(uint8_t *pointer)
{
	uint32_t out;
	memcpy((uint8_t*)&out, pointer, 4);
	return out;
}

/**
 * @brief  Write an unsigned integer (32-bit) in the provided location
 * Copy four bytes from the provided value into the provided pointer.
 * pointer must point to a memory allocated of at least 4 bytes long.
 * 
 * 
 * @param pointer Pointer to write to
 * @param value Value to write
 */
void uint32_write(uint8_t *pointer, uint32_t value)
{
	memcpy(pointer, (uint8_t*)&value, 4);
}

/**
 * @brief Read an unsigned integer (40-bit) from the provided pointer
 * 
 * @param pointer Pointer to read from
 * @return uint64_t Value readed
 */
uint64_t uint40_read(uint8_t *pointer)
{
	uint64_t out = 0;
	memcpy((uint8_t*)&out, pointer, 5);
	return out;
}

/**
 * @brief Write an unsigned integer (40-bit) in the provided location
 * Copy five bytes from the provided value into the provided pointer.
 * pointer must point to a memory allocated of at least 5 bytes long.
 * 
 * @param pointer Pointer to write to
 * @param value Value to write
 */
void uint40_write(uint8_t *pointer, uint64_t value)
{
	memcpy(pointer, (uint8_t*)&value, 5);
}

/**
 * @brief Verify if a memory block of 4 bytes is all zeros
 * 
 * @param n Buffer to verify
 * @return true If all values are 0. False otherwise.
 */
bool uint32_is_zero(uint8_t *n)
{
	if (*n == 0)
		if (*(n+1) == 0)
			if (*(n+2) == 0)
				if (*(n+3) == 0) return true;
	return false;
}

