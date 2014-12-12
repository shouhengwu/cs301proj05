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

//-------------------------------------------------------------- functions used to fix images 1 and 2.

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

void mark_reference_map(uint16_t clust, int value, int reference_map[], int map_size){
	if(clust < 0 || clust >= (uint16_t)map_size){
		return;
	}//end if 

	if(value != 0 && value != 1){
		return;
	}//end if

	reference_map[clust] = value;

}

void trim_size_dirent(struct direntry *dirent, int size_FAT){//trim down the size indicated by the directory entry so that it agrees with the size as indicated by the FAT table
	putulong(dirent->deFileSize, size_FAT);

}//end trim_dirent_size

void trim_size_FAT(uint16_t currentclust, uint8_t *image_buf, struct bpb33 *bpb, int size_dirent, int cluster_size, int reference_map[]){
	//trim down the FAT chain that starts with currentclust to the size indicated by size_dirent

	reference_map[currentclust] = 1;

	int num_of_clusters = size_dirent / cluster_size;
	if(size_dirent % cluster_size != 0){
		num_of_clusters++;
	}//end if

	for(int count = 1; count < num_of_clusters; count++){
		currentclust = get_fat_entry(currentclust, image_buf, bpb);
		reference_map[currentclust] = 1;
	}//end for


	uint16_t tmp = currentclust;
	currentclust = get_fat_entry(currentclust, image_buf, bpb);
	reference_map[currentclust] = 1;

	set_fat_entry(tmp, FAT12_MASK & CLUST_EOFS, image_buf, bpb);
	reference_map[tmp] = 1;

	while(!is_end_of_file(currentclust)){ //mark everything that is after the new EOF and before the original EOF as free		
		tmp = currentclust;
		currentclust = get_fat_entry(currentclust, image_buf, bpb);
		reference_map[currentclust] = 1;

		set_fat_entry(tmp, FAT12_MASK & CLUST_FREE, image_buf, bpb);
		reference_map[tmp] = 0;
	}//end while 
	set_fat_entry(currentclust, FAT12_MASK & CLUST_FREE, image_buf, bpb);//mark the original EOF as free
	reference_map[currentclust] = 0;

}//end trim_FAT_size

int calculateFATsize(uint16_t data_cluster, int cluster_size, uint8_t *image_buf, struct bpb33 *bpb, int reference_map[]){

	int size_FAT = 0;

	reference_map[data_cluster] = 1;

	while(!is_end_of_file(data_cluster)){
		size_FAT += cluster_size;
		data_cluster = get_fat_entry(data_cluster, image_buf, bpb);
		reference_map[data_cluster] = 1;
	}//end while
	
	return size_FAT;

}//end calculateFATsize

bool is_bad_clust(uint16_t clust, uint8_t *image_buf, struct bpb33* bpb){
	return (get_fat_entry(clust, image_buf, bpb) == (FAT12_MASK & CLUST_BAD) );
}//end is_bad_cluster

bool is_free_clust(uint16_t clust, uint8_t *image_buf, struct bpb33* bpb){
	return (get_fat_entry(clust, image_buf, bpb) == (FAT12_MASK & CLUST_FREE) );
}//end is_free_cluster


void resolve_inconsistencies_and_populate_map(uint16_t clust, uint8_t *image_buf, struct bpb33 *bpb, int reference_map[]){
		
	//This function resolves the size differences between what the metadata indicates and what the FAT clusters indicate.
	//It also populates reference_map which shows which clusters are referenced and which are not. 

	reference_map[clust] = 1;

	if(is_end_of_file(clust) ){
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
			resolve_inconsistencies_and_populate_map(startclust, image_buf, bpb, reference_map);

		}//end if
		else if(type == 0){//if this entry contains information about a regular file, get its sizes as indicated by the FAT table and the directory entry, respectively

			int size_dirent = -1;
			int size_FAT = -1;

			size_dirent = getulong(dirent->deFileSize);

			uint16_t data_cluster = getushort(dirent->deStartCluster);	
			reference_map[data_cluster] = 1;
			size_FAT = calculateFATsize(data_cluster, cluster_size, image_buf, bpb, reference_map);	

			if(size_FAT - size_dirent > cluster_size){
				
				printf("FAT size is too large for: %s. Metadata size: %d. FAT size: %d. ", entry_name, size_dirent, size_FAT);//TEST
				trim_size_FAT(data_cluster, image_buf, bpb, size_dirent, cluster_size, reference_map);
				printf("After reconciling sizes: the metadata size is: %d. The FAT size is: %d. \n\n", size_dirent, calculateFATsize(data_cluster, cluster_size, image_buf, bpb, reference_map));
			}//end if
			else if(size_dirent > size_FAT){
				printf("Metadata size is too large: %s. Metadata size: %d. FAT size: %d. ", entry_name, size_dirent, size_FAT);//TEST
				trim_size_dirent(dirent, size_FAT);
				printf("After reconciling sizes: The metadata size is: %d. The FAT size is: %d. \n\n", getulong(dirent->deFileSize), size_FAT);//TEST
			}//end if
			
		}//end else if

		dirent++;

	}//end for

}//end resolve_inconsistencies

//------------------------------------------------------------------------------- functions used to fix image 3.

