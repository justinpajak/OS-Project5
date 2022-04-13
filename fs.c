#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define FS_MAGIC           0xf0f03410
#define INODES_PER_BLOCK   128
#define POINTERS_PER_INODE 5
#define POINTERS_PER_BLOCK 1024

struct fs_superblock {
	int magic;
	int nblocks;
	int ninodeblocks;
	int ninodes;
};

struct fs_inode {
	int isvalid;
	int size;
	int direct[POINTERS_PER_INODE];
	int indirect;
};

union fs_block {
	struct fs_superblock super;
	struct fs_inode inode[INODES_PER_BLOCK];
	int pointers[POINTERS_PER_BLOCK];
	char data[DISK_BLOCK_SIZE];
};

/* Free block bitmap */
/* Index by block number, 1 - used, 0 - free */
int *bitmap = NULL;

int fs_format() {
	/* Return failure if attempting to format an already mounted disk */
	if (bitmap != NULL) {
		return 0;
	}
	
	/* Destroy any data already present */
	int nblocks = disk_size();
	char *buffer = (char*)calloc(4096, sizeof(char));
	for (size_t i = 0; i < nblocks; i++) {
		disk_write(i, buffer);
	}
	free(buffer);
	
	/* Calculate # of blocks to be allocated for inodes and total # of inodes */
	int ninodeblocks = (nblocks + (10 - 1)) / 10;
	int ninodes = ninodeblocks * INODES_PER_BLOCK;
	
	/* Write the superblock */
	union fs_block block;
	block.super.magic = FS_MAGIC;
	block.super.nblocks = nblocks;
	block.super.ninodeblocks = ninodeblocks;
	block.super.ninodes = ninodes;
	disk_write(0, block.data);

	return 1;
}

void fs_debug() {
	/* Read superblock data from disk */
	union fs_block block;
	disk_read(0, block.data);

	/* Print superblock info */
	printf("superblock:\n");
	if (block.super.magic == FS_MAGIC) {
		printf("    magic number is valid\n");
	} else {
		printf("    magic number is invalid\n");
	}
	printf("    %d blocks on disk\n",block.super.nblocks);
	printf("    %d blocks for inodes\n",block.super.ninodeblocks);
	printf("    %d inodes total\n",block.super.ninodes);

	/* Iterate through each block containing inodes */
	for (int i = 0; i < block.super.ninodeblocks; i++) {
		disk_read(i + 1, block.data);

		/* Iterate through each inode in block */
		for (int j = 0; j < INODES_PER_BLOCK; j++) {
			int inode_no = 128 * i + j;
			struct fs_inode inode = block.inode[j];
			if (inode.isvalid) {
				printf("inode %d:\n", inode_no);
				printf("    size %d bytes\n", inode.size);

				/* Iterate over direct blocks for inode */
				int any_dblocks = 0;
				for (int k = 0; k < POINTERS_PER_INODE; k++) {
					if (inode.direct[k]) {
						any_dblocks = 1;
						break;
					}
				}
				if (any_dblocks) {
					printf("    direct blocks: ");
					for (int k = 0; k < POINTERS_PER_INODE; k++) {
						if (inode.direct[k]) {
							printf("%d ", inode.direct[k]);
						}
					}
					printf("\n");
				}

				/* Iterate over indirect data blocks for inode */
				if (inode.indirect) {
					printf("    indirect block: %d\n", inode.indirect);
					disk_read(inode.indirect, block.data);
					printf("    indirect data blocks: ");
					for (int k = 0; k < POINTERS_PER_BLOCK; k++) {
						if (block.pointers[k]) {
							printf("%d ", block.pointers[k]);
						}
					}
					printf("\n");
				}
			}
		}
	}
}

int fs_mount() {
	/* Examine the disk for a filesystem */
	union fs_block block;
	disk_read(0, block.data);

	/* If there is a filesystem on disk, create the bitmap */
	if (block.super.magic == FS_MAGIC) {
		bitmap = (int*)calloc(block.super.nblocks, sizeof(int));

		/* Mark the superblock and inode blocks as used */
		for (int i = 0; i < block.super.ninodeblocks + 1; i++) {
			bitmap[i] = 1;
		}

		/* Scan through all of the inodes to populate the bitmap */
		for (int i = 0; i < block.super.ninodeblocks; i++) {
			disk_read(i + 1, block.data);
			for (int j = 0; j < INODES_PER_BLOCK; j++) {
				struct fs_inode inode = block.inode[j];
				if (inode.isvalid) {
					/* Scan through direct blocks */
					for (int k = 0; k < POINTERS_PER_INODE; k++) {
						if (inode.direct[k]) {
							bitmap[inode.direct[k]] = 1;
						}
					}

					/* Scan through indirect blocks */
					if (inode.indirect) {
						disk_read(inode.indirect, block.data);
						for (int k = 0; k < POINTERS_PER_BLOCK; k++) {
							if (block.pointers[k]) {
								bitmap[block.pointers[k]] = 1;
							}
						}
					}
				}
			}
		}

		/* USE TO DEBUG */
		/* 
		printf("USED BLOCKS\n");
		for (int i = 0; i < disk_size(); i++) {
			if (bitmap[i]) {
				printf("%d ", i);
			}
		}
		printf("\n");
		*/

		return 1;
	}

	/* No file system is present on disk */
	return 0;
}

