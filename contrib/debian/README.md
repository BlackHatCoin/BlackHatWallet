
Debian
====================
This directory contains files used to package blkcd/blkc-qt
for Debian-based Linux systems. If you compile blkcd/blkc-qt yourself, there are some useful files here.

## BlackHat: URI support ##


blackhat-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install blackhat-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your blkc-qt binary to `/usr/bin`
and the `../../share/pixmaps/blkc128.png` to `/usr/share/pixmaps`

blackhat-qt.protocol (KDE)