/* write the values into a directory entry */
void write_dirent(struct direntry *dirent, char *filename, uint16_t start_cluster, uint32_t size){
    char *p, *p2;
    char *uppername;
    int len, i;

    /* clean out anything old that used to be here */
    memset(dirent, 0, sizeof(struct direntry));

    /* extract just the filename part */
    uppername = strdup(filename);
    p2 = uppername;
    for (i = 0; i < strlen(filename); i++) {
		if (p2[i] == '/' || p2[i] == '\\'){
			uppername = p2+i+1;
		}
    }

    /* convert filename to upper case */
    for (i = 0; i < strlen(uppername); i++) {
		uppername[i] = toupper(uppername[i]);
	}

    /* set the file name and extension */
    memset(dirent->deName, ' ', 8);
    p = strchr(uppername, '.');
    memcpy(dirent->deExtension, "___", 3);
    if (p == NULL){
		fprintf(stderr, "No filename extension given - defaulting to .___\n");
    }
    else{
		*p = '\0';
		p++;
		len = strlen(p);
		if (len > 3) len = 3;
		memcpy(dirent->deExtension, p, len);
    }

    if (strlen(uppername)>8){
		uppername[8]='\0';
    }
    
	memcpy(dirent->deName, uppername, strlen(uppername));
    free(p2);

    /* set the attributes and file size */
    dirent->deAttributes = ATTR_NORMAL;
    putushort(dirent->deStartCluster, start_cluster);
    putulong(dirent->deFileSize, size);

    /* could also set time and date here if we really
       cared... */
}//end write_dirent

void find_orphans(int reference_map[], uint16_t orphan_list[], int map_size, uint8_t *image_buf, struct bpb33* bpb){//create and return a list of cluster numbers of the orphans

	int counter = 0;

	for(uint16_t i = 2; i < map_size; i++){
		if(reference_map[i] == 0 && !is_free_clust(i, image_buf, bpb) ){
			orphan_list[counter] = i;
			counter++;
		}//end if
	}//end for

}//end find_orphans

struct direntry *find_available_direntry(uint8_t *image_buf, struct bpb33* bpb){

	struct direntry *dirent = (struct direntry *)root_dir_addr(image_buf, bpb);
	int i = 0;
	while(i < bpb->bpbRootDirEnts){
		if((uint8_t)dirent->deName[0] == SLOT_EMPTY || (uint8_t)dirent->deName[0] == SLOT_DELETED){
			return dirent;
		}//end if

		i++;
		dirent++;
	}//end while
	
	return NULL;
}//end find_available_direntry

void house_an_orphan(uint16_t orphan, uint8_t *image_buf, struct bpb33* bpb, int count){
	struct direntry *dirent = find_available_direntry(image_buf, bpb); // return a pointer to a directory entry that is either empty or deleted
	if(dirent == NULL){
		printf("There is no available directory entry left. Cannot house this orphan cluster.\n");
		return;
	}//end if
	
	char name[64] = {'f', 'o', 'u', 'n', 'd', '\0'};
	char number[] = {'\0', '\0', '\0', '\0', '\0'};
	sprintf(number, "%d", count+1);
	strcat(name, number);
	strcat(name, ".dat");

	int cluster_size = bpb->bpbBytesPerSec * bpb->bpbSecPerClust;

	write_dirent(dirent, name, orphan, cluster_size);

}//end  house_orphans

void house_orphans(uint16_t orphan_list[], uint8_t *image_buf, struct bpb33* bpb){
	int count = 0;
	while(orphan_list[count] != (uint16_t) 0){
		house_an_orphan(orphan_list[count], image_buf, bpb, count);
		orphan_list++;
	}//end while

}//end house_orphans

void print_orphans(uint16_t orphan_list[]){

	if(orphan_list[0] != (uint16_t) 0) {
		printf("Orphans found. They are cluster(s): ");

		int count_orphans = 0;
		while(orphan_list[count_orphans] != (uint16_t) 0){
			printf("%d ", orphan_list[count_orphans]); 
			count_orphans++;
		}//end while
		printf("\n");
	}//end if

}//end print_orphans

void initialize_reference_map(int reference_map[], int map_size){
	for(int i = 0; i < map_size; i++){//initialize reference_map
		reference_map[i] = 0;
	}//end for
}

void initialize_orphan_list(uint16_t orphan_list[], int map_size){
	for(int i = 0; i < map_size; i++){//initialize orphan_list
		orphan_list[i] = (uint16_t) 0;
	}//end for

}

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

	int map_size = 2849; //2880 - 1 - 9 - 9 - 14  + 2 = 2849
	int reference_map[map_size];//means referenced, 0 means not referenced
	initialize_reference_map(reference_map, map_size);
	resolve_inconsistencies_and_populate_map(root_dir_start_clust, image_buf, bpb, reference_map);
	uint16_t orphan_list[map_size];	
	initialize_orphan_list(orphan_list, map_size);
	find_orphans(reference_map, orphan_list, map_size, image_buf, bpb);
	print_orphans(orphan_list);
	house_orphans(orphan_list, image_buf, bpb);

	/*TEST
	for(int i = 0; i < map_size; i++){
		printf("%d  ", orphan_list[i]);
		if( (i+1) % 8 == 0){
			printf("\n");	
		}
	}
	//TEST_END */

	

    unmmap_file(image_buf, &fd);	
	free(bpb);

    return 0;
}
