DiskDir Extended 1.51 (2007-01-02) - plugin for Total Commander
================

The same as DiskDir - creates a list file with all selected files and directories,
including subdirs - but also lists contents of archive files
internally:  
  ARJ, ACE, CAB, JAR, RAR, ZIP, TAR, TGZ (TAR.GZ), TBZ (TBZ2, TAR.BZ, TAR.BZ2) and
  ISO (NRG, IMG, BIN).  
Also lists all the other archive files recognised by installed TC plugins.
You can choose which archive types to list and whether to try to determine unknown
archive type using installed TC plugins.
Format is fully compatible with DiskDir, i.e. this plugin can display DiskDir
files, as well as DiskDir can display this plugin's files.

It creates plain text list files which can be opened in Word, Excel or other
office tools which can format TAB-delimited files, and then be formatted for
printing. It can also be used as a catalog tool: You can scan your CDs with it,
each to a file, and then search in these catalogs using the Search command with
the option "Search in archives".

Installation (TotalCommander 6.5 and later):

1. View contents of DiskDirExtended_1.51.zip within TotalCommander
2. Follow instructions

Installation (TotalCommander prior to 6.5):

1. Unzip the DiskDirExtended.wcx to any directory
2. In Total Commander, choose Configuration - Options
3. Open the 'Packer' page
4. Click 'Configure packer extension WCXs'
5. Type any extension, e.g. "lst"
6. Click 'New type', and select the DiskDirExtended.wcx
7. Click OK

How to use:

1. Select folders and files that you want to list
2. Select destination directory in opposite panel
3. Press Pack (Alt+F5). Select your chosen extension (e.g. "lst").
4. You can 'Configure' this packer. You can choose which archive types contents
   to list, or to be asked whether to list particular archive during "packing".
   This configuration is stored in DiskDirExtended.ini in the same directory as
   DiskDirExtended.wcx. See DiskDirExtended.ini for more configuration options.
5. Press OK.

This is free software and is provided "as-is" - no warranty whatsoever.

Peter Trebaticky  
Bratislava (Slovakia)  
mailto: peter.trebaticky@gmail.com

Changelog:  
**1.51**

* update for new TC7 and above API for packer plugins, huge files (> 4GB) are
  also displayed correctly (only in TC7 and above)
* fixed bug from version 1.5 eventually causing infinite loop

**1.5**

* added support for iso (nrg, bin, img)
* added support for all other extensions recognised by TC plugins
* it is possible to define (in DiskDirExtended.ini) which extensions belong to
  the same type of archive - e.g. zip=ear,war,ip
* it is possible to choose (in DiskDirExtended.ini) whether to list archives
  that appear to be empty (resulting in empty directories) - default is no
* fixed packing files from "Search result" panel

**1.4**

* added possibility to modify the list file
* better handling of 'Cancel' button
* huge files (> 4GB) are internally handled correctly, but the user sees their
  sizes incorrectly due to limitations of TotalCmd API for packer plugins

**1.32**
  the time of the directories is stored correctly

**1.31**
  fixed displaying file names containing international characters

**1.3**
  Pressing F3 (view) or F5 (copy) on a file inside list file now works if the
  original source is available - e.g. if you made list of CD and that CD is in
  drive. Only works with files that are not inside some archive (no actual
  unpacking).

**1.2**
  added support for

* tar
* tgz (tar.gz)
* tbz (tbz2, tar.bz, tar.bz2)

**1.1**
  added configuration dialog

**1.0**
  supports
    ace, arj, cab, jar, rar, zip
