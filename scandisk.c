#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include "bootsect.h"
#include "bpb.h"
#include "direntry.h"
#include "fat.h"
#include "dos.h"


void usage(char *progname) {
    fprintf(stderr, "usage: %s <imagename>\n", progname);
    exit(1);
}//end usage()

uint16_t read_dirent(struct direntry *dirent, int *type, char *filename){
	//reads a directory entry, modifies type to indicate whether this entry refers to a directory (1), a regular file (0), or neither (-1), modifies filename to reflect the name of the directory or the file. Returns the start cluster if the entry refers to a directory, returns 0 if the entry refers to a regular file or refers to neither directory nor file. 

    uint16_t followclust = 0;

    int i;
    char name[9];
    char extension[4];
    uint16_t file_cluster;
    name[8] = ' ';
    extension[3] = ' ';
    memcpy(name, &(dirent->deName[0]), 8);
    memcpy(extension, dirent->deExtension, 3);

	*type = -1; //type is initialized to indicate that the entry refers to neither a directory nor a file


	if (name[0] == SLOT_EMPTY){
		return followclust;
	}

    /* skip over deleted entries */
    if (((uint8_t)name[0]) == SLOT_DELETED)
    {
		return followclust;
    }

    if (((uint8_t)name[0]) == 0x2E)
    {
		// dot entry ("." or "..")
		// skip it
        return followclust;
    }

    /* names are space padded - remove the spaces */
    for (i = 8; i > 0; i--) 
    {
		if (name[i] == ' ') 
			name[i] = '\0';
		else 
			break;
    }

    /* remove the spaces from extensions */
    for (i = 3; i > 0; i--){
		if (extension[i] == ' ') 
			extension[i] = '\0';
		else 
			break;
    }


	//fill out filename
	int j = 0;
	while(name[j] != '\0' && j < 9){
		filename[j] = name[j];
		j++;
	}//end while

	if(strlen(extension)){
		filename[j] = '.';
		j++;
		int k = 0;
		while(extension[k] != '\0' && k < 4){
			filename[j] = extension[k];
			j++;
			k++;
		}//end while
		filename[j] = '\0';
	}//end if
	else{
		filename[j] = '\0';
	}//end else


    if ((dirent->deAttributes & ATTR_WIN95LFN) == ATTR_WIN95LFN){
		// ignore any long file name extension entries
		//
		// printf("Win95 long-filename entry seq 0x%0x\n", dirent->deName[0]);
    }
    else if ((dirent->deAttributes & ATTR_VOLUME) != 0){
		
    } 
    else if ((dirent->deAttributes & ATTR_DIRECTORY) != 0){
        // don't deal with hidden directories; MacOS makes these
        // for trash directories and such; just ignore them.
		if ((dirent->deAttributes & ATTR_HIDDEN) != ATTR_HIDDEN){
			*type = 1;			
			file_cluster = getushort(dirent->deStartCluster);
		    followclust = file_cluster;
		}
    }
    else{
        /*
         * a "regular" file entry
         */
		*type = 0;
    }

    return followclust;
}//end read_dirent

void trim_size_dirent(struct direntry *dirent, int size_FAT){//trim down the size indicated by the directory entry so that it agrees with the size as indicated by the FAT table
	putulong(dirent->deFileSize, size_FAT);

}//end trim_dirent_size

void trim_size_FAT(uint16_t startclust, uint8_t *image_buf, struct bpb33 *bpb, int size_dirent, int cluster_size){
	//trim down the FAT chain that starts with startclust to the size indicated by size_dirent
	int covered_size = 0;
	uint16_t currentclust = startclust;
	
	while(covered_size < size_dirent){
		covered_size += cluster_size;
		currentclust = get_fat_entry(currentclust, image_buf, bpb);
	}//end while

	//mark currentclust as EOF, and free all clusters that are after the currentclust and up to the original EOF (inclusive).
	uint16_t tmp = currentclust;
	currentclust = get_fat_entry(currentclust, image_buf, bpb);	
	set_fat_entry(tmp, FAT12_MASK & CLUST_EOFS, image_buf, bpb);
	while(is_valid_cluster(currentclust, bpb)){
		printf("Freeing clusters!\n"); //TEST
		tmp = currentclust;
		currentclust = get_fat_entry(currentclust, image_buf, bpb);	
		set_fat_entry(tmp, CLUST_FREE, image_buf, bpb);		
	}//end while
	set_fat_entry(currentclust, CLUST_FREE, image_buf, bpb);//Free the original EOF

}//end trim_FAT_size

