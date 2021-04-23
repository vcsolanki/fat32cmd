#define WIN32_LEAN_AND_MEAN
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdarg>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include "default_values.h"
#include "basic_fat_structs.h"
#include <Windows.h>
#undef max
#undef min

using namespace std;

struct mbr mbr_info;
struct fat32 fat32_part[4];
struct fat16 fat16_part[4];
struct PartitionInfo part[4];
uint8_t p = 0;

string write_type;
string write_file;
string image_file;
string image_type;
string list_path;
string remove_file;
string extract_type;
string extract_file;

struct CHSC GetCHSC(uint16_t value)
{
	struct CHSC chsc;
	chsc.sector = value & 0x3F;
	int ch = (value & 0xC0) << 2;
	int cl = (value & 0xFF00) >> 8;
	chsc.cylinder = ch & cl;
	return chsc;
}

string c(char c)
{
	string s = "\033[";
	s.append(to_string(c));
	s.append("m");
	return s;
}

string ls(char* src, int n)
{
	string s;
	for (int i = 0; i < n; i++)
		s += src[i];
	return s;
}

string f(string fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	stringstream ss(fmt);
	string out;
	char c;
	uint16_t n = 0;
	while (!ss.eof())
	{
		ss >> n;
		string s = string(va_arg(args, const char*));
		for (size_t i = s.size(); i <= n; i++)
			s.append(" ");
		out.append(s);
		if (!ss.eof()) break;
		ss >> c;
	}
	va_end(args);
	return out;
}

string get_date(uint16_t value)
{
	int day = DATE_DAY(value);
	int mon = DATE_MONTH(value);
	int yer = DATE_YEAR(value);
	string d;
	stringstream ss(d);
	ss << setfill('0') << setw(2) << day << "-";
	ss << setfill('0') << setw(2) << mon << "-";
	ss << setfill('0') << setw(4) << yer;
	return ss.str();
}

string get_time(uint16_t value)
{
	int hour = TIME_HOUR(value);
	int min = TIME_MINUTE(value);
	int sec = TIME_SECOND(value);
	string s;
	stringstream ss(s);
	ss << setfill('0') << setw(2) << hour << ":";
	ss << setfill('0') << setw(2) << min << ":";
	ss << setfill('0') << setw(2) << sec;
	return ss.str();
}

string get_size(uint32_t value)
{
	short j = 0;
	while (value > 1024)
	{
		value = value / 1024;
		j++;
	}
	string s;
	s.append(to_string(value));
	s.append(bytes[j]);
	return s;
}

string get_attrib_type(uint8_t attrib)
{
	string s;
	if (attrib & ATTRIB_READ_ONLY) s.append("ReadOnly ");
	if (attrib & ATTRIB_ARCHIVE) s.append("Archive ");
	if (attrib & ATTRIB_SUBDIR) s.append("Dir ");
	if (attrib & ATTRIB_SYSTEM) s.append("System ");
	if (attrib & ATTRIB_HIDDEN) s.append("Hidden ");
	return s;
}

vector<string> split_path(string str)
{
	vector<string> spt;
	size_t start = 0;
	size_t end = str.find("/");
	if (end == -1) end = str.find("\\");
	while (end != -1)
	{
		string s = str.substr(start, end - start);
		if(s.size() > 0) spt.push_back(s);
		start = end + 1;
		end = str.find("/", start);
		if (end == -1) end = str.find("\\", start);
	}
	spt.push_back(str.substr(start, end - start));
	return spt;
}

vector<pair<string,RootEntry>> get_entries(FILE* rootd)
{
	vector<pair<string, RootEntry>> flist;
	uint32_t read_address = 0;
	fseek(rootd, 0, SEEK_SET);
	RootEntry entry;
	entry.FileName[0] = 0xFF;
	string name;
	for(;;)
	{
		fseek(rootd, read_address, SEEK_SET);
		fread(&entry, 1, 32, rootd);
		read_address += 32;
		if (entry.FileName[0] == 0x00) break;
		if (entry.FileName[0] == DELETED_ROOT_ENTRY) continue;
		if (entry.Attribute & ATTRIB_LONG_ENTRY) continue;
		if (entry.FileName[0] == '.' || entry.FileName[1] == '.') continue;
		if (entry.FileName[0] == 0x05) entry.FileName[0] = 0xE5;
		struct LongEntry longentry;
		string name;
		for (int j = 2;; j++)
		{
			fseek(rootd, read_address - (32 * j), SEEK_SET);
			fread(&longentry, 1, sizeof(longentry), rootd);
			for (int k = 0; k < 10; k = k + 2) name += longentry.Name1[k];
			for (int k = 0; k < 12; k = k + 2) name += longentry.Name2[k];
			for (int k = 0; k < 4; k = k + 2) name += longentry.Name3[k];
			if (longentry.Sequence & LONG_SEQUENCE_LAST)
				break;
		}
		flist.emplace_back(name, entry);
	}
	return flist;
}

