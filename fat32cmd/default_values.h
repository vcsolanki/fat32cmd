#pragma once

#include <string>

#define BLACK           30
#define RED             31
#define GREEN           32
#define YELLOW          33
#define BLUE            34
#define MAGENTA         35
#define CYAN            36
#define WHITE           37
#define RESET           0
#define BRIGHT          1
#define UNDERLINE       4
#define INVERSE         7
#define BRIGHTOFF       21
#define UNDERLINEOFF    24
#define INVERSEOFF      27
#define BG(color) (color+10)


enum class operation {
	WRITE = 0,
	BOOT = 1,
	ALIGN = 2,
	LIST = 3,
	REMOVE = 4,
	EXTRACT = 5
};

#define ACTIVE_PARTITION	0x80
#define INACTIVE_PARTITION	0x00

#define PTYPE_UNKNOWN		0x00
#define PTYPE_12FAT			0x01
#define PTYPE_16FAT_SMALL	0x04
#define PTYPE_EXTENDED		0x05
#define PTYPE_16FAT_LARGE	0x06
#define PTYPE_32FAT			0x0B
#define PTYPE_32FAT_LBA		0x0C
#define PTYPE_16FAT_LBA		0x0E
#define PTYPE_EXTENDED_LBA	0x0F

#define MEDIA_HARD_DISK		0xF8
#define MEDIA_FLOPPY_1_4MB	0xF0
#define MEDIA_RAM_DISK		0xFA

#define ATTRIB_READ_ONLY	0x01
#define ATTRIB_HIDDEN		0x02
#define ATTRIB_SYSTEM		0x04
#define ATTIRB_VOLUME_LABEL	0x08
#define ATTRIB_SUBDIR		0x10
#define ATTRIB_ARCHIVE		0x20
#define ATTRIB_LONG_ENTRY	0x0F

#define TIME_HOUR(value)	((value & 0xF800) >> 11)
#define TIME_MINUTE(value)	((value & 0x07E0) >> 5)
#define TIME_SECOND(value)	(value & 0x001F)

#define DATE_YEAR(value)	(1980 + ((value & 0xFE00) >> 9))
#define DATE_MONTH(value)	((value & 0x01E0) >> 5)
#define DATE_DAY(value)		(value & 0x001F)

#define LONG_SEQUENCE_LAST	0x40
#define LONG_SEQUENCE		0x0F

#define DELETED_ROOT_ENTRY	0xE5

#define VFAT_LFN 0x0

#define FAT12_CLUSTER_FREE	0x000
#define FAT12_CLUSTER_BAD	0xFF7
#define FAT12_CLUSTER_LAST	0xFF8

#define FAT16_CLUSTER_FREE	0x0000
#define FAT16_CLUSTER_BAD	0xFFF7
#define FAT16_CLUSTER_LAST	0xFFF8

#define FAT32_CLUSTER_FREE	0x00000000
#define FAT32_CLUSTER_BAD	0xFFFFFFF7
#define FAT32_CLUSTER_LAST	0xFFFFFFF

std::string bytes[6] = {
	" Bytes",
	" KB",
	" MB",
	" GB",
	" TB",
	" PB"
};