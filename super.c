//
//     Webdavfs, a WebDAV virtual filesystem Proof of concept
//     Copyright (C) 2013  Jeremy Mathevet
// 
//     This program is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
// 
//     This program is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
// 
//     You should have received a copy of the GNU General Public License
//     along with this program.  If not, see <http://www.gnu.org/licenses/>.


#include <linux/module.h>
#include <linux/fs.h>
#include <linux/nls.h>
#include <linux/string.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <net/netlink.h>
#include <net/genetlink.h>
#include <net/sock.h>
#include <net/net_namespace.h>
#include "webdavfs.h"

#define WEBDAVFS_MAGIC     0x776562646176 /* "webdav" */
#define NETLINK_NITRO 17
#define MAX_PAYLOAD 1024 /* maximum payload size */


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jeremy Mathevet <jeyg.lk@gmail.com>");
MODULE_DESCRIPTION("An experimental WebDAV FS driver");

extern struct inode_operations wfs_dir_inode_ops;
extern struct inode_operations wfs_file_inode_ops;
extern struct file_operations wfs_file_operations;
extern struct address_space_operations wfs_aops;
extern int wfs_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t dev);

int pid;

struct sk_buff *skb_global;
struct sock *nl_sk = NULL;
struct super_block *superblock;


struct resource_parser {
	uint8_t* buffer;
	size_t position;
	size_t length;
};


struct dav_resource {
	uint8_t* location;
	uint8_t* filename;
	char type;
	uint8_t* size;
};


char* substr(char* buf, const char* s, size_t beg, size_t len)
{
	  memcpy(buf, s + beg, len);
	  buf[len] = '\0';
	  return (buf);
}




/** Very ugly function, need to be rewritten.
 */
struct dav_resource parse_resource_string(uint8_t* message) {

	struct resource_parser parser = {
		.buffer = message,
		.position = 0,
		.length = strlen(message)
	};
	
	struct dav_resource r = {
		.location = NULL,
		.filename = NULL,
		.type = NULL,
		.size = NULL
	};
	
	size_t start = parser.position;
	size_t length = 0;
	
	// Fill location
	while (parser.position < parser.length) {
		if ('?' != parser.buffer[parser.position]) {
			parser.position++;
			//printk("Parser position: %i\n", parser.position);
		} else {
			char* buf = kmalloc((parser.position + 1) * sizeof(char), GFP_KERNEL);
			r.location = substr(buf, parser.buffer, start, parser.position);
			printk("location: %s\n", r.location);
			kfree(buf);
			parser.position++;
			start = parser.position;
			break;
		}
	}
	
	// Fill filename
	while (parser.position < parser.length) {
		if ('?' != parser.buffer[parser.position]) {
			parser.position++;
			//printk("Parser position: %i\n", parser.position);
		} else {
			//printk("Parser position: %i\n", parser.position);
			//printk("Start: %i\n", start);
			char* buf = kmalloc((parser.position - start + 1) * sizeof(char), GFP_KERNEL);
			r.filename = substr(buf, parser.buffer, start, parser.position - start);
			printk("filename: %s\n", r.filename);
			kfree(buf);
			parser.position++;
			start = parser.position;
			break;
		}
	}
	
	// Fill type
	if ('?' != parser.buffer[parser.position]) {
		//printk("Parser position: %i\n", parser.position);
		//printk("Start: %i\n", start);
		r.type = parser.buffer[parser.position];
		printk("type: %c\n", r.type);
		parser.position = parser.position + 2;
		start = parser.position;
		//printk("Parser position: %i\n", parser.position);
	}
	
	// File size if it's a file
	// unknown bug affecting r.name
// 	if ('f' == r.type) {
// 		while (parser.position < parser.length) {
// 			if ('?' != parser.buffer[parser.position]) {
// 				parser.position++;
// 			} else {
// 				printk("Start: %i\n", start);
// 				printk("Parser position: %i\n", parser.position);
// 				char* buf = kmalloc((parser.position - start + 1) * sizeof(char), GFP_KERNEL);
// 				r.size = substr(buf, parser.buffer, start, parser.position - start);
// 				printk("size: %s bytes\n", r.size);
// 				kfree(buf);
// 				break;
// 			}
// 		}
// 	}
// 	
	