FILE* read_fat32_rootdir(FILE* img, long int cluster = 0) //only used for FAT32 image.
{
	FILE* rootd = fopen("dir.fat32cmd", "wb+");
	uint32_t read_cluster = (part[p].Type == PType::FAT32) ? part[p].RootTableCluster : 2;
	if (cluster != 0)
		read_cluster = cluster;
	uint32_t read_address;
	uint8_t* buf = (uint8_t*)malloc(part[p].BytesPerSector);
	do
	{
		read_address = (part[p].RootTableSector * part[p].BytesPerSector) + ((read_cluster - 2) * part[p].ClusterSize) * part[p].BytesPerSector;
		fseek(img, read_address, SEEK_SET);
		fread(buf, 1, part[p].BytesPerSector, img);
		fwrite(buf, 1, part[p].BytesPerSector, rootd);
		fseek(img, (part[p].FatTableSector * part[p].BytesPerSector) + (read_cluster * part[p].ClusterEntrySize), SEEK_SET);
		fread(&read_cluster, 1, part[p].ClusterEntrySize, img);
	} 
	while (read_cluster < FAT32_CLUSTER_LAST);
	fseek(rootd, 0, SEEK_SET);
	return rootd;
}

FILE* read_fat16_rootdir(FILE* img, long int cluster = 0)
{
	FILE* rootd = fopen("dir.fat32cmd", "wb+");
	uint32_t read_address = part[p].RootTableSector * part[p].BytesPerSector;
	if (cluster == 0)
	{
		fseek(img, read_address, SEEK_SET);
		uint8_t* buf = (uint8_t*)malloc(part[p].RootEntries * 32);
		fread(buf, 1, part[p].RootEntries * 32, img);
		fwrite(buf, 1, part[p].RootEntries * 32, rootd);
	}
	else
	{
		long int cluster_last = FAT16_CLUSTER_LAST;
		int cluster_entry = part[p].ClusterEntrySize;
		long int fat_table = part[p].FatTableSector * part[p].BytesPerSector;
		uint8_t* buf = (uint8_t*)malloc(part[p].ClusterSize * part[p].BytesPerSector);
		if (part[p].Type == PType::FAT12) cluster_last = FAT12_CLUSTER_LAST;
		do {
			fseek(img, read_address + ((cluster - 2) * part[p].ClusterSize) * part[p].BytesPerSector + part[p].RootEntries * 32, SEEK_SET);
			fread(buf, 1, part[p].ClusterSize * part[p].BytesPerSector, img);
			fwrite(buf, 1, part[p].ClusterSize * part[p].BytesPerSector, rootd);
			if (cluster_entry != 1)
			{
				fseek(img, fat_table + (cluster * cluster_entry), SEEK_SET);
				fread(&cluster, 1, cluster_entry, img);
			}
			else
			{
				int acc = (cluster * 12) / 8;
				acc += fat_table;
				fseek(img, acc, 0);
				if (cluster % 2 == 0)
				{
					cluster = 0;
					fread(&cluster, 1, 2, img);
					cluster = (cluster & 0x0FFF);
				}
				else
				{
					cluster = 0;
					fread(&cluster, 1, 2, img);
					cluster = (cluster & 0xFFF0) >> 4;
				}
			}
		} while (cluster < cluster_last);
	}
	fseek(rootd, 0, SEEK_SET);
	return rootd;
}

vector<FileList> prepare_flist(FILE* img, int cluster = 0)
{
	vector<FileList> flist;
	FileList file;
	FILE* rootd = (part[p].Type == PType::FAT32) ? read_fat32_rootdir(img, cluster) : read_fat16_rootdir(img, cluster);
	vector<pair<string, RootEntry>> list = get_entries(rootd);
	for (auto e : list)
	{
		file.name = e.first;
		file.entry = e.second;
		if (e.second.Attribute == ATTRIB_SUBDIR)
		{
			file.sublist = prepare_flist(img, (e.second.HighFirstCluster << 16 | e.second.LowFirstCluster));
		}
		flist.push_back(file);
	}
	fclose(rootd);
	remove("dir.fat32cmd");
	return flist;
}

bool file_exist(string name)
{
	ifstream f(name.c_str());
	return f.good();
}

