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
	return 0;
}

int fs_delete( int inumber ) {
	return 0;
}

int fs_getsize( int inumber ) {
	return -1;
}

int fs_read( int inumber, char *data, int length, int offset ) {
	return 0;
}

int fs_write( int inumber, const char *data, int length, int offset ) {
	return 0;
}