int calculateFATsize(uint16_t data_cluster, int cluster_size, uint8_t *image_buf, struct bpb33 *bpb){

	int size_FAT = 0;

	while(is_valid_cluster(data_cluster, bpb)){
		size_FAT += cluster_size;
		data_cluster = get_fat_entry(data_cluster, image_buf, bpb);
	}//end while	

	return size_FAT;

}//end calculateFATsize

void resolve_inconsistencies(uint16_t clust, uint8_t *image_buf, struct bpb33 *bpb){
		
	//look at the files in the directory entries in start clust, and get their sizes (in bytes) as indicated by the metadata (from the directory entry) and the FAT table, respectively.

	if(clust != 0 && !is_valid_cluster(clust, bpb)){
		printf("This is an invalid cluster.\n");//TEST
		return;
	}//end if 

	//loop through the directory entries of the start clust
	struct direntry *dirent = (struct direntry*)cluster_to_addr(clust, image_buf, bpb);
	int cluster_size = bpb->bpbBytesPerSec * bpb->bpbSecPerClust;
	int direntry_per_cluster = cluster_size / sizeof(struct direntry);

    for (int i = 0; i < direntry_per_cluster; i++){

		char entry_name[14];
		memset(entry_name, '\0', 14);
		int type = -1;

		uint16_t startclust = read_dirent(dirent, &type, entry_name);
		printf("The type is: %d\n", type); //TEST
		if(type == 1){//if this entry contains information about a directory
			//follow_dir(startclust, image_buf, bpb);

			printf("This is a directory. The name is: %s. Going into directories.\n     ", entry_name);//TEST
			resolve_inconsistencies(startclust, image_buf, bpb);

		}//end if
		else if(type == 0){//if this entry contains information about a regular file, get its sizes as indicated by the FAT table and the directory entry, respectively

			int size_dirent;
			int size_FAT;

			size_dirent = getulong(dirent->deFileSize);

			uint16_t data_cluster = getushort(dirent->deStartCluster);			
			size_FAT = calculateFATsize(data_cluster, cluster_size, image_buf, bpb);	

			printf("This is a regular file. The name is: %s. The metadata size is: %d. The FAT size is: %d. ", entry_name, size_dirent, size_FAT);//TEST
			if(size_FAT - size_dirent > cluster_size){
				printf("FAT size too large!");
				trim_size_FAT(startclust, image_buf, bpb, size_dirent, cluster_size);
				printf("After truncation: The metadata size is: %d. The FAT size is: %d. \n", size_dirent, calculateFATsize(data_cluster, cluster_size, image_buf, bpb));
			}//end if
			else if(size_dirent > size_FAT){
				printf("Metadata size too large!\n");
				trim_size_dirent(dirent, size_FAT);
				//printf("After truncation: The metadata size is: %d. The FAT size is: %d. \n", getulong(dirent->deFileSize), size_FAT);//TEST
			}//end if
			else{
				printf("\n");
			}//TEST
			
		}//end else if

		dirent++;

	}//end for

}//end resolve_inconsistencies

int main(int argc, char** argv) {
    uint8_t *image_buf;
    int fd;
    struct bpb33* bpb;
    if (argc < 2) {
	usage(argv[0]);
    }

    image_buf = mmap_file(argv[1], &fd);
    bpb = check_bootsector(image_buf);
	printf("\n");

    // your code should start here...
	uint16_t root_dir_start_clust = 0;
	resolve_inconsistencies(root_dir_start_clust, image_buf, bpb);



    unmmap_file(image_buf, &fd);
    return 0;
}