bool check_image(string image)
{
	memset(&part, 0, sizeof(PartitionInfo) * 4);
	FILE* img = fopen(image.c_str(), "rb");
	fseek(img, 0x1be, 0);
	fread(&mbr_info, 1, sizeof(mbr_info), img);
	for (int i = 0; i < 4; i++)
	{
		if (mbr_info.partition_entry[i].CurrentState == ACTIVE_PARTITION)
		{
			fseek(img, mbr_info.partition_entry[i].SectorsBetween * 512, 0);
			if (mbr_info.partition_entry[i].Type == PTYPE_32FAT ||
				mbr_info.partition_entry[i].Type == PTYPE_32FAT_LBA)
			{
				struct fat32 info;
				fread(&info, 1, sizeof(info), img);
				fat32_part[i] = info;
				part[i].BytesPerSector = info.BytesPerSector;
				part[i].ClusterSize = info.SectorPerCluster;
				part[i].FatTableSector = mbr_info.partition_entry[i].SectorsBetween + info.ReservedSectors;
				part[i].RootEntries = info.RootEntries;
				part[i].RootTableCluster = info.FirstClusterOfRootDirectory;
				part[i].RootTableSector = mbr_info.partition_entry[i].SectorsBetween + info.ReservedSectors + (info.NumberOfFat * info.SectorPerFat);
				part[i].ClusterEntrySize = 4;
				part[i].Type = PType::FAT32;
			}
			else if (mbr_info.partition_entry[i].Type == PTYPE_16FAT_LARGE ||
				mbr_info.partition_entry[i].Type == PTYPE_16FAT_SMALL ||
				mbr_info.partition_entry[i].Type == PTYPE_16FAT_LBA)
			{
				struct fat16 info;
				fread(&info, 1, sizeof(info), img);
				fat16_part[i] = info;
				part[i].BytesPerSector = info.BytesPerSector;
				part[i].ClusterSize = info.SectorPerCluster;
				part[i].FatTableSector = mbr_info.partition_entry[i].SectorsBetween + info.ReservedSectors;
				part[i].RootEntries = info.RootEntries;
				part[i].RootTableCluster = 0;
				part[i].RootTableSector = mbr_info.partition_entry[i].SectorsBetween + info.ReservedSectors + (info.NumberOfFat * info.SectorPerFat);
				part[i].ClusterEntrySize = 2;
				part[i].Type = PType::FAT16;
			}
			else if (mbr_info.partition_entry[i].Type == PTYPE_12FAT)
			{
				struct fat16 info;
				fread(&info, 1, sizeof(info), img);
				fat16_part[i] = info;
				part[i].BytesPerSector = info.BytesPerSector;
				part[i].ClusterSize = info.SectorPerCluster;
				part[i].FatTableSector = mbr_info.partition_entry[i].SectorsBetween + info.ReservedSectors;
				part[i].RootEntries = info.RootEntries;
				part[i].RootTableCluster = 0;
				part[i].RootTableSector = mbr_info.partition_entry[i].SectorsBetween + info.ReservedSectors + (info.NumberOfFat * info.SectorPerFat);
				part[i].ClusterEntrySize = 1;
				part[i].Type = PType::FAT12;
			}
		}
	}
	fclose(img);
	if (part[0].BytesPerSector == 0 &&
		part[1].BytesPerSector == 0 &&
		part[2].BytesPerSector == 0 &&
		part[3].BytesPerSector == 0)
		return false;
	return true;
}

void writetoimage(string src, string dest, string type)
{
	if (!file_exist(dest))
	{
		cout << c(BRIGHT) << c(RED);
		cout << "Image dont exist : " << c(WHITE) << dest;
		cout << c(RESET) << endl;
		return;
	}
	if (!file_exist(src))
	{
		cout << c(BRIGHT) << c(RED);
		cout << "File dont exist : " << c(WHITE) << src;
		cout << c(RESET) << endl;
		return;
	}
	if (!check_image(dest))
	{
		cout << c(BRIGHT) << c(RED);
		cout << "Unable to recognize FAT image. Make sure its FAT12, FAT16 or FAT32 Image." << endl;
		cout << c(RESET);
		return;
	}
	if (type._Equal("boot"))
	{
		try
		{
			FILE* img = fopen(dest.c_str(), "rb+");
			FILE* btf = fopen(src.c_str(), "r");
			fseek(btf, 0, SEEK_END);
			long int size = ftell(btf);
			if (size < 512)
				throw exception("Size must be 512 bytes for boot sector!");
			fseek(btf, 0, 0);
			uint8_t* buf = (uint8_t*)malloc(512);
			fread(buf, 1, 512, btf);
			fseek(img, mbr_info.partition_entry[p].SectorsBetween * 512, 0);
			fwrite(buf, 1, 512, img);
			cout << c(BRIGHT);
			if (size > ftell(btf))
			{
				cout << c(YELLOW);
				cout << "Extra bytes ignored!" << endl;
			}
			cout << c(GREEN);
			cout << "Boot Sector writing done!" << endl;
			cout << c(RESET);
			fclose(img);
			fclose(btf);
		}
		catch (exception e)
		{
			cout << c(BRIGHT);
			cout << c(WHITE) << "Exception :" << c(RED) << e.what() << endl;
			cout << c(RESET);
		}
	}
	else if (type._Equal("file"))
	{
	}
	else
	{
		cout << c(RED) << c(BRIGHT);
		cout << "Runtime Error: " << c(WHITE) << "Unknown Type for Write operation, aborting!" << endl;
		cout << c(RESET);
	}
}

