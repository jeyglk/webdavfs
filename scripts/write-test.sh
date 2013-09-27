#!/bin/bash

insmod webdavfs.ko;
mount -t webdavfs any /mnt/sid;
touch /mnt/sid/lol;
umount any;
rmmod webdavfs
