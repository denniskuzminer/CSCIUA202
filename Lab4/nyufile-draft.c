#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

// Boot sector
#pragma pack(push, 1)
typedef struct BootEntry
{
    unsigned char BS_jmpBoot[3];   /* Assembly instruction to jump to boot code */
    unsigned char BS_OEMName[8];   /* OEM Name in ASCII */
    unsigned short BPB_BytsPerSec; // Bytes per sector. Allowed values include 512,
                                   //  1024, 2048, and 4096 */
    unsigned char BPB_SecPerClus;  /* Sectors per cluster (data unit). Allowed values
                 are powers of 2, but the cluster size must be 32KB
         or smaller */
    unsigned short BPB_RsvdSecCnt; /* Size in sectors of the reserved area */
    unsigned char BPB_NumFATs;     /* Number of FATs */
    unsigned short BPB_RootEntCnt; /* Maximum number of files in the root directory for
                    FAT12 and FAT16. This is 0 for FAT32 */
    unsigned short BPB_TotSec16;   /* 16-bit value of number of sectors in file system */
    unsigned char BPB_Media;       /* Media type */
    unsigned short BPB_FATSz16;    /* 16-bit size in sectors of each FAT for FAT12 and
                   FAT16. For FAT32, this field is 0 */
    unsigned short BPB_SecPerTrk;  /* Sectors per track of storage device */
    unsigned short BPB_NumHeads;   /* Number of heads in storage device */
    unsigned int BPB_HiddSec;      /* Number of sectors before the start of partition */
    unsigned int BPB_TotSec32;     /* 32-bit value of number of sectors in file system.
                 Either this value or the 16-bit value above must be
                    0 */

    unsigned int BPB_FATSz32;       /* 32-bit size in sectors of one FAT */
    unsigned short BPB_ExtFlags;    /* A flag for FAT */
    unsigned short BPB_FSVer;       /* The major and minor version number */
    unsigned int BPB_RootClus;      /* Cluster where the root directory can be
      found */
    unsigned short BPB_FSInfo;      /* Sector where FSINFO structure can be
      found */
    unsigned short BPB_BkBootSec;   /* Sector where backup copy of boot sector is
   located */
    unsigned char BPB_Reserved[12]; /* Reserved */
    unsigned char BS_DrvNum;        /* BIOS INT13h drive number */
    unsigned char BS_Reserved1;     /* Not used */
    unsigned char BS_BootSig;       /* Extended boot signature to identify if the
       next three values are valid */
    unsigned int BS_VolID;          /* Volume serial number */
    unsigned char BS_VolLab[11];    /* Volume label in ASCII. User defines when
    creating the file system */
    unsigned char BS_FilSysType[8]; /* File system type label in ASCII */

} BootEntry;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct DirEntry
{
    unsigned char DIR_Name[11];     /* File name */
    unsigned char DIR_Attr;         /* File attributes */
    unsigned char DIR_NTRes;        /* Reserved */
    unsigned char DIR_CrtTimeTenth; /* Created time (tenths of second) */
    unsigned short DIR_CrtTime;     /* Created time (hours, minutes, seconds) */
    unsigned short DIR_CrtDate;     /* Created day */
    unsigned short DIR_LstAccDate;  /* Accessed day */
    unsigned short DIR_FstClusHI;   /* High 2 bytes of the first cluster address */
    unsigned short DIR_WrtTime;     /* Written time (hours, minutes, seconds */
    unsigned short DIR_WrtDate;     /* Written day */
    unsigned short DIR_FstClusLO;   /* Low 2 bytes of the first cluster address */
    unsigned int DIR_FileSize;      /* File size in bytes. (0 for directories) */
} DirEntry;
#pragma pack(pop)

// global variables
BootEntry *boot_entry;
DirEntry *dir_entry;

struct stat sb;
int fd; // catch error plz
FILE *fp;

