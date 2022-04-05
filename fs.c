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

int fs_format() {
	return 0;
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
				printf("    direct blocks: ");
				for (int i = 0; i < POINTERS_PER_INODE; i++) {
					if (inode.direct[i]) {
						printf("%d ", inode.direct[i]);
					}
				}
				printf("\n");

				/* Iterate over indirect data blocks for inode */
				if (inode.indirect) {
					printf("    indirect block: %d\n", inode.indirect);
					disk_read(inode.indirect, block.data);
					printf("    indirect data blocks: ");
					for (int i = 0; i < POINTERS_PER_BLOCK; i++) {
						if (block.pointers[i]) {
							printf("%d ", block.pointers[i]);
						}
					}
					printf("\n");
				}
			}
		}
	}
}

int fs_mount() {
	return 0;
}

int fs_create() {
	/* Create new inode of zero length */
	union fs_block block;
	disk_read(0, block.data);

	for (int i = 0; i < block.super.ninodeblocks; i++) {
		disk_read(i + 1, block.data);
		for (int j = 0; i < INODES_PER_BLOCK; j++) {
			int inode_no = 128 * i + j;
			if (inode_no == 0) {
				continue;
			}
			struct fs_inode inode = block.inode[j];
			
			/* Found free inode */
			if (!inode.isvalid) {
				/* Mark inode as valid and write changes to disk */
				inode.isvalid = 1;
				block.inode[j] = inode;
				disk_write(i + 1, block.data);
				return inode_no;
			}
		}
	}
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
