Disk Image Loader
=================

A while ago I obtained an SCSI2SD interface for some of my vintage synthesizers.
It can emulate several drives with just one SD card, which is a feature I wanted
to take advantage of. I formatted an SD card to contain three partitions for raw
AKAI / Roland / Kurzweil CD images and a partition to be used as a FAT-formatted
hard drive. It is trivial to "mount" new CD images using Linux by using the "dd"
command, but Windows has no built-in tools to do this directly.

I decided to write a simple GUI tool which directly writes up to three CD images
to specified sectors on the SD card. Windows can only handle one partition on SD
cards, so I recommend to first partition the SD card using gparted on Linux, and
obtain the sector offsets to use with this tool from there.

A word of caution
-----------------

This tool directly manipulates the physical device and does not work on device
volumes - they are only displayed in the UI for determining the physical device
they belong to. There are practically no safe-guards, so choosing the wrong
device can cause irreversibly damage to your file system. This is one reason why
I am not providing pre-compiled binaries for this project. 

Configuration file
------------------

A configuration file called `config.json` can be placed in the same directory as
the executable, containing the default volume name to look for and the default
offsets (in 512-byte sectors) to copy the images to. An example JSON file might
look like this:

```
{
  "default_volume":"XV-5080",
  "offset1":"3655680",
  "offset2":"5007360",
  "offset3":"6359040"
}
```

Dependencies
------------

Disk Image Loader is written in C++ using Visual Studio 2015. Since it uses
low-level disk access, it doesn't make a lot of sense to support any other
compilers or operating systems.

Disk Image Loader has the following external dependencies:

 -  Qt 5 (https://www.qt.io/download/)

Contact
-------

Disk Image Loader was created by Johannes Schultz.
You can contact me through my websites:
 -  https://sagagames.de/
 -  https://sagamusix.de/