void bootimage(string image)
{
	if (!file_exist(image))
	{
		cout << c(BRIGHT) << c(RED);
		cout << "Image dont exist : " << c(WHITE) << image;
		cout << c(RESET) << endl;
		return;
	}
	if (check_image(image))
	{
		
		if (part[p].Type == PType::FAT32)
		{
			cout << c(BRIGHT);
			cout << c(WHITE) << f("20", "OEM Name") << ": " << c(YELLOW) << ls((char*)fat32_part[p].OEM, 8) << endl;
			cout << c(WHITE) << f("20", "Volume Name") << ": " << c(YELLOW) << ls((char*)fat32_part[p].VolumeLabel, 8) << endl;
			cout << c(WHITE) << f("20", "Type") << ": " << c(YELLOW) << ls((char*)fat32_part[p].FileSystem, 8) << endl;
			cout << c(WHITE) << f("20", "Sectors") << ": " << c(YELLOW) << fat32_part[p].SectorInPartition << endl;
			cout << c(WHITE) << f("20", "Bytes Per Sector") << ": " << c(YELLOW) << fat32_part[p].BytesPerSector << endl;
			cout << c(WHITE) << f("20", "Sectors Per Track") << ": " << c(YELLOW) << fat32_part[p].SectorPerTrack << endl;
			cout << c(WHITE) << f("20", "Heads") << ": " << c(YELLOW) << fat32_part[p].NumberOfHeads << endl;
			cout << c(WHITE) << f("20", "Hidden Sectors") << ": " << c(YELLOW) << fat32_part[p].HiddenSectors << endl;
			cout << c(WHITE) << f("20", "FAT Sectors") << ": " << c(YELLOW) << fat32_part[p].SectorPerFat << endl;
			cout << c(WHITE) << f("20", "FAT Count") << ": " << c(YELLOW) << +fat32_part[p].NumberOfFat << endl;
			cout << c(WHITE) << f("20", "Reserved Sectors") << ": " << c(YELLOW) << +fat32_part[p].ReservedSectors << endl;
			cout << c(WHITE) << f("20", "Sectors Per Cluster") << ": " << c(YELLOW) << +fat32_part[p].SectorPerCluster << endl;
			cout << c(WHITE) << f("20", "Root Entries") << ": " << c(YELLOW) << +fat32_part[p].RootEntries << endl;
			cout << c(RESET);
		}
		else if (part[p].Type == PType::FAT12 || part[p].Type == PType::FAT16)
		{
			cout << c(BRIGHT);
			cout << c(WHITE) << f("20", "OEM Name") << ": " << c(YELLOW) << ls((char*)fat16_part[p].OEM, 8) << endl;
			cout << c(WHITE) << f("20", "Volume Name") << ": " << c(YELLOW) << ls((char*)fat16_part[p].VolumeLabel, 8) << endl;
			cout << c(WHITE) << f("20", "Type") << ": " << c(YELLOW) << ls((char*)fat16_part[p].FileSystem, 8) << endl;
			cout << c(WHITE) << f("20", "Sectors") << ": " << c(YELLOW) << fat16_part[p].NumberOfSectorInFS << endl;
			cout << c(WHITE) << f("20", "Bytes Per Sector") << ": " << c(YELLOW) << fat16_part[p].BytesPerSector << endl;
			cout << c(WHITE) << f("20", "Sectors Per Track") << ": " << c(YELLOW) << fat16_part[p].SectorPerTrack << endl;
			cout << c(WHITE) << f("20", "Heads") << ": " << c(YELLOW) << fat16_part[p].NumberOfHeads << endl;
			cout << c(WHITE) << f("20", "Hidden Sectors") << ": " << c(YELLOW) << fat16_part[p].HiddenSectors << endl;
			cout << c(WHITE) << f("20", "FAT Sectors") << ": " << c(YELLOW) << fat16_part[p].SectorPerFat << endl;
			cout << c(WHITE) << f("20", "FAT Count") << ": " << c(YELLOW) << +fat16_part[p].NumberOfFat << endl;
			cout << c(WHITE) << f("20", "Reserved Sectors") << ": " << c(YELLOW) << +fat16_part[p].ReservedSectors << endl;
			cout << c(WHITE) << f("20", "Sectors Per Cluster") << ": " << c(YELLOW) << +fat16_part[p].SectorPerCluster << endl;
			cout << c(WHITE) << f("20", "Root Entries") << ": " << c(YELLOW) << +fat16_part[p].RootEntries << endl;
			cout << c(RESET);
		}
	}
	else
	{
		cout << c(BRIGHT) << c(RED);
		cout << "Unable to recognize FAT image. Make sure its FAT12, FAT16 or FAT32 Image." << endl;
		cout << c(RESET);
		return;
	}
}

