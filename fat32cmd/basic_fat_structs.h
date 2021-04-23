#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct CHSC
{
	int cylinder;
	int sector;
};

enum class PType
{
	FAT12 = 0,
	FAT16 = 1,
	FAT32 = 2
};

#pragma pack(push,1)
struct fat32
{
	uint8_t		jumpcode[3];
	uint8_t		OEM[8];
	uint16_t	BytesPerSector;
	uint8_t		SectorPerCluster;
	uint16_t	ReservedSectors;
	uint8_t		NumberOfFat;
	uint16_t	RootEntries;
	uint16_t	NumberOfSectorInFS; //NA for fat32
	uint8_t		MediaDesc;
	uint16_t	SectorPerFatOld; //NA for fat32
	uint16_t	SectorPerTrack;
	uint16_t	NumberOfHeads;
	uint32_t	HiddenSectors;
	uint32_t	SectorInPartition;
	uint32_t	SectorPerFat;
	uint16_t	Flags;
	uint16_t	FileSystemVersion;
	uint32_t	FirstClusterOfRootDirectory;
	uint16_t	FileSystemSector;
	uint16_t	BackupBootSector;
	uint8_t		Reserved[12];
	uint8_t		LogicalDriveNumber;
	uint8_t		Unused;
	uint8_t		ExtendedSignature;
	uint32_t	SerialNumber;
	uint8_t		VolumeLabel[11];
	uint8_t		FileSystem[8];
};
#pragma pack(pop)

#pragma pack(push,1)
struct fat16
{
	uint8_t		jumpcode[3];
	uint8_t		OEM[8];
	uint16_t	BytesPerSector;
	uint8_t		SectorPerCluster;
	uint16_t	ReservedSectors;
	uint8_t		NumberOfFat;
	uint16_t	RootEntries;
	uint16_t	NumberOfSectorInFS;
	uint8_t		MediaDesc;
	uint16_t	SectorPerFat;
	uint16_t	SectorPerTrack;
	uint16_t	NumberOfHeads;
	uint32_t	HiddenSectors;
	uint32_t	NumberOfSectorInFSBackup;
	uint8_t		LogicalDriveNumber;
	uint8_t		Reserved;
	uint8_t		ExtendedSignature;
	uint32_t	SerialNumber;
	uint8_t		VolumeLabel[11];
	uint8_t		FileSystem[8];
};
#pragma pack(pop)

struct PartitionInfo
{
	uint32_t ClusterSize;
	uint32_t BytesPerSector;
	uint32_t FatTableSector;
	uint32_t RootEntries;
	uint32_t RootTableSector;
	uint32_t RootTableCluster;
	uint32_t ClusterEntrySize;
	PType Type;
};

#pragma pack(push,1)
struct LongEntry
{
	uint8_t Sequence;
	uint8_t Name1[10];
	uint8_t Attribute;
	uint8_t Type;
	uint8_t Checksum;
	uint8_t Name2[12];
	uint16_t FirstCluster;
	uint8_t Name3[4];
};
#pragma pack(pop)

#pragma pack(push,1)
struct RootEntry
{
	uint8_t FileName[8];
	uint8_t FileExtension[3];
	uint8_t Attribute;
	uint8_t Reserved;
	uint8_t CreationMillisecondStamp;
	uint16_t CreationTime;
	uint16_t CreationDate;
	uint16_t LastAccessedDate;
	uint16_t HighFirstCluster;
	uint16_t LastWriteTime;
	uint16_t LastWriteDate;
	uint16_t LowFirstCluster;
	uint32_t FileSize;
};
#pragma pack(pop)

#pragma pack(push,1)
struct partition
{
	uint8_t		CurrentState;
	uint8_t		BeginHead;
	uint16_t	BeginCylinderSector;
	uint8_t		Type;
	uint8_t		EndHead;
	uint16_t	EndCylinderSector;
	uint32_t	SectorsBetween;
	uint32_t	NumberOfSectors;
};
#pragma pack(pop)

#pragma pack(push,1)
struct mbr
{
	struct partition partition_entry[4];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct FSInfo
{
	uint32_t	Sign1;
	uint8_t		Reserved1[480];
	uint32_t	Sing2;
	uint32_t	FreeClusters;
	uint32_t	NextFreeCluster;
	uint8_t		Reserved2[12];
	uint32_t	SectorSign;
};
#pragma pack(pop)

struct FileList
{
	std::string name;
	RootEntry entry;
	std::vector<FileList> sublist;
};