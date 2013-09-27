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

struct address_space_operations wfs_aops = {
	.readpage       = simple_readpage,
	.write_begin    = simple_write_begin,
	.write_end      = simple_write_end,
	
	/* DEPRECATED .prepare_write  = simple_prepare_write,
	.commit_write   = simple_commit_write
	.writepage	= simple_writepage,*/
};

struct file_operations wfs_file_operations = {
	.read           = do_sync_read,
	.aio_read	= generic_file_aio_read,
	.write          = do_sync_write,
	.aio_write	= generic_file_aio_write,
	.mmap           = generic_file_mmap,
	.fsync          = noop_fsync,
	.splice_read    = generic_file_splice_read,
	.llseek         = generic_file_llseek,
};