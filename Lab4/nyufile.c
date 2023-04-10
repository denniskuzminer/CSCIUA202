#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <openssl/sha.h>

#define SHA_DIGEST_LENGTH 20

unsigned char *SHA1(const unsigned char *d, size_t n, unsigned char *md);

unsigned char fileHash[SHA_DIGEST_LENGTH];
char *usage = "Usage: ./nyufile disk <options>\n  -i                     Print the file system information.\n  -l                     List the root directory.\n  -r filename [-s sha1]  Recover a contiguous file.\n  -R filename -s sha1    Recover a possibly non-contiguous file.";
int fd;
struct stat sb;

#pragma pack(push, 1)
typedef struct BootEntry
{
    unsigned char BS_jmpBoot[3];    /* Assembly instruction to jump to boot code */
    unsigned char BS_OEMName[8];    /* OEM Name in ASCII */
    unsigned short BPB_BytsPerSec;  /* Bytes per sector. Allowed values include 512,
      1024, 2048, and 4096 */
    unsigned char BPB_SecPerClus;   /* Sectors per cluster (data unit). Allowed values
       are powers of 2, but the cluster size must be 32KB
       or smaller */
    unsigned short BPB_RsvdSecCnt;  /* Size in sectors of the reserved area */
    unsigned char BPB_NumFATs;      /* Number of FATs */
    unsigned short BPB_RootEntCnt;  /* Maximum number of files in the root directory for
      FAT12 and FAT16. This is 0 for FAT32 */
    unsigned short BPB_TotSec16;    /* 16-bit value of number of sectors in file system */
    unsigned char BPB_Media;        /* Media type */
    unsigned short BPB_FATSz16;     /* 16-bit size in sectors of each FAT for FAT12 and
         FAT16. For FAT32, this field is 0 */
    unsigned short BPB_SecPerTrk;   /* Sectors per track of storage device */
    unsigned short BPB_NumHeads;    /* Number of heads in storage device */
    unsigned int BPB_HiddSec;       /* Number of sectors before the start of partition */
    unsigned int BPB_TotSec32;      /* 32-bit value of number of sectors in file system.
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

#pragma pack(push, 1)
typedef struct DirCluster
{
    DirEntry cluster[16];
} DirCluster;
#pragma pack(pop)

BootEntry *info;

void getDiskInfo(char *fileName)
{
    fd = open(fileName, O_RDWR);
    if (fd == -1)
    {
        perror("open");
        exit(1);
    }
    if (fstat(fd, &sb) == -1)
    {
        perror("fstat");
        exit(1);
    }
    info = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (info == MAP_FAILED)
    {
        perror("mmap");
        exit(1);
    }
}

void printInfo()
{
    printf("Number of FATs = %d\n", info->BPB_NumFATs);
    printf("Number of bytes per sector = %d\n", info->BPB_BytsPerSec);
    printf("Number of sectors per cluster = %d\n", info->BPB_SecPerClus);
    printf("Number of reserved sectors = %d\n", info->BPB_RsvdSecCnt);
}

void removeSpaces(char *str, char *ref)
{
    int count = 0;
    for (int i = 0; ref[i + 1]; i++)
    {
        if (i == 8 && ref[i] != ' ')
            str[count++] = '.';
        if (ref[i] != ' ')
            str[count++] = ref[i];
    }
    str[count] = '\0';
}

char *formatDirName(char *name, DirEntry *de)
{
    for (int i = 0; name[i]; i++)
        name[i] = ' ';
    strcpy(name, de->DIR_Name);
    removeSpaces(name, de->DIR_Name);
    if (0x10 == de->DIR_Attr)
        strcat(name, "/");
    return name;
}

void listRootEntries()
{
    unsigned int RootDirSectors = (((unsigned int)info->BPB_RootEntCnt * 32) + ((unsigned int)info->BPB_BytsPerSec - 1)) / (unsigned int)info->BPB_BytsPerSec;
    unsigned int FirstDataSector = (unsigned int)info->BPB_RsvdSecCnt + ((unsigned int)info->BPB_NumFATs * (unsigned int)info->BPB_FATSz32) + RootDirSectors;
    unsigned int entries = 0;
    for (unsigned int buf = (unsigned int)info->BPB_RootClus; buf < 0x0FFFFFF8;)
    {
        unsigned int FirstSectorofCluster = ((buf - 2) * (unsigned int)info->BPB_SecPerClus) + FirstDataSector;
        unsigned int next = ((info->BPB_RsvdSecCnt * (unsigned int)info->BPB_BytsPerSec) + 4 * buf);
        for (unsigned int i = 0; i < (((unsigned int)info->BPB_BytsPerSec * (unsigned int)info->BPB_SecPerClus) / 32); i++)
        {
            DirEntry *de = (DirEntry *)(mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0) + (FirstSectorofCluster * info->BPB_BytsPerSec) + (i * 32));
            if (de->DIR_Name[0] == '\0')
                break;
            if (de->DIR_Name[0] == 0xe5)
                continue;
            char name[13];
            printf("%s (size = %d, starting cluster = %d)\n", formatDirName(name, de), de->DIR_FileSize, (unsigned int)de->DIR_FstClusHI + (unsigned int)de->DIR_FstClusLO);
            entries++;
        }
        buf = *(unsigned int *)(mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0) + next);
    }
    printf("Total number of entries = %d\n", entries);
}

void recoverFile(char *fileName)
{
    int RootDirSectors = (((int)info->BPB_RootEntCnt * 32) + ((int)info->BPB_BytsPerSec - 1)) / (int)info->BPB_BytsPerSec;
    int FirstDataSector = (int)info->BPB_RsvdSecCnt + ((int)info->BPB_NumFATs * (int)info->BPB_FATSz32) + RootDirSectors;
    int numFilesFound = 0;
    char *foundName;
    unsigned int foundFatEntryLoc;
    unsigned int foundFileSize;
    for (unsigned int buf = (unsigned int)info->BPB_RootClus; buf < 0x0FFFFFF8;)
    {
        int FirstSectorofCluster = ((buf - 2) * (int)info->BPB_SecPerClus) + FirstDataSector;
        for (int i = 0; i < (((unsigned int)info->BPB_BytsPerSec * (unsigned int)info->BPB_SecPerClus) / 32); i++)
        {
            DirEntry *de = (DirEntry *)(mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0) + (FirstSectorofCluster * info->BPB_BytsPerSec) + (i * 32));
            if (de->DIR_Name[0] == '\0')
                break;
            char name[13];
            char deletedName[13];
            strcpy(deletedName, fileName);
            deletedName[0] = 0xe5;
            if (strcmp(formatDirName(name, de), deletedName) == 0 && de->DIR_Attr != 0x10)
            {
                numFilesFound++;
                foundName = &(de->DIR_Name[0]);
                foundFatEntryLoc = de->DIR_FstClusLO + de->DIR_FstClusHI;
                foundFileSize = de->DIR_FileSize;
                if (numFilesFound > 1)
                    break;
                if (fileHash != NULL)
                {
                    unsigned char currHash[20];
                    // SHA1((char *)(mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0) + (foundFatEntryLoc * info->BPB_BytsPerSec * info->BPB_SecPerClus)), de->DIR_FileSize, currHash);
                    if (fileHash == currHash)
                    {
                    }
                }
            }
        }
        if (numFilesFound > 1)
            break; // 2b18dc55dc7c0ceb042a35dc73cbc95c38d78052
        // 01ba98b3c90126f14577d5b1fdb1ffe9d3364469
        int next = ((info->BPB_RsvdSecCnt * (int)info->BPB_BytsPerSec) + 4 * buf);
        buf = *(unsigned int *)(mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0) + next);
    }
    if (numFilesFound == 1)
    {
        foundName[0] = fileName[0];
        unsigned int *foundFatEntry = (unsigned int *)(mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0) + ((info->BPB_RsvdSecCnt * (int)info->BPB_BytsPerSec) + 4 * foundFatEntryLoc));
        if ((int)foundFileSize <= ((int)info->BPB_BytsPerSec * ((int)info->BPB_SecPerClus)))
        {
            *foundFatEntry = foundFatEntryLoc == 2 ? 0x0FFFFFF8 : 0x0FFFFFFF;
        }
        else
        {
            for (int i = 1; (i * (info->BPB_SecPerClus * (int)info->BPB_BytsPerSec)) < foundFileSize; i++)
            {
                *foundFatEntry = (unsigned int)foundFatEntryLoc + i;
                foundFatEntry = (unsigned int *)(mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0) + ((info->BPB_RsvdSecCnt * (int)info->BPB_BytsPerSec) + 4 * (foundFatEntryLoc + i)));
            }
            *foundFatEntry = 0x0FFFFFFF;
        }
        printf("%s: successfully recovered\n", fileName);
    }
    else if (numFilesFound > 1)
    {
        printf("%s: multiple candidates found\n", fileName);
    }
    else
    {
        printf("%s: file not found\n", fileName);
    }
}

int main(int argc, char **argv)
{
    argv[1] = "./fat32_3.disk";
    argv[2] = "-r";
    argv[3] = "BIGWORDS.TXT";
    argv[4] = "-s";
    argv[3] = "01ba98b3c90126f14577d5b1fdb1ffe9d3364469";
    // if (argc <= 2 || argc == 5 || argc >= 7)
    // {
    //     printf("%s\n", usage);
    //     return 1;
    // }
    getDiskInfo(argv[1]);
    if (strcmp(argv[2], "-i") == 0)
    {
        printInfo();
    }
    else if (strcmp(argv[2], "-l") == 0)
    {
        listRootEntries();
    }
    else if (strcmp(argv[2], "-r") == 0)
    {
        // if (argc <= 3)
        // {
        //     printf("%s\n", usage);
        //     return 1;
        // }
        // if (argc == 6)
        // {
        if (strcmp(argv[4], "-s") == 0)
        {
            memcpy(fileHash, argv[5], SHA_DIGEST_LENGTH);
        }
        else
        {
            printf("%s\n", usage);
            return 1;
        }
        // }
        // else
        // {
        //     printf("%s\n", usage);
        //     return 1;
        // }

        recoverFile(argv[3]);
    }
    else if (strcmp(argv[2], "-R") == 0)
    {
    }
    else
    {
        printf("%s\n", usage);
    }
}