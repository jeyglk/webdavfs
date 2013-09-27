#define WEBDAVFS_ROOT_I 2 

struct webdavfs_sb_info {
	int mnt_flags;
	struct nls_table *local_nls;
};

static inline struct webdavfs_sb_info *
WFS_SB(struct super_block *sb)
{
	return sb->s_fs_info;
}
