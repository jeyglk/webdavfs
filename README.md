webdavfs
========

A WebDAV network filesystem PoC


To test our solution, we need a GNU/Linux host, ideally running on Linux 3.10, with ownCloud (tested on ownCloud 5.0.11) and compilation tools.


Disclaimer
==========

This software is a proof of concept for an academic work, it is UNSTABLE (very likely to make your system crash), HAS NO READ/WRITE SUPPORT and many crucial filesystem features are missing. It SHOULD NOT be used for any other purpose than for research and development.


Prerequisite
============

- Linux headers
- build-essentials


Download
========

    $ git clone https://github.com/jeyglk/webdavfs.git webdavfs


Building webdavfs
=================

    $ cd webdavfs/
    $ make


Building webdavClient
=====================

    $ cd webdavClient/
    $ mkdir build
    $ cd build
    $ cmake -DCMAKE_BUILD_TYPE=Release ..
    $ make


Usage
======

When compiled, load the webdavfs module in the kernel and mount the virtual filesystem. Run the following commands from the webdavfs directory:

    # insmod webdavfs.ko
    # mount –t webdavfs any /mnt/point/path


Then, from the webdavClient build directory:

    # ./webdavClient
