// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * src/pointer.c
 *
 * Pointer handling functions
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
#include "ldb.h"
#include "logger.h"

/**
  * @file pointer.c
  * @date 12 Jul 2020
  * @brief List helpers function
 
  * //TODO Long description
  * @see https://github.com/scanoss/ldb/blob/master/src/pointer.c
  */

/**
 * @brief Returns the map position for the given record ID, using only the last 3 bytes
 * of the key, since the sector name contains the first byte 
 * 
 * @param key The key to get the map position for
 * @return uint64_t The map position
 */
uint64_t ldb_map_pointer_pos(uint8_t *key)
{

	uint64_t out = 0;

	/* Obtain a nuclear "char" pointer to the uint32 */
	uint8_t *k = (uint8_t *) &out;

	/* Assign less significant bytes (inverting for easy debugging, so that 00 00 01 is the second position in the map)*/
	k[0]=key[3];
	k[1]=key[2];
	k[2]=key[1];

	return out * LDB_PTR_LN;
}

/**
 * @brief Return pointer to the beginning of the given list (The last node)
 * 	
 * @param ldb_sector Sector of ldb
 * @param key Key of the ldb
 * @return uint64_t Obtain the pointer to the last node of the list
 */
uint64_t ldb_list_pointer(FILE *ldb_sector, uint8_t *key)
{
	fseeko64(ldb_sector, ldb_map_pointer_pos(key), SEEK_SET);
	return ldb_uint40_read(ldb_sector);
}

/**
 * @brief Return pointer to the last node of the list
 * 
 * @param ldb_sector Stream of the opened ldb
 * @param list_pointer Pointer to the list
 * @return uint64_t The 40 bits with the ldb sector
 */
uint64_t ldb_last_node_pointer(FILE *ldb_sector, uint64_t list_pointer)
{
	if (list_pointer == 0) return 0;
	fseeko64(ldb_sector, list_pointer, SEEK_SET);
	return ldb_uint40_read(ldb_sector);
}

uint64_t ldb_node_next(FILE *ldb_sector, uint64_t ptr)
{
	fseeko64(ldb_sector, ptr, SEEK_SET);
	uint64_t next_node;
	fread(&next_node, 1, LDB_PTR_LN, ldb_sector);
	return next_node;
}

uint64_t last_node_recovery(FILE *ldb_sector, uint64_t list)
{
	uint64_t ptr = list + LDB_PTR_LN;
	fseeko64(ldb_sector, ptr, SEEK_SET);
	uint64_t next_node = ldb_node_next(ldb_sector, ptr);
	ptr = 0;
	log_info("Last node recovery: %ld- ptr = %ld\n", list, ptr);
	while(next_node != 0)
	{
		ptr = next_node;
		log_info("Next node ptr = %ld\n", ptr);
		next_node = ldb_node_next(ldb_sector, ptr);
	}
	return ptr;
}

/**
 * @brief Update list pointers
 * 
 * @param ldb_sector Stream of the opened ldb
 * @param key Associated key
 * @param list List pointer where the new node will be added
 * @param new_node The new node to add
 */
void ldb_update_list_pointers(FILE *ldb_sector, uint8_t *key, uint64_t list, uint64_t new_node)
{
	/* If this is the first node of the list, we update the map and leave */
	if (list == 0)
	{
		fseeko64(ldb_sector, ldb_map_pointer_pos(key), SEEK_SET);
		ldb_uint40_write(ldb_sector, new_node);
		if (new_node < LDB_MAP_SIZE) ldb_error("E054 Data corruption");
	}

	/* Otherwise we update the list */
	else
	{

		/* Get the current last node pointer */
		fseeko64(ldb_sector, list, SEEK_SET);
		uint64_t last_node = ldb_uint40_read(ldb_sector);

		if (last_node < LDB_MAP_SIZE) {
			/*printf("\nMap size is %u\n", LDB_MAP_SIZE);
			printf ("\nData corruption on list %lu for key %02x%02x%02x%02x with last node %lu < %u\n", list, key[0], key[1], key[2], key[3], last_node, LDB_MAP_SIZE);
			ldb_error("E055 Data corruption");*/
			log_info("\nLast node is missing in list %lu for key %02x%02x%02x%02x\n", list, key[0], key[1], key[2], key[3]);
			last_node = last_node_recovery(ldb_sector, list);
			if (last_node == 0) //the list is empty, the first node must be re written
			{
				fseeko64(ldb_sector, list, SEEK_SET);
				ldb_uint40_write(ldb_sector, new_node); //set first node 
				fseeko64(ldb_sector, list + LDB_PTR_LN, SEEK_SET); //set last node
				ldb_uint40_write(ldb_sector, new_node);
				return;
			}
			else if (last_node > 0 && last_node < LDB_MAP_SIZE)
				ldb_error("E055 Data corruption");
			
		}

		/* Update the list pointer to the new last node */
		fseeko64(ldb_sector, list, SEEK_SET);
		ldb_uint40_write(ldb_sector, new_node);


		/* Update the last node pointer to next (new) node */
		fseeko64(ldb_sector, last_node, SEEK_SET);
		ldb_uint40_write(ldb_sector, new_node);
	

	}
}

/**
 * @brief // TODO
 * 
 * @param ldb_sector // TODO
 * @param key // TODO
 */
void ldb_list_unlink(FILE *ldb_sector, uint8_t *key)
{
	fseeko64(ldb_sector, ldb_map_pointer_pos(key), SEEK_SET);
	ldb_uint40_write(ldb_sector, 0);
}

