//
//     Webdavfs, a WebDAV virtual filesystem proof of concept
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
#include <net/netlink.h>
#include <net/sock.h>
#include <linux/string.h>
#include "webdavfs.h"

#define NETLINK_NITRO 17
#define MAX_PAYLOAD 1024 /* maximum payload size */

extern struct dentry_operations wfs_dentry_ops;
extern struct inode *webdavfs_get_inode(struct super_block *sb, int mode, dev_t dev);

extern int pid;

extern struct sock *nl_sk;

/*
 * Lookup the data
 */
static struct dentry *wfs_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd)
{
	printk(KERN_INFO "webdavfs: wfs lookup\n");
	struct webdavfs_sb_info * wfs_sb = WFS_SB(dir->i_sb);
	if (dentry->d_name.len > NAME_MAX)
		return ERR_PTR(-ENAMETOOLONG);
	dentry->d_op = &wfs_dentry_ops;
	d_add(dentry, NULL);
	return NULL;
}

int wfs_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t dev)
{
	struct inode * inode = webdavfs_get_inode(dir->i_sb, mode, dev);
	int error = -ENOSPC;
	
	printk(KERN_INFO "webdavfs: mknod\n");
	printk(KERN_INFO "dentry name: %s\n", dentry->d_name.name);
	printk(KERN_INFO "parent dentry name: %s\n", dentry->d_parent->d_name.name);
	if (inode) {
		if (dir->i_mode & S_ISGID) {
			inode->i_gid = dir->i_gid;
			/* TEMP */
			 if (S_ISDIR(mode))
                                inode->i_mode |= S_ISGID;
		}
		d_instantiate(dentry, inode);
		dget(dentry);   /* Extra count - pin the dentry in core */
		error = 0;
		dir->i_mtime = dir->i_ctime = CURRENT_TIME;

		/* real filesystems would normally use i_size_write function */
		dir->i_size += 0x20;  /* bogus small size for each dir entry */
	}
	return error;
}

static int wfs_mkdir(struct inode * dir, struct dentry * dentry, int mode)
{
  
	struct sk_buff *skb_out;
	struct nlmsghdr *nlh;
	char* msg = "?m?testMkdir?";
	//char* dentry_name;
	int msg_size = strlen(msg);
	
	if(nl_sk ==NULL)
	printk("netlink create fail\n");
	else
	printk("netlink create pass\n");

	skb_out = nlmsg_new(msg_size, 0);
	if(skb_out == NULL)
	printk("skb alloc fail\n");
	else
	printk("skb alloc pass\n");

	nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);
	NETLINK_CB(skb_out).dst_group = 0; /* not in mcast group */
	
	//strcpy(dentry_name, dentry->d_name.name);
	//snprintf(msg, 100, "?m?%s?", dentry_name);

	strncpy(nlmsg_data(nlh), msg, msg_size);

	netlink_unicast(nl_sk, skb_out, pid, MSG_DONTWAIT);
  
        int retval = 0;
	retval = wfs_mknod(dir, dentry, mode | S_IFDIR, 0);

	/* link count is two for dir, for dot and dot dot */
        if (!retval)
                /* DEPRECATED dir->i_nlink++; */
				inode_inc_link_count(dir);
        return retval;
}

static int wfs_create(struct inode *dir, struct dentry *dentry, int mode,
			struct nameidata *nd)
{
  
  struct sk_buff *skb_out;
	struct nlmsghdr *nlh;
	char* msg = "?f?testFileCreate?";
	//char* dentry_name;
	int msg_size = strlen(msg);
	
	if(nl_sk ==NULL)
	printk("netlink create fail\n");
	else
	printk("netlink create pass\n");

	skb_out = nlmsg_new(msg_size, 0);
	if(skb_out == NULL)
	printk("skb alloc fail\n");
	else
	printk("skb alloc pass\n");

	nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);
	NETLINK_CB(skb_out).dst_group = 0; /* not in mcast group */

	strncpy(nlmsg_data(nlh), msg, msg_size);

	netlink_unicast(nl_sk, skb_out, pid, MSG_DONTWAIT);
	
	return wfs_mknod(dir, dentry, mode | S_IFREG, 0);
}


struct inode_operations wfs_file_inode_ops = {
        .getattr        = simple_getattr,
	.setattr        = simple_setattr,
};


struct inode_operations wfs_dir_inode_ops = {
	.create         = wfs_create,
	//.lookup         = wfs_lookup,
	.lookup		= simple_lookup,
	.unlink         = simple_unlink,
	.mkdir		= wfs_mkdir,
	.rmdir		= simple_rmdir,
	.mknod          = wfs_mknod,
	.rename         = simple_rename,
};