int fs_create() {
	//create an inode of zero length
	union fs_block block;
	
	//iterate over the inodes in the disk
	disk_read(0, block.data);
	for (int i = 0; i < block.super.ninodeblocks; i++){
		//get the inode block
		disk_read(i + 1, block.data);
		//iterate over the inodes in the inode block and find first invalid inode
		for (int j = 1; j < INODES_PER_BLOCK+1; j++){
			struct fs_inode inode = block.inode[j];
			if(!inode.isvalid){
				//create the new inode struct
				struct fs_inode new_inode;
				//set all values to 0
				new_inode.isvalid = 1;
				new_inode.size = 0;
				new_inode.indirect = 0;
				//calculate the inode number
				int inode_no = 128 * i + j;
				//set the new inode in the blocks inodes
				block.inode[j] = new_inode;
				//write back to disk
				disk_write(i+1, block.data);
				return inode_no;
			}
		}
	}
	//return positive inode number on success and 0 on failure
	return 0;
}

int fs_delete( int inumber ) {
	//create an inode of zero length
	union fs_block block;
	//read in the superblock
	disk_read(0, block.data);
	int block_no = ((inumber/128) * 128)+1;
	// check to see if the inode number is in range
	if(block_no > block.super.ninodeblocks || block_no < 1){
		printf("BAD BLOCK #\n");
		return 0;
	}
	// read in the data block
	disk_read(block_no, block.data);

	//get the relative inumber in the block
	int rel_inum = inumber % 128;
	struct fs_inode del_inode = block.inode[rel_inum];
	//check if it is a valid inode
	if(!del_inode.isvalid){
		printf("INODE NOT VALID\n");
		return 0;
	}
	for (int k = 0; k < POINTERS_PER_INODE; k++){
		if (del_inode.direct[k]) {
			// get the data block
			disk_read(del_inode.direct[k], block.data);
			// create blank data for the block
			char blank_data[DISK_BLOCK_SIZE];
			// have the data block point to the blank data
			*block.data = *blank_data;
			// set the bitmap at that location to 0
			bitmap[del_inode.direct[k]] = 0;
		} else if (del_inode.indirect) {
			// get the indirect block
			disk_read(del_inode.indirect, block.data);
			for (int j = 0; j < POINTERS_PER_BLOCK; j++) {
				if (block.pointers[j]) {
					// get the block from the pointer
					union fs_block data_block;
					disk_read(block.pointers[j], data_block.data);
					// create blank data for it to point to
					char blank_data[DISK_BLOCK_SIZE];
					*data_block.data = *blank_data;
					// set the bitmap at this location to 0
					bitmap[block.pointers[j]] = 0;
				}
			}
		}
	}
	// set the inode to invalid
	disk_read(block_no, block.data);
	// set the valid bit to 0
	del_inode.isvalid = 0;
	block.inode[rel_inum] = del_inode;
	disk_write(block_no, block.data);
	

	return 1;
}

int fs_getsize( int inumber ) {
	//create an inode of zero length
	union fs_block block;
	//read in the superblock
	disk_read(0, block.data);
	int block_no = ((inumber/128) * 128)+1;	
	// check if the block number is valid
	if(block_no > block.super.ninodeblocks || block_no < 1){
		return -1;
	}
	// get the offset relative to the block
	disk_read(block_no, block.data);
	int rel_inum = inumber % 128;
	struct fs_inode my_inode = block.inode[rel_inum];
	// check if it is a valid inode
	if(!my_inode.isvalid){
		return -1;
	}
	return my_inode.size;
}

int fs_read( int inumber, char *data, int length, int offset ) {
	return 0;
}

int fs_write( int inumber, const char *data, int length, int offset ) {
	return 0;
}