void alignimage(string image)
{
	if (!file_exist(image))
	{
		cout << c(BRIGHT) << c(RED);
		cout << "Image don't exist : " << c(WHITE) << image;
		cout << c(RESET) << endl;
		return;
	}
	FILE* img = fopen(image.c_str(), "rb+");
	fseek(img, 0, SEEK_END);
	long int szf = ftell(img);
	long int aligned = szf + (512 - szf % 512);
	int mis = 512 - szf % 512;
	cout << c(BRIGHT) << c(WHITE);
	cout << f("20", "Image Original size") << c(YELLOW) << ": " << szf << " Bytes" << endl;
	cout << c(WHITE);
	if (szf % 512 != 0)
	{
		cout << f("20", "Image Aligned size") << c(YELLOW) << ": " << aligned << " Bytes" << endl;
		char buf[512];
		memset(&buf, 0, mis);
		size_t res = fwrite(&buf, 1, mis, img);
		cout << c(GREEN) << "Image writing done!" << endl;
	}
	else
		cout << c(YELLOW) << "Image is already aligned!";
	cout << c(RESET);
	fclose(img);
}

void listimage(string image)
{
	if (!file_exist(image))
	{
		cout << c(BRIGHT) << c(RED);
		cout << "Image don't exist : " << c(WHITE) << image;
		cout << c(RESET) << endl;
		return;
	}
	if (check_image(image))
	{
		vector<string> path = split_path(list_path);
		long int root_directory = part[p].RootTableSector * part[p].BytesPerSector;
		FILE* img = fopen(image.c_str(), "r");
		vector<FileList> files = prepare_flist(img, part[p].RootTableCluster);
		if(!list_path._Equal("*"))
		for (size_t i = 0; i < path.size(); i++)
		{
			bool found = false;
			for (size_t j = 0; j < files.size(); j++)
			{
				if (strncmp(files[j].name.c_str(), path[i].c_str(), path[i].size()) == 0)
				{
					if (files[j].entry.Attribute & ATTRIB_SUBDIR)
					{
						vector<FileList> temp = files[j].sublist;
						files = temp;
					}
				}
			}
		}
		cout << c(BRIGHT) << c(YELLOW);
		cout << f("26", "Name") << f("11", "Size") << f("20", "Creation Date") << "Type" << endl << endl;
		cout << c(WHITE);
		for (auto fl : files)
		{
			cout << f("26", fl.name.c_str()) << f("11", get_size(fl.entry.FileSize).c_str()) << f("20", get_date(fl.entry.CreationDate).c_str()) << get_attrib_type(fl.entry.Attribute) << endl;
		}
		fclose(img);
		if (files.size() == 0)
		{
			cout << c(BRIGHT) << c(YELLOW);
			cout << "There is no files in this Image file, its empty!";
			cout << c(RESET);
			return;
		}
		cout << c(RESET);
	}
}

void removefromimage(string remove_file, string image)
{
	if (!file_exist(image))
	{
		cout << c(BRIGHT) << c(RED);
		cout << "Image dont exist : " << c(WHITE) << image;
		cout << c(RESET) << endl;
		return;
	}
	if (check_image(image))
	{
	}
}

