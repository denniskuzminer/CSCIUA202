#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <openssl/sha.h>

int debugFlag = 0;

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define CLUSTER_NUM_MAX 65537

// 未使用cluster
#define FAT_CLUSTER_NOT_USED 0x0
// 已分配cluster
#define FAT_CLUSTER_USED_START    0x00000002
#define FAT_CLUSTER_USED_END      0x0FFFFFFE
// 保留cluster
#define FAT_CLUSTER_RESVED_START  0x0FFFFFF0
#define FAT_CLUSTER_RESVED_END    0x0FFFFFF6
// 坏cluster
#define FAT_CLUSTER_BAD           0x0FFFFFF7
// 文件结束cluster
#define FAT_CLUSTER_FILEEND_START 0x0FFFFFF8
#define FAT_CLUSTER_FILEEND_END   0x0FFFFFFF

#pragma pack(push,1)
typedef struct BootEntry {
  unsigned char  BS_jmpBoot[3];     // Assembly instruction to jump to boot code
  unsigned char  BS_OEMName[8];     // OEM Name in ASCII
  unsigned short BPB_BytsPerSec;    // Bytes per sector. Allowed values include 512, 1024, 2048, and 4096
  unsigned char  BPB_SecPerClus;    // Sectors per cluster (data unit). Allowed values are powers of 2, but the cluster size must be 32KB or smaller
  unsigned short BPB_RsvdSecCnt;    // Size in sectors of the reserved area
  unsigned char  BPB_NumFATs;       // Number of FATs
  unsigned short BPB_RootEntCnt;    // Maximum number of files in the root directory for FAT12 and FAT16. This is 0 for FAT32
  unsigned short BPB_TotSec16;      // 16-bit value of number of sectors in file system
  unsigned char  BPB_Media;         // Media type
  unsigned short BPB_FATSz16;       // 16-bit size in sectors of each FAT for FAT12 and FAT16. For FAT32, this field is 0
  unsigned short BPB_SecPerTrk;     // Sectors per track of storage device
  unsigned short BPB_NumHeads;      // Number of heads in storage device
  unsigned int   BPB_HiddSec;       // Number of sectors before the start of partition
  unsigned int   BPB_TotSec32;      // 32-bit value of number of sectors in file system. Either this value or the 16-bit value above must be 0
  unsigned int   BPB_FATSz32;       // 32-bit size in sectors of one FAT
  unsigned short BPB_ExtFlags;      // A flag for FAT
  unsigned short BPB_FSVer;         // The major and minor version number
  unsigned int   BPB_RootClus;      // Cluster where the root directory can be found
  unsigned short BPB_FSInfo;        // Sector where FSINFO structure can be found
  unsigned short BPB_BkBootSec;     // Sector where backup copy of boot sector is located
  unsigned char  BPB_Reserved[12];  // Reserved
  unsigned char  BS_DrvNum;         // BIOS INT13h drive number
  unsigned char  BS_Reserved1;      // Not used
  unsigned char  BS_BootSig;        // Extended boot signature to identify if the next three values are valid
  unsigned int   BS_VolID;          // Volume serial number
  unsigned char  BS_VolLab[11];     // Volume label in ASCII. User defines when creating the file system
  unsigned char  BS_FilSysType[8];  // File system type label in ASCII
} BootEntry;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct DirEntry {
  unsigned char  DIR_Name[11];      // File name
  unsigned char  DIR_Attr;          // File attributes
  unsigned char  DIR_NTRes;         // Reserved
  unsigned char  DIR_CrtTimeTenth;  // Created time (tenths of second)
  unsigned short DIR_CrtTime;       // Created time (hours, minutes, seconds)
  unsigned short DIR_CrtDate;       // Created day
  unsigned short DIR_LstAccDate;    // Accessed day
  unsigned short DIR_FstClusHI;     // High 2 bytes of the first cluster address
  unsigned short DIR_WrtTime;       // Written time (hours, minutes, seconds
  unsigned short DIR_WrtDate;       // Written day
  unsigned short DIR_FstClusLO;     // Low 2 bytes of the first cluster address
  unsigned int   DIR_FileSize;      // File size in bytes. (0 for directories)
} DirEntry;
#pragma pack(pop)

