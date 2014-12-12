#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdbool.h>

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

void trim_size_FAT(uint16_t currentclust, uint8_t *image_buf, struct bpb33 *bpb, int size_dirent, int cluster_size){
	//trim down the FAT chain that starts with currentclust to the size indicated by size_dirent

	int num_of_clusters = size_dirent / cluster_size;
	if(size_dirent % cluster_size != 0){
		num_of_clusters++;
	}//end if

	for(int count = 1; count < num_of_clusters; count++){
		currentclust = get_fat_entry(currentclust, image_buf, bpb);
	}//end for

	uint16_t tmp = currentclust;
	currentclust = get_fat_entry(currentclust, image_buf, bpb);
	set_fat_entry(tmp, FAT12_MASK & CLUST_EOFS, image_buf, bpb);
	while(!is_end_of_file(currentclust)){ //mark everything that is after the new EOF and before the original EOF as free
		tmp = currentclust;
		currentclust = get_fat_entry(currentclust, image_buf, bpb);
		set_fat_entry(tmp, FAT12_MASK & CLUST_FREE, image_buf, bpb);
	}//end while 
	set_fat_entry(tmp, FAT12_MASK & CLUST_FREE, image_buf, bpb);//mark the original EOF as free

}//end trim_FAT_size

bool is_bad_clust(uint16_t clust){
	return (clust == (FAT12_MASK & CLUST_BAD) );
}//end is_bad_cluster

bool is_free_clust(uint16_t clust){
	return (clust == (FAT12_MASK & CLUST_FREE) );
}//end is_free_cluster

bool is_last_clust(uint16_t clust){
	return (clust == (FAT12_MASK & CLUST_LAST) );
}//end is_last_cluster

int calculateFATsize(uint16_t data_cluster, int cluster_size, uint8_t *image_buf, struct bpb33 *bpb){

	int size_FAT = 0;

	while(!is_end_of_file(data_cluster)){
		size_FAT += cluster_size;
		data_cluster = get_fat_entry(data_cluster, image_buf, bpb);
	}//end while
	
	return size_FAT;

}//end calculateFATsize

void resolve_inconsistencies(uint16_t clust, uint8_t *image_buf, struct bpb33 *bpb){
		
	//look at the files in the directory entries in start clust, and get their sizes (in bytes) as indicated by the metadata (from the directory entry) and the FAT table, respectively.

	if(is_bad_clust(clust) || is_end_of_file(clust) || is_last_clust(clust) ){
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
		if(type == 1){//if this entry contains information about a directory
			//printf("This is a directory. The name is: %s. Going into directories.\n     ", entry_name);//TEST
			resolve_inconsistencies(startclust, image_buf, bpb);

		}//end if
		else if(type == 0){//if this entry contains information about a regular file, get its sizes as indicated by the FAT table and the directory entry, respectively

			int size_dirent = -1;
			int size_FAT = -1;

			size_dirent = getulong(dirent->deFileSize);

			uint16_t data_cluster = getushort(dirent->deStartCluster);			
			size_FAT = calculateFATsize(data_cluster, cluster_size, image_buf, bpb);	

			if(size_FAT - size_dirent > cluster_size){
				
				printf("FAT size too large! File name: %s. Metadata size: %d. FAT size: %d\n", entry_name, size_dirent, size_FAT);//TEST
				trim_size_FAT(data_cluster, image_buf, bpb, size_dirent, cluster_size);
				printf("After truncation: the metadata size is: %d. The FAT size is: %d. \n", size_dirent, calculateFATsize(data_cluster, cluster_size, image_buf, bpb));
			}//end if
			else if(size_dirent > size_FAT){
				printf("Metadata size too large! File name: %s. Metadata size: %d. FAT size: %d\n", entry_name, size_dirent, size_FAT);//TEST
				trim_size_dirent(dirent, size_FAT);
				printf("After truncation: The metadata size is: %d. The FAT size is: %d. \n", getulong(dirent->deFileSize), size_FAT);//TEST
			}//end if
			
		}//end else if

		dirent++;

	}//end for

}//end resolve_inconsistencies

void mark_cluster_map(uint16_t clust, int cluster_map[]){
	cluster_map[clust - (FAT12_MASK & CLUST_FIRST) ] = 1;
}//end mark_cluster_map

void populate_cluster_map(uint16_t clust, uint8_t *image_buf, struct bpb33 *bpb, int cluster_map[]){

	mark_cluster_map(clust, cluster_map);
	if(is_bad_clust(clust) || is_end_of_file(clust) || is_last_clust(clust) ){
		return;
	}//end if 

	struct direntry *dirent = (struct direntry*)cluster_to_addr(clust, image_buf, bpb);
	int cluster_size = bpb->bpbBytesPerSec * bpb->bpbSecPerClust;
	int direntry_per_cluster = cluster_size / sizeof(struct direntry);

    for (int i = 0; i < direntry_per_cluster; i++){

		char entry_name[14];
		memset(entry_name, '\0', 14);
		int type = -1;

		uint16_t startclust = read_dirent(dirent, &type, entry_name);

		if(type == 1){//if this direntry is a directory
			populate_cluster_map(startclust, image_buf, bpb, cluster_map);
		}//end if
		else if(type == 0){//if this direntry is a regular file
			uint16_t data_cluster = getushort(dirent->deStartCluster);
			while(!is_free_clust(data_cluster) && !is_bad_clust(data_cluster) ){
				mark_cluster_map(data_cluster, cluster_map);
				data_cluster = get_fat_entry(data_cluster, image_buf, bpb);
				if(is_end_of_file(data_cluster)){
					mark_cluster_map(data_cluster, cluster_map);
					break;
				}//end if
			}//end while
		}//end else if

		dirent++;

	}//end for

}//end populate_cluster_map

void build_orphanage(uint16_t root_dir_start_clust, uint8_t *image_buf, struct bpb33 *bpb){

	//test
	printf("Total number of clusters: %d\n", ( FAT12_MASK & CLUST_LAST) - (FAT12_MASK & CLUST_FIRST) + 1);

	//find orphan
	int cluster_map[( FAT12_MASK & CLUST_LAST) - (FAT12_MASK & CLUST_FIRST) + 1]; //1 means referenced, 0 means not referenced
	for(int i = 0; i < strlen(cluster_map); i++){//initialize the cluster_map
		cluster_map[i] = 0;
	}//end for
	
	printf("strlen(cluster_map): %d\n", sizeof(cluster_map) / sizeof(int) );

	//TEST, print cluster_map
	for(int i = 0; i < strlen(cluster_map); i++){
		printf("%d  ", cluster_map[i]);		
	}//end for

	populate_cluster_map(root_dir_start_clust, image_buf, bpb, cluster_map);
	



}//end build_orphanage


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
	resolve_inconsistencies(root_dir_start_clust, image_buf, bpb); //resolve size differences
	build_orphanage(root_dir_start_clust, image_buf, bpb); //clean up orphaned clusters
	
	/*
	uint16_t experiment_clust = 3;
	set_fat_entry(experiment_clust, FAT12_MASK & CLUST_BAD, image_buf, bpb);
 	if(get_fat_entry(experiment_clust, image_buf, bpb) == (FAT12_MASK & CLUST_BAD)){
		printf("Bad cluster detected.\n");
	}
	else{
		printf("Bad cluster not detected.\n");
	}
	*/
	

    unmmap_file(image_buf, &fd);
    return 0;
}