bool isDirectory(DirEntry *dir_entry)
{
    // bool is not a primitive data type in C!.. need to import.
    if (dir_entry->DIR_Attr == 0x10)
        return true;

    return false;
}

// from https://stackoverflow.com/questions/1726302/remove-spaces-from-a-string-in-c
void remove_spaces(char *s)
{
    char *d = s;
    do
    {
        while (*d == ' ')
        {
            ++d;
        }
    } while (*s++ = *d++);

    // strtok(s, "\n");
}

char *formatName(char *name, DirEntry *dir_entry)
{
    // remove spaces
    char *new_name = malloc(sizeof(name) + 1);
    new_name = name;

    // char c;
    //  if directory, add / at the end
    if (isDirectory(dir_entry))
    {

        // char slash = '/';
        remove_spaces(new_name);

        // printf("%s\n", name);
        // strncat(new_name, &slash, 1);
        new_name[strlen(name) - 1] = '/';
        // strcpy(new_name, name + '/');
        // remove_spaces(name);
        // printf("%s\n", new_name);
        return new_name;
    }
    // empty file?
    else if ((int)dir_entry->DIR_FstClusHI + (int)dir_entry->DIR_FstClusLO == 0)
    {
        remove_spaces(name);
        // name[11] = '\0';
        // new_name[strlen(name)] = '\0';
        return name;
    }

    else
    {
        // add the . in between file name and type
        /*
        - DIR_Name[0..7] is the file name
        * padded with spaces , not NULL-terminated.
        - DIR_Name[8..10] is the file extension
        - DIR_Name is NOT null-terminated.
        */

        char *ext_name = malloc(11); // extension name

        int i;
        int j; // pointer for ext_name
        for (i = 0, j = 0; i < 11; i++, j++)
        {

            if (i == 8 && name[i] != ' ')
            {
                ext_name[j] = '.';
                j++;
            }

            ext_name[j] = name[i];
        }
        // ext_name[11] = '\0';
        remove_spaces(ext_name);

        // printf("%c", ext_name[strlen(name)]);
        //  ext_name[strlen(name)-1] = 'f';
        return ext_name;
    }

    return name;
}
/*
 * Milestone 2: print the file system information
 */
void printFSInfo()
{
    // milestone 2: print file system information.

    // just use -> to reference the different attributes of the boot sector.

    printf("Number of FATs = %u\n", boot_entry->BPB_NumFATs);
    printf("Number of bytes per sector = %u\n", boot_entry->BPB_BytsPerSec);
    printf("Number of sectors per cluster = %u\n", boot_entry->BPB_SecPerClus);
    printf("Number of reserved sectors = %u\n", boot_entry->BPB_RsvdSecCnt);
}

uint32_t *FAT;
void readFAT()
{
    int FAT_location = boot_entry->BPB_RsvdSecCnt * boot_entry->BPB_BytsPerSec;
    size_t FAT_size = boot_entry->BPB_FATSz32 * boot_entry->BPB_BytsPerSec * boot_entry->BPB_NumFATs;

    uint16_t FAT_buf[FAT_size];

    // Read the FAT
    unsigned int bytes_read = fread(FAT_buf, 1, FAT_size, fp);

    // if (bytes_read < fat_size), error

    FAT = (uint32_t *)FAT_buf;

    // FAT has been read.
}

/*
 * Milestone 3: list the root directory
 */