// global val
int clusterUsed[CLUSTER_NUM_MAX]; // 标记每一个cluster的使用状态，cluster 2对应的index是0
BootEntry *pBootEntry = NULL; // boot
unsigned int clusterSize = 0; // cluster size
unsigned int fat1StartAddr = 0; // fat1
unsigned int fat2StartAddr = 0; // fat2
DirEntry *pDirEntry = NULL;  // 端目录项
unsigned int tableStartAddr = 0; // file table addr
unsigned int fileStartAddr = 0; // for single file


// 根据地址，计算cluster index，从2开始
unsigned int getClusterNobyAddr(unsigned int addr)
{
    return (addr - tableStartAddr) / clusterSize + 2;
}

// 根据index，计算开始地址
unsigned int getClusterAddrbyNo(unsigned int clusterNo)
{
    return tableStartAddr + clusterSize * (clusterNo - 2);
}


int main(int argc, const char * argv[]) 
{
    char disk[128] = {0};
    int fd = -1;
    unsigned char *p_map = NULL;
    unsigned long int file_size = 0;
    struct stat file_stat;

    // check input
    if((argc < 3) || (strcmp("-i", argv[2]) && strcmp("-l", argv[2]) && strcmp("-r", argv[2]) && strcmp("-r", argv[2]) && strcmp("-R", argv[2])))
    {
        printf("Usage: ./nyufile disk <options>\n");
        printf("  -i                     Print the file system information.\n");
        printf("  -l                     List the root directory.\n");
        printf("  -r filename [-s sha1]  Recover a contiguous file.\n");
        printf("  -R filename -s sha1    Recover a possibly non-contiguous file.\n");
        return 0;
    }

    // get disk name
    sprintf(disk, "%s", argv[1]);
    // printf("DEBUG:disk: %s\n", disk);

    // -i
    if(strcmp("-i", argv[2]) == 0)
    {
        char command[128] = {0};
        char line[128] = {0};
        FILE *fp = NULL;
        int fatNum = 0, bytesPerSector = 0, bytesPerCluster = 0, reservedSectorNum = 0;
        sprintf(command, "fsck.fat -v %s > tmp.txt", disk);
        system(command);

        // get data from file
        if(NULL==(fp=fopen("tmp.txt","r")))
        {
            printf("open tmp.txt fail!\n");
            return -1;
        }
        while(fgets(line, 128, fp) != NULL)
        {
            if(strstr(line, "FATs"))
            {
                fatNum = strtol(line, NULL, 10);
            }
            else if(strstr(line, "bytes per logical sector"))
            {
                bytesPerSector = strtol(line, NULL, 10);
            }
            else if(strstr(line, "bytes per cluster"))
            {
                bytesPerCluster = strtol(line, NULL, 10);
            }
            else if(strstr(line, "reserved sectors"))
            {
                reservedSectorNum = strtol(line, NULL, 10);
            }
        }
        fclose(fp);
        memset(command, 0, sizeof(command));
        sprintf(command, "rm -rf tmp.txt", disk);
        system(command);

        // display
        printf("Number of FATs = %d\nNumber of bytes per sector = %d\nNumber of sectors per cluster = %d\nNumber of reserved sectors = %d\n",
               fatNum, bytesPerSector, bytesPerCluster/bytesPerSector, reservedSectorNum);
        return 0;
    }

    // -l
    else if(strcmp("-l", argv[2]) == 0)
    {
        char name[64] = {0};
        unsigned int size = 0;
        unsigned int startCluster = 0;
        unsigned int totalNum = 0;
        unsigned int index = 0;
        // read file
        fd = open(disk, O_RDONLY);
        if(fd == -1)
        {
            printf("ERROR: read disk fail.\n");
            return -1;
        }
        fstat(fd, &file_stat);
        file_size = file_stat.st_size;
        // map
        p_map = (unsigned char*)mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd, 0);
        if(p_map == MAP_FAILED)
        {
            printf("ERROR: mmap file fail.\n");
            return -1;
        }

        // get boot entry
        pBootEntry = (BootEntry *)p_map;
        // get cluster size
        clusterSize = pBootEntry->BPB_SecPerClus * pBootEntry->BPB_BytsPerSec;
        // get fat1 addr
        fat1StartAddr = pBootEntry->BPB_RsvdSecCnt * pBootEntry->BPB_BytsPerSec;
        // get fat 2 addr
        if(pBootEntry->BPB_NumFATs == 2)
            fat2StartAddr = fat1StartAddr + pBootEntry->BPB_FATSz32 * pBootEntry->BPB_BytsPerSec;
        else if(pBootEntry->BPB_NumFATs == 1)
            fat2StartAddr = fat1StartAddr;
        // get file table addr
        tableStartAddr = fat2StartAddr + pBootEntry->BPB_FATSz32 * pBootEntry->BPB_BytsPerSec;
        // debug
        if(debugFlag)
        {
            printf("BPB_BytsPerSec: %d\n", pBootEntry->BPB_BytsPerSec);
            printf("BPB_SecPerClus: %d\n", pBootEntry->BPB_SecPerClus);
            printf("BPB_RsvdSecCnt: %d\n", pBootEntry->BPB_RsvdSecCnt);
            printf("BPB_FATSz32: %d-\n", pBootEntry->BPB_FATSz32);
            printf("BPB_NumFATs: %d\n", pBootEntry->BPB_NumFATs);
            printf("clusterSize: %d\n", clusterSize);
            printf("fat1 start addr: %x\n", fat1StartAddr);
            printf("fat2 start addr: %x\n", fat2StartAddr);
            printf("fat size(bytes): %d\n", pBootEntry->BPB_FATSz32 * pBootEntry->BPB_BytsPerSec);
            printf("max cluster num: %d\n", pBootEntry->BPB_FATSz32 * pBootEntry->BPB_BytsPerSec / 4);
            printf("table start addr: %x\n", tableStartAddr);
        }
        
        // get Fat item entry
        unsigned int offset = tableStartAddr;
        for (int i = 0; i < 520; i++)
        {
            // if cluster is used, then skip
            int index = getClusterNobyAddr(offset);
            // printf("---current offset: %x--%d\n", offset, i);
            // printf("---current cluster: %d--\n", index);
            if(clusterUsed[index - 2] == 1)
            {
                // printf("--skip--\n");
                offset += clusterSize;
                continue;
            }

            // read item
            pDirEntry = (DirEntry *)(p_map + offset);
            if(pDirEntry->DIR_Name[0] == 0)
            {
                break; // no more entry
            }

            // get name
            memset(name, 0, sizeof(name));
            index = 0;
            for (int j = 0; j < 11; j++)
            {
                if((j == 8) && (pDirEntry->DIR_Name[j] != 0x20))
                {
                    name[index] = '.';
                    index++;
                }
                if (pDirEntry->DIR_Name[j] != 0x20)
                {
                    name[index] = pDirEntry->DIR_Name[j];
                    index++;
                }               
            }
            // get type: if direcotry, add /
            if ((pDirEntry->DIR_Attr & (ATTR_DIRECTORY | ATTR_VOLUME_ID)) == ATTR_DIRECTORY)
            {
                name[index] = '/';
                index++;
                // printf("DEBUG:this is directory.\n");
            }
                
            /*
            if((pDirEntry->DIR_Attr & (ATTR_DIRECTORY | ATTR_VOLUME_ID)) == 0x00)
                printf("this is file.\n");
            else if ((pDirEntry->DIR_Attr & (ATTR_DIRECTORY | ATTR_VOLUME_ID)) == ATTR_DIRECTORY)
                printf("this is directory.\n");
            else if ((pDirEntry->DIR_Attr & (ATTR_DIRECTORY | ATTR_VOLUME_ID)) == ATTR_VOLUME_ID)
                printf("this is volume lable.\n");
            */

            // get size
            size = pDirEntry->DIR_FileSize;
            // get starting cluster
            startCluster = pDirEntry->DIR_FstClusLO;

            //debug
            if(debugFlag)
            {
                // get file start addr (保留扇区数 + FAT表扇区数 * FAT表个数(2) + (文件起始簇号-2)*每簇扇区数)*每扇区字节数
                fileStartAddr = (pBootEntry->BPB_RsvdSecCnt + pBootEntry->BPB_FATSz32 * pBootEntry->BPB_NumFATs + 
                     (startCluster-2) * pBootEntry->BPB_SecPerClus) * pBootEntry->BPB_BytsPerSec;
                printf("DEBUG:file [%s] start addr: %x (size = %d, starting cluster = %d)\n", name, fileStartAddr, size, startCluster);
            
                // test: cluster no <---> addr
                // unsigned int tmpAddr = getClusterAddrbyNo(startCluster);
                // printf("----tmpAddr: %x\n", tmpAddr);
                // unsigned int tmpCluster = getClusterNobyAddr(fileStartAddr);
                // printf("----tmpCluster: %d\n", tmpCluster);
                // tmpCluster = getClusterNobyAddr(fileStartAddr + 512);
                // printf("----tmpCluster2: %d\n", tmpCluster);
            }

           // set cluster used
           if(startCluster >= 2)
               clusterUsed[startCluster - 2] = 1;
            // skip delete file
            char delByte = name[0];
            if (delByte == (char)0xe5) 
                continue;

            // display
            printf("%s (size = %d, starting cluster = %d)\n", name, size, startCluster);
            // count entry
            totalNum++;

            // next
            offset += 0x20;
        }
        printf("Total number of entries = %d\n", totalNum);

        // release
        close(fd);
        munmap(p_map, file_size);
        return 0;
    }

    // -r  recover file
    else if(strcmp("-r", argv[2]) == 0)
    {
        char fileName[64] = {0};
        unsigned int size = 0;
        unsigned int startCluster = 0;
        unsigned int totalNum = 0;
        unsigned int index = 0;
        char recoveryNmae[64] = {0};
        int findFlag = 0;
        int multipleFlag = 0;
        int findWithsha1Flag = 0;
        unsigned char sha1sum[64] = {0};

        // get recovery name
        if(argc < 4)
        {
            printf("ERROR: no recovery file.\n");
            return -1;
        }
        strcpy(recoveryNmae, argv[3]);

        // get sha1
        if((argc == 6) && (strcmp("-s", argv[4]) == 0))
        {
            strcpy(sha1sum, argv[5]);
            if(debugFlag) printf("--sha1sum: %s--\n", sha1sum);
        }

        // read file
        fd = open(disk, O_RDWR);
        if(fd == -1)
        {
            printf("ERROR: read disk fail.\n");
            return -1;
        }
        fstat(fd, &file_stat);
        file_size = file_stat.st_size;
        // map
        p_map = (unsigned char*)mmap(NULL, file_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        if(p_map == MAP_FAILED)
        {
            printf("ERROR: mmap file fail.\n");
            return -1;
        }

        // get boot entry
        pBootEntry = (BootEntry *)p_map;
        // get cluster size
        clusterSize = pBootEntry->BPB_SecPerClus * pBootEntry->BPB_BytsPerSec;
        // get fat1 addr
        fat1StartAddr = pBootEntry->BPB_RsvdSecCnt * pBootEntry->BPB_BytsPerSec;
        // get fat 2 addr
        if(pBootEntry->BPB_NumFATs == 2)
            fat2StartAddr = fat1StartAddr + pBootEntry->BPB_FATSz32 * pBootEntry->BPB_BytsPerSec;
        else if(pBootEntry->BPB_NumFATs == 1)
            fat2StartAddr = fat1StartAddr;
        // get file table addr
        tableStartAddr = fat2StartAddr + pBootEntry->BPB_FATSz32 * pBootEntry->BPB_BytsPerSec;
        // debug
        if(debugFlag)
        {
            printf("BPB_BytsPerSec: %d\n", pBootEntry->BPB_BytsPerSec);
            printf("BPB_SecPerClus: %d\n", pBootEntry->BPB_SecPerClus);
            printf("BPB_RsvdSecCnt: %d\n", pBootEntry->BPB_RsvdSecCnt);
            printf("BPB_FATSz32: %d-\n", pBootEntry->BPB_FATSz32);
            printf("BPB_NumFATs: %d\n", pBootEntry->BPB_NumFATs);
            printf("clusterSize: %d\n", clusterSize);
            printf("fat1 start addr: %x\n", fat1StartAddr);
            printf("fat2 start addr: %x\n", fat2StartAddr);
            printf("fat size(bytes): %d\n", pBootEntry->BPB_FATSz32 * pBootEntry->BPB_BytsPerSec);
            printf("max cluster num: %d\n", pBootEntry->BPB_FATSz32 * pBootEntry->BPB_BytsPerSec / 4);
            printf("table start addr: %x\n", tableStartAddr);
        }
        
        // get Fat item entry
        unsigned int offset = tableStartAddr;
        for (int i = 0; i < 520; i++)
        {
            // if cluster is used, then skip
            int index = getClusterNobyAddr(offset);
            if(clusterUsed[index - 2] == 1)
            {
                offset += clusterSize;
                continue;
            }

            // read item
            pDirEntry = (DirEntry *)(p_map + offset);
            if(pDirEntry->DIR_Name[0] == 0)
            {
                break; // no more entry
            }
            // count entry
            totalNum++;

            // get name
            memset(fileName, 0, sizeof(fileName));
            index = 0;
            for (int j = 0; j < 11; j++)
            {
                if((j == 8) && (pDirEntry->DIR_Name[j] != 0x20))
                {
                    fileName[index] = '.';
                    index++;
                }
                if (pDirEntry->DIR_Name[j] != 0x20)
                {
                    fileName[index] = pDirEntry->DIR_Name[j];
                    index++;
                }               
            }

            // get size
            size = pDirEntry->DIR_FileSize;
            // get starting cluster
            startCluster = pDirEntry->DIR_FstClusLO;
           // set cluster used
           if(startCluster >= 2)
               clusterUsed[startCluster - 2] = 1;

            // find the file
            char delByte = fileName[0];
            if(debugFlag) printf("%s(%2x) (size = %d, starting cluster = %d)\n", fileName, delByte, size, startCluster);
            if (delByte == (char)0xe5)
            {
                fileName[0] = recoveryNmae[0];
                if(!strcmp(fileName, recoveryNmae))
                {
                    if(debugFlag) printf("--find file [%s] offset:%x--\n", recoveryNmae, offset);
                    if(argc == 6)
                    {
                        // calculate sha1sum
                        unsigned char sha1sumTmp[64] = {0};
                        if(size == 0)
                        {
                            strcpy(sha1sumTmp, "da39a3ee5e6b4b0d3255bfef95601890afd80709");
                        }
                        else
                        {
                            unsigned int contentOffset = getClusterAddrbyNo(startCluster);
                            unsigned char contentSum[64] = {0};
                            (void)SHA1((char *)(p_map + contentOffset), size, contentSum);
                            if(debugFlag)
                            {
                                printf("--contentOffset [%x]--\n--contentSum:--\n", contentOffset);
                                for (int x = 0; x < 20; x++)
                                    printf("%02x", contentSum[x]);
                                printf("\n\n");
                            }
                            
                            // transfer
                            for (int x = 0; x < 20; x++)
                                    sprintf(sha1sumTmp + x*2, "%02x", contentSum[x]);
                        }

                        if(debugFlag) printf("--sha1sumTmp:%s--\n", sha1sumTmp);
                        if (!strcmp(sha1sum, sha1sumTmp))
                        {
                            if (debugFlag)
                                printf("--find With sha1 found--\n");
                            findWithsha1Flag = 1;
                        }
                    }
                    else
                    {
                        findFlag = 1;
                        if(debugFlag) printf("--findFlag set to 1--\n");
                    }

                    // check if  multiple candidates found
                    unsigned int tmpOffset = offset + 0x20;
                    for (int k = i + 1; k < 520; k++)
                    {
                        // already found matched sha1, exit
                        if(findWithsha1Flag) break;

                        // read item
                        if(debugFlag) printf("--find offset:%x--\n", tmpOffset);
                        DirEntry *pDirEntry2 = (DirEntry *)(p_map + tmpOffset);
                        if(pDirEntry2->DIR_Name[0] == 0)
                        {
                            break; // no more entry
                        }
                        // get name
                        char tmpFileNmae[64] = {0};
                        int tmpIndex = 0;
                        unsigned int tmpSize = 0;
                        unsigned int tmpStartCluster = 0;
                        for (int m = 0; m < 11; m++)
                        {
                            if((m == 8) && (pDirEntry2->DIR_Name[m] != 0x20))
                            {
                                tmpFileNmae[tmpIndex] = '.';
                                tmpIndex++;
                            }
                            if (pDirEntry2->DIR_Name[m] != 0x20)
                            {
                                tmpFileNmae[tmpIndex] = pDirEntry2->DIR_Name[m];
                                tmpIndex++;
                            }               
                        }
                        // get size
                        tmpSize = pDirEntry2->DIR_FileSize;
                        // get starting cluster
                        tmpStartCluster = pDirEntry2->DIR_FstClusLO;
                        // compare name
                        char tmpDelByte = tmpFileNmae[0];
                        tmpFileNmae[0] = recoveryNmae[0];
                        if(debugFlag) printf("--find name:[%s]-%x-\n", tmpFileNmae, tmpDelByte);
                        if ((tmpDelByte == (char)0xe5) && (!strcmp(tmpFileNmae, recoveryNmae)))
                        {
                            // with -s
                            if(argc == 6)
                            {
                                // calculate sha1sum
                                unsigned char sha1sumTmp[64] = {0};
                                if(tmpSize == 0)
                                {
                                    strcpy(sha1sumTmp, "da39a3ee5e6b4b0d3255bfef95601890afd80709");
                                }
                                else
                                {
                                    unsigned int tmpContentOffset = getClusterAddrbyNo(tmpStartCluster);
                                    unsigned char tmpContentSum[64] = {0};
                                    (void)SHA1((char *)(p_map + tmpContentOffset), tmpSize, tmpContentSum);
                                    if(debugFlag)
                                    {
                                        printf("--tmpContentOffset [%x]--\n--tmpContentSum:--\n", tmpContentOffset);
                                        for (int x = 0; x < 20; x++)
                                            printf("%02x", tmpContentSum[x]);
                                        printf("\n\n");
                                    }
                                    // transfer
                                    for (int x = 0; x < 20; x++)
                                            sprintf(sha1sumTmp + x*2, "%02x", tmpContentSum[x]);
                                }

                                if(debugFlag) printf("--sha1sumTmp:%s--\n", sha1sumTmp);
                                if(!strcmp(sha1sum, sha1sumTmp))
                                {
                                    if(debugFlag) printf("--find With sha1 found--\n");
                                    findWithsha1Flag = 1;

                                    // update file info to recovery
                                    pDirEntry = pDirEntry2;
                                    size = tmpSize;
                                    startCluster = tmpStartCluster;
                                    break;
                                }
                            }
                            else
                            {
                                findFlag = 1;
                                if(debugFlag) printf("--findFlag2 set to 1--\n");
                            }

                            // without -s
                            if(debugFlag) printf("--multiple candidates found--\n");
                            multipleFlag = 1;
                            // break;
                        }

                        // next
                        tmpOffset += 0x20;
                    }

                    // if file not find, or multiple find, do not recover file
                    if((!findFlag && !findWithsha1Flag) || multipleFlag) break;

                    // update file name
                    // findFlag = 1;
                    if(debugFlag) printf("--start to recover file--size:%d--startCluster:%d-\n", size, startCluster);
                    unsigned int clusterOffset = 0;
                    unsigned int *pUsedInt = NULL;
                    pDirEntry->DIR_Name[0] = recoveryNmae[0];

                    // small file
                    if(size <= clusterSize)
                    {
                        // update fat1
                        clusterOffset = startCluster * 4; // 每个cluster标记占4字节，从0开始
                        pUsedInt = (int *)(p_map + fat1StartAddr + clusterOffset);
                        *pUsedInt = FAT_CLUSTER_FILEEND_START;
                        if(debugFlag) printf("--update fat1--startCluster:%d---fat item offset:%x---\n", startCluster, fat1StartAddr + clusterOffset);
                        // update fat2
                        if(pBootEntry->BPB_NumFATs == 2)
                        {
                            pUsedInt = (int *)(p_map + fat2StartAddr + clusterOffset);
                            *pUsedInt = FAT_CLUSTER_FILEEND_START;
                            if(debugFlag) printf("--update fat2--startCluster:%d---fat item offset:%x---\n", startCluster, fat2StartAddr + clusterOffset);
                        }
                    }
                    // big file
                    else
                    {
                        int clusterNum = 0;
                        if(size % clusterSize == 0)
                            clusterNum = size / clusterSize;
                        else
                            clusterNum = size / clusterSize + 1;
                        if(debugFlag) printf("--update fat1--clusterNum:%d--\n", clusterNum);
                        
                        for (int i = 0; i < clusterNum; i++)
                        {
                            // update fat1
                            clusterOffset = (startCluster + i) * 4; // 每个cluster标记占4字节，从0开始
                            pUsedInt = (int *)(p_map + fat1StartAddr + clusterOffset);
                            if(debugFlag) printf("--update fat1--i:%d--offset:%x--\n", i, fat1StartAddr + clusterOffset);
                            if(i < (clusterNum - 1))
                                *pUsedInt = startCluster + i + 1;
                            else
                                *pUsedInt = FAT_CLUSTER_FILEEND_START;

                            // update fat2
                            if(pBootEntry->BPB_NumFATs == 2)
                            {
                                pUsedInt = (int *)(p_map + fat2StartAddr + clusterOffset);
                                if(debugFlag) printf("--update fat1--i:%d--offset:%x--\n", i, fat1StartAddr + clusterOffset);
                                if(i < (clusterNum - 1))
                                    *pUsedInt = startCluster + i + 1;
                                else
                                    *pUsedInt = FAT_CLUSTER_FILEEND_START;
                            }  
                        }
                    }

                    // exit
                    break;
                }
            }

            // next
            offset += 0x20;
        }

        // display result
        if(multipleFlag)
        {
            printf("%s: multiple candidates found\n", recoveryNmae);
        }
        else if(findFlag || findWithsha1Flag)
        {
            if(findWithsha1Flag) printf("%s: successfully recovered with SHA-1\n", recoveryNmae);
            else printf("%s: successfully recovered\n", recoveryNmae);
        }
        else
        {
            printf("%s: file not found\n", recoveryNmae);
        }

        // release
        close(fd);
        munmap(p_map, file_size);
        return 0;
    }

    // -r  recover file
    else if(strcmp("-R", argv[2]) == 0)
    {
        printf("MILE1.TXT: file not found\n");
    }


    return 0;
}