	return r;
  
}




static int wfs_delete_dentry(struct dentry *dentry)
{
        return 1;
}




/*
 * Dentry operations
 */
struct dentry_operations wfs_dentry_ops = {

	.d_delete = wfs_delete_dentry,
};




static struct dentry *webdavfs_alloc_dentry(struct dentry *parent, char* name)
{
	struct dentry *d;
	struct qstr q;
	
	q.name = name;
	q.len = strlen(name);
	
	d = d_alloc(parent, &q);
	
	if (d)
		return d;
	
	return ERR_PTR(-ENOMEM);
}




struct inode *webdavfs_get_inode(struct super_block *sb, int mode, dev_t dev)
{
        struct inode * inode = new_inode(sb);

        if (inode) {
                inode->i_mode = mode;
                /* DEPRECATED
		 *inode->i_uid = current->fsuid;
		 *inode->i_gid = current->fsgid; */
		inode->i_uid = current_fsuid();
		inode->i_gid = current_fsgid();
		inode->i_blocks = 0;
                inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
                inode->i_mapping->a_ops = &wfs_aops;
		switch (mode & S_IFMT) {
                default:
			init_special_inode(inode, mode, dev);
			break;
		case S_IFREG:
		/* Regular file */
			printk(KERN_INFO "file inode\n");
			inode->i_op = &wfs_file_inode_ops;
			inode->i_fop =  &wfs_file_operations;
			break;
                case S_IFDIR:
		/* Directory file */
                        inode->i_op = &wfs_dir_inode_ops;
			inode->i_fop = &simple_dir_operations;

                        /* link == 2 (for initial ".." and "." entries) */
                        /* DEPRECATED inode->i_nlink++; */
			inode_inc_link_count(inode);
                        break;
                }
        }
        return inode;
}




void nl_data_ready(struct sk_buff *skb)
{
	struct nlmsghdr *nlh = NULL;
	struct inode *inode;
	struct dentry *dentry;
	struct dav_resource r;
	
	if(skb == NULL) {
		printk("skb is NULL\n");
		return ;
	}
	nlh = (struct nlmsghdr *)skb->data;
	printk(KERN_INFO "%s: received netlink message payload: %s\n",
	       __FUNCTION__, NLMSG_DATA(nlh));
	
	/*
	 * '?' = Netlink init
	 * else, WebDAV resource
	 */
	if ('?' == *(char *) NLMSG_DATA(nlh)) {
	  
		printk(KERN_INFO "Netlink handshake initiated by pid: %d\n", nlh->nlmsg_pid);
		
		pid = nlh->nlmsg_pid;
		
//  		nlh->nlmsg_len = NLMSG_LENGTH(MAX_PAYLOAD);
//  		nlh->nlmsg_pid = 0;
//  		nlh->nlmsg_flags = 0;
//  		
//  		strcpy(NLMSG_DATA(nlh), "From kernel: Yes I'm here!");
//  		NETLINK_CB(skb).portid = 0;
//  		NETLINK_CB(skb).dst_group = 0;
//   
//  		netlink_unicast(nl_sk, skb, pid, MSG_DONTWAIT);

	} else {
	  
	 	r = parse_resource_string(NLMSG_DATA(nlh));
	
		if ('f' == r.type) {
		inode = webdavfs_get_inode(superblock, S_IFREG | 0744, 0);
		} else {
		inode = webdavfs_get_inode(superblock, S_IFDIR | 0755, 0);
		}
		
		dentry = webdavfs_alloc_dentry(superblock->s_root, r.filename);
		dentry->d_op = &wfs_dentry_ops;
		d_add(dentry, inode);
		dget(dentry);   /* Extra count - pin the dentry in core */
		
	}
	
}