void listRootDirectory()
{
    // cluster = basic unit of logical file access, in which, the cluster where the root directory can be found is in BPB_RootClus = 2
    // a cluster is a group of contiguous sectors. smallest addressable unit (4096-32768 bytes)
    // default sector size: 512 bytes. think boot sector.

    /*

    HOW TO READ A FILE

    ---Step 1: read the first cluster (datta block) from the directory entry

    ---Step 2: look up the next cluster in the FAT and read that cluster.

    ---Step 3: repeat the process until an EOF entry is found at the FAT
    EOF >= 0x0ffffff8
    ^ last cluster of file. linked list stops there.

    */

    /*
    1. Get the start address of the data area, store it as a DirEntry pointer
     2. Print everything
     3. Increment DirEntry
     4. Check for 0x00, stop the loop when so and exit
     5. go back to 2.

     */

    // compute the starting sector of data area.. located after FAT.
    // from discord
    int RootDirSectors = boot_entry->BPB_RsvdSecCnt + (boot_entry->BPB_NumFATs * boot_entry->BPB_FATSz32);
    // compute starting sector of root directory..
    int FirstDataSector = RootDirSectors + ((int)(boot_entry->BPB_RootClus - 2) * (int)boot_entry->BPB_SecPerClus);

    void *mapped_address;
    mapped_address = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    // dir_entry = (DirEntry *) (mapped_address + FirstCluster * boot_entry->BPB_BytsPerSec);

    uint32_t cluster_num = 2;
    int num_entries = 0;

    // int FirstCluster = ((cluster_num - 2) * (int)boot_entry->BPB_SecPerClus) + FirstDataSector; // cluster_num-2 because first two sectors hold special data. they are omitted from the calculations
    // readFAT();

    int FirstCluster = ((cluster_num - 2) * (int)boot_entry->BPB_SecPerClus) + FirstDataSector; // cluster_num-2 because first two sectors hold special data. they are omitted from the calculations

    while (cluster_num < 0x0FFFFFF8)
    {
        unsigned int next = ((boot_entry->BPB_RsvdSecCnt * (unsigned int)boot_entry->BPB_BytsPerSec) + 4 * cluster_num);
        FirstCluster = ((cluster_num - 2) * (unsigned int)boot_entry->BPB_SecPerClus) + FirstDataSector;
        // step 2- read the next cluster.

        // every entry is 16 bytes.. so go through max 16 bytes
        // printf("%d\n", boot_entry->BPB_BytsPerSec/boot_entry->BPB_RsvdSecCnt);
        // for (int i=0; i < boot_entry->BPB_BytsPerSec/boot_entry->BPB_RsvdSecCnt; i++) {
        for (int i = 0; i < (((unsigned int)boot_entry->BPB_BytsPerSec * (unsigned int)boot_entry->BPB_SecPerClus) / 32); i++)
        {
            // for (int i = 0; )
            // printf("%s\n", dir_entry->DIR_Name);

            // dir_entry = (DirEntry *) (mapped_address + FirstCluster * boot_entry->BPB_BytsPerSec);
            // dir_entry = (DirEntry *) (mapped_address + (FirstCluster * boot_entry->BPB_BytsPerSec));
            dir_entry = (DirEntry *)(mapped_address + (FirstCluster * boot_entry->BPB_BytsPerSec) + (i * 32));

            // printf("%u\n", dir_entry);
            if (dir_entry->DIR_Name[0] == '\0')
            {

                break;
            }
            // skip deleted files.
            if (dir_entry->DIR_Name[0] == 0xe5)
            {
                // mapped_address += 32;
                continue;
            }

            // remove spaces, and if dir_Attr is a directory, add /
            char *name = malloc(11);
            strcpy(name, (char *)dir_entry->DIR_Name);

            char *formatted_name = formatName(name, dir_entry);
            printf("%s (size = %d, starting cluster = %d)\n", formatted_name, dir_entry->DIR_FileSize, dir_entry->DIR_FstClusHI + dir_entry->DIR_FstClusLO);

            // increment dir_entry by sizeof(dir_entry)
            // direntry is a consecutive 32-byte data
            // mapped_address += 32;
            num_entries++;
        }
        // step 2
        // read the FAT to determine the next cluster in the cluster chain
        // int FAT_location = boot_entry->BPB_RsvdSecCnt * boot_entry->BPB_BytsPerSec;
        // int FAT_size = boot_entry->BPB_FATSz32 * boot_entry->BPB_BytsPerSec;

        // int NextCluster = ((boot_entry->BPB_RsvdSecCnt * (int)boot_entry->BPB_BytsPerSec) + 4 * cluster_num);
        // cluster_num = FAT[cluster_num];
        cluster_num = *(unsigned int *)(mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0) + next);

        // cluster_num = *(unsigned int *)(mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0) + NextCluster);
        // cluster_num = *(unsigned int *)mapped_address + NextCluster;
        // mapped_address += 0x20;

        // step 3   repeat the process until an EOF entry is found at the FAT
        // EOF >= 0x0ffffff8
    }
    // printf("hey\n");
    printf("Total number of entries = %d\n", num_entries);
    // if (num_entries == 0) break;
}

