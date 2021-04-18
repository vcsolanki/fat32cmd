# FAT32CMD
A Small Utility that can work with FAT Hard Disk Images.

I made this utility to easily work with my ongoing OS development! It can handle all basic stuff with FAT Images.
It also works with FAT12, FAT16 and obviously FAT32.

# Commands

## Syntax   

    fat32cmd [command] [options] [image]
  
    -h, --help      Show's help for Commands.
    -w, --write     Write to FAT Image.
    -i, --image     Indicates FAT Image file.
    -b, --boot      Information about FAT Image.
    -a, --align     Align the FAT Image.
    -l, --list      List all files and folders from FAT Image.
    -e, --extract   Extract file or boot from FAT Image.
    -r, --remove    Remove file from FAT Image.
           
## Example
           
    fat32cmd -w boot boot.bin -i hdd.img
    fat32cmd --align --image hdd.img
    fat32cmd -b -i hdd.img