void extractfromimageg(string image)
{
	if (!file_exist(image))
	{
		cout << c(BRIGHT) << c(RED);
		cout << "Image don't exist : " << c(WHITE) << image;
		cout << c(RESET) << endl;
		return;
	}
	if (check_image(image))
	{
		if (extract_type._Equal("file"))
		{
			vector<string> path = split_path(extract_file);
			long root_directory = part[p].RootTableSector * part[p].BytesPerSector;
			FILE* img = fopen(image.c_str(), "rb");
			vector<FileList> files = prepare_flist(img);
			vector<FileList> srch = files;
			bool file_found = false;
			for (size_t i = 0; i < path.size(); i++)
			{
				for (size_t j = 0; j < files.size(); j++)
				{
					if (strncmp(files[j].name.c_str(), path[i].c_str(), path[i].size()) == 0)
					{
						if (files[j].entry.Attribute & ATTRIB_SUBDIR)
						{
							vector<FileList> temp = files[j].sublist;
							files = temp;
						}
					}
				}
			}
			struct RootEntry rootentry;
			rootentry.FileSize = -1;
			for (auto f : files)
			{
				if (strncmp(f.name.c_str(), path[path.size()-1].c_str(), path[path.size()-1].size()) == 0)
					rootentry = f.entry;
			}
			if (rootentry.FileSize == -1)
			{
				cout << c(BRIGHT) << c(YELLOW);
				cout << "There is no file named " << extract_file << " in this FAT Image." << endl;
				cout << "Try to list the folder, to get exact path!" << endl;
				cout << c(RESET);
			}
			else
			{
				uint32_t cluster_last, cluster_entry;
				if (part[p].Type == PType::FAT32)
				{	
					cluster_last = FAT32_CLUSTER_LAST;
					cluster_entry = 4;
				}
				else if (part[p].Type == PType::FAT12)
				{
					cluster_last = FAT12_CLUSTER_LAST;
					cluster_entry = 1;
				}
				else if (part[p].Type == PType::FAT16)
				{
					cluster_last = FAT16_CLUSTER_LAST;
					cluster_entry = 2;
				}
				FILE* extr = fopen(path[path.size()-1].c_str(), "wb");
				long fat_table = part[p].FatTableSector * part[p].BytesPerSector;
				uint32_t cluster = rootentry.LowFirstCluster;
				if (part[p].Type == PType::FAT32)
					cluster = (rootentry.HighFirstCluster << 16) | rootentry.LowFirstCluster;
				uint32_t size = 0;
				uint32_t read_size = 0;
				uint8_t* buf = (uint8_t*)malloc(part[p].ClusterSize * part[p].BytesPerSector);
				do
				{
					read_size = part[p].ClusterSize * part[p].BytesPerSector;
					size += read_size;
					if (read_size > rootentry.FileSize)
						read_size = rootentry.FileSize;
					else if (size > rootentry.FileSize)
					{
						size = size - read_size;
						read_size = rootentry.FileSize - size;
					}
					fseek(img, root_directory + (part[p].BytesPerSector * (part[p].ClusterSize * (cluster - 2)) + (part[p].RootEntries * 32)), SEEK_SET);
					fread(buf, 1, read_size, img);
					fwrite(buf, 1, read_size, extr);
					if (cluster_entry != 1)
					{
						fseek(img, fat_table + (cluster * cluster_entry), SEEK_SET);
						fread(&cluster, 1, cluster_entry, img);
					}
					else
					{
						int acc = (cluster * 12) / 8;
						acc += fat_table;
						fseek(img, acc, 0);
						if (cluster % 2 == 0)
						{
							cluster = 0;
							fread(&cluster, 1, 2, img);
							cluster = (cluster & 0x0FFF);
						}
						else
						{
							cluster = 0;
							fread(&cluster, 1, 2, img);
							cluster = (cluster & 0xFFF0) >> 4;
						}
					}
				} while (cluster < cluster_last);
				fclose(extr);
				cout << c(BRIGHT) << c(GREEN);
				cout << "File Extraction Done!";
				cout << c(RESET);
			}
			fclose(img);
		}
		else if (extract_type._Equal("boot"))
		{
			FILE* boot = fopen(extract_file.c_str(), "wb");
			FILE* img = fopen(image.c_str(), "rb");
			char sector[512];
			fseek(img, mbr_info.partition_entry[0].SectorsBetween * 512, 0);
			fread(&sector, 1, 512, img);
			fwrite(&sector, 1, 512, boot);
			cout << c(BRIGHT) << c(GREEN);
			cout << "Boot Extraction Done!";
			cout << c(RESET);
			fclose(img);
			fclose(boot);
		}
	}
}

int main(unsigned int argc, char** args)
{
	DWORD mode;
	GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode);
	if (!(mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING))
	{
		mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), mode);
	}

	vector<operation> task;
	vector<string> list;
	bool all_good = true;
