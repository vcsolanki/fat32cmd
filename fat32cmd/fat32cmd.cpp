#define WIN32_LEAN_AND_MEAN
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdarg>
#include <vector>
#include <cstdint>
#include <iomanip>
#include "default_values.h"
#include "basic_fat_structs.h"
#include <Windows.h>
#undef max
#undef min

using namespace std;

vector<struct fat32> fat32_part;
vector<struct fat16> fat16_part;
struct mbr mbr_info;

string write_type;
string write_file;
string image_file;
string image_type;
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

int read_root_entry(long int current_root, long int org_root, int level, int entries, int sector_size, int cluster_size, FILE* img)
{
	static int n;
	for (int i = 0;; i++)
	{
		struct RootEntry rootentry;
		fseek(img, current_root + (32 * i), 0);
		fread(&rootentry, 1, sizeof(rootentry), img);
		if (rootentry.FileName[0] != 0x00)
		{
			if (rootentry.FileName[0] == DELETED_ROOT_ENTRY) continue;
			if (rootentry.Attribute & ATTRIB_LONG_ENTRY) continue;
			if (rootentry.FileName[0] == '.' && level != 0) continue;
			if (rootentry.FileName[0] == 0x05) rootentry.FileName[0] &= 0xE0;
			n++;
			struct LongEntry longentry;
			string name;
			for (int j = 1;; j++)
			{
				fseek(img, current_root + (32 * i) - (32 * j), 0);
				fread(&longentry, 1, sizeof(longentry), img);
				for (int k = 0; k < 10; k = k + 2) name += longentry.Name1[k];
				for (int k = 0; k < 12; k = k + 2) name += longentry.Name2[k];
				for (int k = 0; k < 4; k = k + 2) name += longentry.Name3[k];
				if (longentry.Sequence & LONG_SEQUENCE_LAST)
					break;
			}
			cout << f("26", name.c_str()) << f("11", get_size(rootentry.FileSize).c_str()) << f("25", get_date(rootentry.LastWriteDate).append(" " + get_time(rootentry.LastWriteTime)).c_str()) << get_attrib_type(rootentry.Attribute) << endl;
			if (rootentry.Attribute & ATTRIB_SUBDIR)
			{
				long int old_root = current_root;
				current_root = org_root + (sector_size * (cluster_size * (rootentry.LowFirstCluster - 2)) + (entries * 32));
				level++;
				read_root_entry(current_root, org_root, level, entries, sector_size, cluster_size, img);
				current_root = old_root;
				level--;
			}
		}
		else break;
	}
	return n;
}

void get_root_entry(string file, RootEntry* entry, long int current_root, long int org_root, int level, int entries, int sector_size, int cluster_size, FILE* img)
{
	for (int i = 0;; i++)
	{
		struct RootEntry rootentry;
		fseek(img, current_root + (32 * i), 0);
		fread(&rootentry, 1, sizeof(rootentry), img);
		if (rootentry.FileName[0] != 0x00)
		{
			if (rootentry.FileName[0] == DELETED_ROOT_ENTRY) continue;
			if (rootentry.Attribute & ATTRIB_LONG_ENTRY) continue;
			if (rootentry.FileName[0] == '.' && level != 0) continue;
			if (rootentry.FileName[0] == 0x05) rootentry.FileName[0] &= 0xE0;
			struct LongEntry longentry;
			string name;
			for (int j = 1;; j++)
			{
				fseek(img, current_root + (32 * i) - (32 * j), 0);
				fread(&longentry, 1, sizeof(longentry), img);
				for (int k = 0; k < 10; k = k + 2) name += longentry.Name1[k];
				for (int k = 0; k < 12; k = k + 2) name += longentry.Name2[k];
				for (int k = 0; k < 4; k = k + 2) name += longentry.Name3[k];
				if (longentry.Sequence & LONG_SEQUENCE_LAST)
					break;
			}
			const char* str1 = name.c_str();
			const char* str2 = file.c_str();
			if (strcmp(str1, str2) == 0)
			{
				memcpy(entry, &rootentry, sizeof(rootentry));
				return;
			}
			if (rootentry.Attribute & ATTRIB_SUBDIR)
			{
				long int old_root = current_root;
				current_root = org_root + (sector_size * (cluster_size * (rootentry.LowFirstCluster - 2)) + (entries * 32));
				level++;
				get_root_entry(file, entry, current_root, org_root, level, entries, sector_size, cluster_size, img);
				current_root = old_root;
				level--;
			}
		}
		else break;
	}
}

