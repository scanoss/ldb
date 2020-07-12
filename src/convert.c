
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
	uint32_t out;
	fread((uint8_t*)&out, 1, 4, ldb_sector);
	return out;
}

/* Read an unsigned long integer (40-bit) from the provided ldb_sector at the current location */
uint64_t ldb_uint40_read(FILE *ldb_sector)
{
	uint64_t out = 0;
	fread((uint8_t*)&out, 1, 5, ldb_sector);
	return out;
}

/* Read an unsigned integer (16-bit) from the provided ldb_sector at the current location */
uint16_t ldb_uint16_read(FILE *ldb_sector)
{
	uint16_t out;
	fread((uint8_t*)&out, 1, 2, ldb_sector);
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