#ifndef _DEBUG
	if (argc < 2)
	{
		cout << c(YELLOW) << c(BRIGHT);
		cout << "Type --help for more information." << endl;
		cout << c(RESET);
	}
	else
	{
#endif // DEBUG
#ifdef _DEBUG
		string s = "-l /folder_xyz -i ../Release/16hdd.img";
		stringstream ss(s);
		string temp = "";
		while (!ss.eof())
		{
			ss >> temp;
			list.push_back(temp);
		}
#else
		for (unsigned int i = 1; i < argc; i++)
		{
			list.push_back(string(args[i]));
		}
#endif //DEBUG
		if (list[0]._Equal("--help") || list[0]._Equal("-h"))
		{
			cout << c(BRIGHT) << c(GREEN);
			cout << "Fat32CMD - 0.1a - " << __DATE__ << " - Vishal Solanki" << endl << endl;
			cout << c(WHITE);
			cout << f("10", "Syntax:") << c(YELLOW) << "fat32cmd [command] [options] [image]" << endl;
			cout << c(WHITE);
			cout << endl << f("10", "") << "Type " << c(YELLOW) << "[command] --help" << c(WHITE) << " to get more information." << endl << endl;
			cout << f("10", "Command:") << c(YELLOW) << f("15", "-h, --help") << c(WHITE) << "Show's help for Commands." << endl;
			cout << f("10", "") << c(YELLOW) << f("15", "-w, --write") << c(WHITE) << "Write to FAT Image." << endl;
			cout << f("10", "") << c(YELLOW) << f("15", "-i, --image") << c(WHITE) << "Indicates FAT Image file." << endl;
			cout << f("10", "") << c(YELLOW) << f("15", "-b, --boot") << c(WHITE) << "Information about FAT Image." << endl;
			cout << f("10", "") << c(YELLOW) << f("15", "-a, --align") << c(WHITE) << "Align the FAT Image." << endl;
			cout << f("10", "") << c(YELLOW) << f("15", "-l, --list") << c(WHITE) << "List files and folders from FAT Image." << endl;
			cout << f("10", "") << c(YELLOW) << f("15", "-e, --extract") << c(WHITE) << "Extract file or boot from FAT Image." << endl;
			cout << f("10", "") << c(YELLOW) << f("15", "-r, --remove") << c(WHITE) << "Remove file from FAT Image." << endl << endl;

			cout << c(WHITE) << f("10", "Example:") << c(YELLOW) << "fat32cmd -w boot boot.bin -i hdd.img" << endl;
			cout << f("10", "") << c(YELLOW) << "fat32cmd --align --image hdd.img" << endl;
			cout << f("10", "") << c(YELLOW) << "fat32cmd -b -i hdd.img" << endl;

			cout << c(RESET);
		}
		else
		{
			for (unsigned int i = 0; i < list.size(); i++)
			{
				if (list[i]._Equal("-w") || list[i]._Equal("--write"))
				{
					if (i < list.size() - 1) i++; else goto nocommand;
					if (list[i]._Equal("--help") || list[i]._Equal("-h"))
					{
						cout << c(WHITE) << c(BRIGHT);
						cout << f("10", "Syntax:") << c(YELLOW) << list[i - 1] << " [option] [source]" << endl << endl;
						cout << c(WHITE);
						cout << f("10", "Option:") << c(YELLOW) << f("10", "boot") << c(WHITE) << "Write source to the first sector of FAT Image." << endl;
						cout << f("10", "") << c(YELLOW) << f("10", "file") << c(WHITE) << "Write the source as a file in FAT Image." << endl << endl;
						cout << f("10", "Source:") << c(WHITE) << f("10", "") << "Source can be plain or binary file." << endl << endl;
						cout << f("10", "Info:") << c(WHITE) << f("10", "") << "This command is used to write boot sector or any file to the FAT Image." << endl;
						cout << c(RESET);
					}
					else if (list[i]._Equal("boot") || list[i]._Equal("file"))
					{
						write_type = list[i];
						if (i < list.size() - 1) i++; else goto nocommand;
						write_file = list[i];
						task.push_back(operation::WRITE);
					}
					else
					{
						goto arg_error;
					}
				}
				else if (list[i]._Equal("-i") || list[i]._Equal("--image"))
				{
					if (i < list.size() - 1) i++; else goto nocommand;
					if (list[i]._Equal("-h") || list[i]._Equal("--help"))
					{
						cout << c(WHITE) << c(BRIGHT);
						cout << f("10", "Syntax:") << c(YELLOW) << list[i - 1] << " [source]" << endl << endl;
						cout << c(WHITE);
						cout << f("10", "Source:") << "Source is FAT12, FAT16 or FAT32 Image file." << endl;
						cout << f("10", "Info:") << "This is used to distinguish the Image file." << endl;
						cout << c(RESET);
					}
					else
						image_file = list[i];
				}
				else if (list[i]._Equal("-b") || list[i]._Equal("--boot"))
				{
					if (i < list.size() - 1) i++; else goto nocommand;
					if (list[i]._Equal("-h") || list[i]._Equal("--help"))
					{
						cout << c(WHITE) << c(BRIGHT);
						cout << f("10", "Syntax:") << c(YELLOW) << list[i - 1] << endl << endl;
						cout << c(WHITE);
						cout << f("10", "Note:") << "This is standalone command, no parameters." << endl << endl;
						cout << f("10", "Info:") << "This command list the information about the FAT Image, What Volume Label, Type, Sectors, Heads etc." << endl;
						cout << c(RESET);
					}
					else
						task.push_back(operation::BOOT);
					i--;
				}
				else if (list[i]._Equal("-a") || list[i]._Equal("--align"))
				{
					if (i < list.size() - 1) i++; else goto nocommand;
					if (list[i]._Equal("-h") || list[i]._Equal("--help"))
					{
						cout << c(WHITE) << c(BRIGHT);
						cout << f("10", "Syntax:") << c(YELLOW) << list[i - 1] << endl << endl;
						cout << c(WHITE);
						cout << f("10", "Note:") << "This is standalone command, no parameters." << endl << endl;
						cout << f("10", "Info:") << "This command aligns the bytes at the end of FAT Image file." << endl;
						cout << c(RESET);
					}
					else
						task.push_back(operation::ALIGN);
					i--;
				}
				else if (list[i]._Equal("-l") || list[i]._Equal("--list"))
				{
					if (i < list.size() - 1) i++; else goto nocommand;
					if (list[i]._Equal("-h") || list[i]._Equal("--help"))
					{
						cout << c(WHITE) << c(BRIGHT);
						cout << f("10", "Syntax:") << c(YELLOW) << list[i - 1] << "[path]" << endl << endl;
						cout << c(WHITE);
						cout << f("10", "Path:") << "Path indicates the location in FAT Image, you can use \"*\" to list all files from root." << endl << endl;
						cout << f("10", "Info:") << "This command list all the files available in the FAT Image file." << endl;
						cout << c(RESET);
						continue;
					}
					list_path = list[i];
					task.push_back(operation::LIST);
					if (i < list.size() - 1) i++; else goto nocommand;
					i--;
				}
				else if (list[i]._Equal("-r") || list[i]._Equal("--remove"))
				{
					if (i < list.size() - 1) i++; else goto nocommand;
					if (list[i]._Equal("-h") || list[i]._Equal("--help"))
					{
						cout << c(WHITE) << c(BRIGHT);
						cout << f("10", "Syntax:") << c(YELLOW) << list[i - 1] << " [filename]" << endl << endl;
						cout << c(WHITE);
						cout << f("10", "Filename:") << "This string is used for searching file in the FAT Image file." << endl << endl;
						cout << f("10", "Info:") << "This command removes the file from FAT Image file." << endl;
						cout << c(RESET);
					}
					else
					{
						if (i < list.size() - 1) i++; else goto nocommand;
						remove_file = list[i];
						task.push_back(operation::REMOVE);
					}
				}
				else if (list[i]._Equal("-e") || list[i]._Equal("--extract"))
				{
					if (i < list.size() - 1) i++; else goto nocommand;
					if (list[i]._Equal("--help") || list[i]._Equal("-h"))
					{
						cout << c(WHITE) << c(BRIGHT);
						cout << f("10", "Syntax:") << c(YELLOW) << list[i - 1] << " [option] [filename]" << endl << endl;
						cout << c(WHITE);
						cout << f("10", "Option:") << c(YELLOW) << f("10", "boot") << c(WHITE) << "Extract boot from FAT Image." << endl;
						cout << f("10", "") << c(YELLOW) << f("10", "file") << c(WHITE) << "Extract file from FAT Image." << endl << endl;
						cout << f("10", "Filename:") << c(WHITE) << f("10", "") << "Existing file within FAT Image or will be used for boot sector file name." << endl << endl;
						cout << f("10", "Info:") << c(WHITE) << f("10", "") << "This command is used to extract boot sector or any file from the FAT Image." << endl;
						cout << c(RESET);
					}
					else if (list[i]._Equal("boot") || list[i]._Equal("file"))
					{
						extract_type = list[i];
						if (i < list.size() - 1) i++; else goto nocommand;
						extract_file = list[i];
						task.push_back(operation::EXTRACT);
					}
					else
					{
						goto arg_error;
					}
				}
				else
				{
				arg_error:
					cout << c(RED) << c(BRIGHT);
					cout << "Unknown Option : " << c(WHITE) << list[i] << endl;
					cout << c(RESET);
					all_good = false;
					break;
				nocommand:
					cout << c(YELLOW) << c(BRIGHT);
					cout << "Incomplete Command, See [command] --help for syntax." << endl;
					cout << c(RESET);
					all_good = false;
					break;
				}
			}
			image_type = "fat32";
			if (!image_file.empty())
			{
				for (unsigned int i = 0; i < task.size(); i++)
				{
					switch (task[i])
					{
					case operation::WRITE:
					{
						writetoimage(write_file, image_file, write_type);
					}
					break;
					case operation::BOOT:
					{
						bootimage(image_file);
					}
					break;
					case operation::ALIGN:
					{
						alignimage(image_file);
					}
					break;
					case operation::LIST:
					{
						listimage(image_file);
					}
					break;
					case operation::REMOVE:
					{
						removefromimage(remove_file, image_file);
					}
					break;
					case operation::EXTRACT:
					{
						extractfromimageg(image_file);
					}
					break;
					}
				}
			}
			else
			{
				if (task.size() != 0)
				{
					cout << c(BRIGHT) << c(RED);
					cout << "No Image file provided! Use option " << c(YELLOW) << "--image";
					cout << c(RESET) << endl;
				}
			}
		}
#ifndef _DEBUG
	}
#endif // DEBUG
	return 0;
}