/* Milestone 4 */
void recoverSmallFile(char *filename)
{
    /*

    if the file size is no more than one cluster, the recovery can be easiily done.
    * the first cluster address is still searchable.
    * note that files with the same suffix may also be found.

    */

    /*
     approach:
         1. loop through the data sector. and find a match with the filename.

    */
}

/*
 * Milestone 1 - validate usage
 *
 * Usage:      ./nyufile disk <options>
 * -i                      Print the file system information
 * -l                      List the root directory
 * -r filename [-s sha1]   Recover a contiguous file.
 * -R filename -s sha1     Recover a possibly non-contiguous file
 *
 *  Check if the command-line arguments are valid.
 *  If not, print the above usage information and exit.
 */
int main(int argc, char *argv[])
{

    argv[1] = "./fat32_3.disk";
    argv[2] = "-l";
    // if (argc < 3) {
    //     printf("Usage: ./nyufile disk <options>\n");
    //     printf("  -i                     Print the file system information.\n");
    //     printf("  -l                     List the root directory.\n");
    //     printf("  -r filename [-s sha1]  Recover a contiguous file.\n");
    //     printf("  -R filename -s sha1    Recover a possibly non-contiguous file.\n");
    //     return 0;
    // }

    int ch;
    fd = open(argv[1], O_RDWR); // catch error plz
    fp = fopen(argv[1], "r+");

    fstat(fd, &sb);
    char *f;
    f = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    boot_entry = (BootEntry *)f;
    // BootEntry * boot_entry = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);

    int flag_i = 0;
    int flag_l = 1;
    int flag_r = 0;
    int flag_R = 0;
    int flag_s = 0;

    char *filename = malloc(1000);
    char *hash = malloc(1000);

    while ((ch = getopt(argc, argv, "ilr:R:")) != -1)
    {
        switch (ch)
        {
        case 'i':
            // print the file system information. milestone 2
            // read boot sector into BootEntry data structure.
            flag_i = 1;

            // printFSInfo();

            break;
        case 'l':
            flag_l = 1;
            // should list all valid entries in the root directory with
            // filename, file size, starting cluster.
            // void * mapped_address = (void *) f;
            // DirEntry * dir_entry = (DirEntry *)(mapped_address + 0x5000);

            // listRootDirectory(argv, boot_entry, mapped_address);
            // listRootDirectory();
            break;
        case 'r':
            flag_r = 1;
            filename = optarg;
            break;
        case 'R':
            flag_R = 1;
            filename = optarg;
            break;
        case 's':
            flag_s = 1;
            hash = optarg;
        default:
            // trap invalid arg?
            printf("Usage: ./nyufile disk <options>\n");
            printf("  -i                     Print the file system information.\n");
            printf("  -l                     List the root directory.\n");
            printf("  -r filename [-s sha1]  Recover a contiguous file.\n");
            printf("  -R filename -s sha1    Recover a possibly non-contiguous file.\n");
            // exit
        }
    }

    if (flag_i == 1)
    {
        printFSInfo();
    }
    else if (flag_l == 1)
    {
        listRootDirectory();
    }
    else if (flag_r == 1)
    {
        recoverSmallFile(filename);
    }

    return 0;
}