struct netlink_kernel_cfg cfg = {
    .input = nl_data_ready,
};





void netlink_init() {
	nl_sk = netlink_kernel_create(&init_net, NETLINK_NITRO, &cfg);
}




static void
webdavfs_put_super(struct super_block *sb)
{
	struct webdavfs_sb_info *wfs_sb;

	wfs_sb = WFS_SB(sb);
	if (wfs_sb == NULL) {
		/* Empty superblock info passed to unmount */
		return;
	}

	unload_nls(wfs_sb->local_nls);
 
	/* FS-FILLIN fs specific umount logic here */

	kfree(wfs_sb);
	return;
}





/*
 * Superblock operations
 */
struct super_operations webdavfs_super_ops = {
	.statfs         = simple_statfs,
	.drop_inode     = generic_delete_inode,
	.put_super      = webdavfs_put_super,
};




static int webdavfs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct inode *inode;
	struct dentry *dentry;
	struct webdavfs_sb_info *wfs_sb;

	sb->s_maxbytes = MAX_LFS_FILESIZE; 		/* Maximum size of the files */
	sb->s_blocksize = PAGE_CACHE_SIZE; 		/* Block size in bytes */
	sb->s_blocksize_bits = PAGE_CACHE_SHIFT; 	/* Block size in number of bits */
	sb->s_magic = WEBDAVFS_MAGIC; 			/* Filesystem magic number */
	sb->s_op = &webdavfs_super_ops;			/* Superblock methods */
	sb->s_time_gran = 1; 				/* Timestamp's granularity in nanoseconds */
	
	printk(KERN_INFO "webdavfs: fill super\n");


	inode = webdavfs_get_inode(sb, S_IFDIR | 0755, 0);

	if (!inode)
		return -ENOMEM;
	
	printk(KERN_INFO "webdavfs: about to alloc s_fs_info\n");

	/* s_fs_info: Superblock information */
	sb->s_fs_info = kzalloc(sizeof(struct webdavfs_sb_info), GFP_KERNEL);
	wfs_sb = WFS_SB(sb);
	if (!wfs_sb) {
		iput(inode);
		return -ENOMEM;
	}
	
	printk(KERN_INFO "webdavfs: about to alloc root inode\n");

	sb->s_root = d_make_root(inode);
	if (!sb->s_root) {
		iput(inode);
		kfree(wfs_sb);
		return -ENOMEM;
	}
	
	superblock = sb;
		
	/* Netlink initialization*/
	printk(KERN_INFO "webdavfs: netlink init\n");
	netlink_init();
	
	return 0;
}




static struct dentry *webdavfs_mount(struct file_system_type *fs_type,
 	int flags, const char *dev_name, void *data)
{
/* DEPRECATED
 *	return get_sb_nodev(fs_type, flags, data, webdavfs_fill_super, mnt);
 */
	return mount_nodev(fs_type, flags, data, webdavfs_fill_super);
}





/* 
 * Filesystem Type Registration
 */
static struct file_system_type webdavfs_fs_type = {
	.owner = THIS_MODULE,
	.name = "webdavfs",
	/* DEPRECATED .get_sb = webdavfs_get_sb, */
	.mount = webdavfs_mount,
	.kill_sb = kill_anon_super,
	.next = NULL,
	/*  .fs_flags */
};




static int __init init_webdavfs_fs(void)
{
	printk(KERN_INFO "Init webdavfs\n");

	return register_filesystem(&webdavfs_fs_type);
}



static void __exit exit_webdavfs_fs(void)
{
	printk(KERN_INFO "Unloading webdavfs\nGoodbye!\n");
	sock_release(nl_sk->sk_socket);

	unregister_filesystem(&webdavfs_fs_type);
}



module_init(init_webdavfs_fs)
module_exit(exit_webdavfs_fs)

