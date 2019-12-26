# moofs

moofs is a single level File Allocation Table(FAT) filesystem in userspace.

## Original PROJECT LINK
https://bluegrit.cs.umbc.edu/~lsebald1/cmsc421-fa2018/project2.shtml

### Prerequisits

Make sure you have fuse depencencies.

```
apt-get update
apt-get install fuse libfuse2 libfuse-dev
```

### Building moofs

After cloning the repo, make(using Makefile) and run!

```
make
./moofs **IMAGE_FILE.img** [OPTIONS] **MOUNTPOINT**
```

This will start the moofs. Filesystem is mounted on MOUNTPOINT.

## Usage

Currently supports generic filesystem read and write operations. 

## Terminating

CTRL+C the terminal running moofs, should exit cleanly.

  




OPEN SEPARATE TERMINAL & do things
CTRL+C original terminal, should save state back to disk.