bool file_exist(string name)
{
	ifstream f(name.c_str());
	return f.good();
}

bool check_image(string image)
{
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
				fat32_part.push_back(info);
			}
			else if (mbr_info.partition_entry[i].Type == PTYPE_16FAT_LARGE ||
				mbr_info.partition_entry[i].Type == PTYPE_16FAT_SMALL ||
				mbr_info.partition_entry[i].Type == PTYPE_16FAT_LBA)
			{
				struct fat16 info;
				fread(&info, 1, sizeof(info), img);
				fat16_part.push_back(info);
			}
			else if (mbr_info.partition_entry[i].Type == PTYPE_12FAT)
			{
				struct fat16 info;
				fread(&info, 1, sizeof(info), img);
				fat16_part.push_back(info);
			}
		}
	}
	fclose(img);
	if (fat32_part.empty() &&
		fat16_part.empty())
	{
		return false;
	}
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

		for (unsigned int i = 0; i < fat32_part.size(); i++)
		{
			cout << c(BRIGHT);
			cout << c(WHITE) << f("20", "OEM Name") << ": " << c(YELLOW) << ls((char*)fat32_part[i].OEM, 8) << endl;
			cout << c(WHITE) << f("20", "Volume Name") << ": " << c(YELLOW) << ls((char*)fat32_part[i].VolumeLabel, 8) << endl;
			cout << c(WHITE) << f("20", "Type") << ": " << c(YELLOW) << ls((char*)fat32_part[i].FileSystem, 8) << endl;
			cout << c(WHITE) << f("20", "Sectors") << ": " << c(YELLOW) << fat32_part[i].SectorInPartition << endl;
			cout << c(WHITE) << f("20", "Bytes Per Sector") << ": " << c(YELLOW) << fat32_part[i].BytesPerSector << endl;
			cout << c(WHITE) << f("20", "Sectors Per Track") << ": " << c(YELLOW) << fat32_part[i].SectorPerTrack << endl;
			cout << c(WHITE) << f("20", "Heads") << ": " << c(YELLOW) << fat32_part[i].NumberOfHeads << endl;
			cout << c(WHITE) << f("20", "Hidden Sectors") << ": " << c(YELLOW) << fat32_part[i].HiddenSectors << endl;
			cout << c(WHITE) << f("20", "FAT Sectors") << ": " << c(YELLOW) << fat32_part[i].SectorPerFat << endl;
			cout << c(WHITE) << f("20", "FAT Count") << ": " << c(YELLOW) << +fat32_part[i].NumberOfFat << endl;
			cout << c(WHITE) << f("20", "Reserved Sectors") << ": " << c(YELLOW) << +fat32_part[i].ReservedSectors << endl;
			cout << c(WHITE) << f("20", "Sectors Per Cluster") << ": " << c(YELLOW) << +fat32_part[i].SectorPerCluster << endl;
			cout << c(WHITE) << f("20", "Root Entries") << ": " << c(YELLOW) << +fat32_part[i].RootEntries << endl;
			cout << c(RESET);
		}
		for (unsigned int i = 0; i < fat16_part.size(); i++)
		{
			cout << c(BRIGHT);
			cout << c(WHITE) << f("20", "OEM Name") << ": " << c(YELLOW) << ls((char*)fat16_part[i].OEM, 8) << endl;
			cout << c(WHITE) << f("20", "Volume Name") << ": " << c(YELLOW) << ls((char*)fat16_part[i].VolumeLabel, 8) << endl;
			cout << c(WHITE) << f("20", "Type") << ": " << c(YELLOW) << ls((char*)fat16_part[i].FileSystem, 8) << endl;
			cout << c(WHITE) << f("20", "Sectors") << ": " << c(YELLOW) << fat16_part[i].NumberOfSectorInFS << endl;
			cout << c(WHITE) << f("20", "Bytes Per Sector") << ": " << c(YELLOW) << fat16_part[i].BytesPerSector << endl;
			cout << c(WHITE) << f("20", "Sectors Per Track") << ": " << c(YELLOW) << fat16_part[i].SectorPerTrack << endl;
			cout << c(WHITE) << f("20", "Heads") << ": " << c(YELLOW) << fat16_part[i].NumberOfHeads << endl;
			cout << c(WHITE) << f("20", "Hidden Sectors") << ": " << c(YELLOW) << fat16_part[i].HiddenSectors << endl;
			cout << c(WHITE) << f("20", "FAT Sectors") << ": " << c(YELLOW) << fat16_part[i].SectorPerFat << endl;
			cout << c(WHITE) << f("20", "FAT Count") << ": " << c(YELLOW) << +fat16_part[i].NumberOfFat << endl;
			cout << c(WHITE) << f("20", "Reserved Sectors") << ": " << c(YELLOW) << +fat16_part[i].ReservedSectors << endl;
			cout << c(WHITE) << f("20", "Sectors Per Cluster") << ": " << c(YELLOW) << +fat16_part[i].SectorPerCluster << endl;
			cout << c(WHITE) << f("20", "Root Entries") << ": " << c(YELLOW) << +fat16_part[i].RootEntries << endl;
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
		int hidden_sector = 0;
		int reserved_sector = 0;
		int fat_count = 0;
		int fat_sector = 0;
		int entries = 0;
		int sector_size = 0;
		int cluster_size = 0;
		if (fat32_part.size() != 0)
		{
			hidden_sector = fat32_part[0].HiddenSectors;
			reserved_sector = fat32_part[0].ReservedSectors;
			fat_count = fat32_part[0].NumberOfFat;
			fat_sector = fat32_part[0].SectorPerFat;
			sector_size = fat32_part[0].BytesPerSector;
			entries = fat32_part[0].RootEntries;
			cluster_size = fat32_part[0].SectorPerCluster;
		}
		else if (fat16_part.size() != 0)
		{
			hidden_sector = fat16_part[0].HiddenSectors;
			reserved_sector = fat16_part[0].ReservedSectors;
			fat_count = fat16_part[0].NumberOfFat;
			fat_sector = fat16_part[0].SectorPerFat;
			sector_size = fat16_part[0].BytesPerSector;
			entries = fat16_part[0].RootEntries;
			cluster_size = fat16_part[0].SectorPerCluster;
		}
		long int root_directory = (hidden_sector + (fat_sector * fat_count) + reserved_sector) * sector_size;
		FILE* img = fopen(image.c_str(), "r");
		fseek(img, root_directory, 0);
		cout << c(BRIGHT) << c(YELLOW);
		cout << f("26", "Name") << f("11", "Size") << f("25", "Creation Date") << "Type" << endl << endl;
		cout << c(WHITE);
		int n = read_root_entry(root_directory, root_directory, 0, entries, sector_size, cluster_size, img);
		fclose(img);
		if (n == 0)
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
			int hidden_sector = 0;
			int reserved_sector = 0;
			int fat_count = 0;
			int fat_sector = 0;
			int entries = 0;
			int sector_size = 0;
			int cluster_size = 0;
			if (fat32_part.size() != 0)
			{
				hidden_sector = fat32_part[0].HiddenSectors;
				reserved_sector = fat32_part[0].ReservedSectors;
				fat_count = fat32_part[0].NumberOfFat;
				fat_sector = fat32_part[0].SectorPerFat;
				sector_size = fat32_part[0].BytesPerSector;
				entries = fat32_part[0].RootEntries;
				cluster_size = fat32_part[0].SectorPerCluster;
			}
			else if (fat16_part.size() != 0)
			{
				hidden_sector = fat16_part[0].HiddenSectors;
				reserved_sector = fat16_part[0].ReservedSectors;
				fat_count = fat16_part[0].NumberOfFat;
				fat_sector = fat16_part[0].SectorPerFat;
				sector_size = fat16_part[0].BytesPerSector;
				entries = fat16_part[0].RootEntries;
				cluster_size = fat16_part[0].SectorPerCluster;
			}
			long root_directory = (hidden_sector + (fat_sector * fat_count) + reserved_sector) * sector_size;
			FILE* img = fopen(image.c_str(), "rb");
			fseek(img, root_directory, 0);
			struct RootEntry rootentry;
			rootentry.FileSize = -1;
			get_root_entry(extract_file, &rootentry, root_directory, root_directory, 0, entries, sector_size, cluster_size, img);
			if (rootentry.FileSize == -1)
			{
				cout << c(BRIGHT) << c(YELLOW);
				cout << "There is no file named " << extract_file << " in this FAT Image.";
				cout << c(RESET);
			}
			else
			{
				uint32_t cluster_last, cluster_entry;
				if (fat32_part.size() > 0) { cluster_last = FAT32_CLUSTER_LAST; cluster_entry = 4; }
				else if (fat16_part.size() > 0)
					if (strncmp((char*)fat16_part[0].FileSystem, "FAT12   ", 8) == 0) { cluster_last = 0xFF; cluster_entry = 1; }
					else if (strncmp((char*)fat16_part[0].FileSystem, "FAT16   ", 8) == 0) { cluster_last = FAT16_CLUSTER_LAST; cluster_entry = 2; }
				FILE* extr = fopen(extract_file.c_str(), "wb");
				long fat_table = (hidden_sector + reserved_sector) * sector_size;
				uint32_t cluster = rootentry.LowFirstCluster;
				if(fat32_part.size()>0)
					cluster = (rootentry.HighFirstCluster << 16) | rootentry.LowFirstCluster;
				uint32_t size = 0;
				uint32_t read_size = 0;
				uint8_t* buf = (uint8_t*)malloc(cluster_size * sector_size);
				do
				{
					read_size = cluster_size * sector_size;
					size += read_size;
					if (read_size > rootentry.FileSize)
						read_size = rootentry.FileSize;
					else if (size > rootentry.FileSize)
					{
						size = size - read_size;
						read_size = rootentry.FileSize - size;
					}
					fseek(img, root_directory + (sector_size * (cluster_size * (cluster - 2)) + (entries * 32)), SEEK_SET);
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
							cluster = (cluster & 0x00FF);
						}
						else
						{
							cluster = 0;
							fread(&cluster, 1, 2, img);
							cluster = (cluster & 0x0FF0) >> 4;
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
		string s = "-e file image.jpg -i 16hdd.img";
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
			cout << f("10", "") << c(YELLOW) << f("15", "-l, --list") << c(WHITE) << "List all files and folders from FAT Image." << endl;
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
						cout << f("10", "Syntax:") << c(YELLOW) << list[i - 1] << endl << endl;
						cout << c(WHITE);
						cout << f("10", "Note:") << "This is standalone command, no parameters." << endl << endl;
						cout << f("10", "Info:") << "This command list all the files available in the FAT Image file." << endl;
						cout << c(RESET);
					}
					else
						task.push_back(operation::LIST);
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
					cout << "No Image file provided! Use option "<< c(YELLOW) <<"--image";
					cout << c(RESET) << endl;
				}
			}
		}
#ifndef _DEBUG
	}
#endif // DEBUG
	return 0;
}