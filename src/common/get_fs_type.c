/*
 *  get_fs_type.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2026 Holger Kiehl <Holger.Kiehl@dwd.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "afddefs.h"

DESCR__S_M3
/*
 ** NAME
 **   get_fs_type - gets the file system name and if it is local
 **                 or remote
 **
 ** SYNOPSIS
 **   int get_fs_type(char *path, int is_remote, char *fs_type_name)
 **
 ** DESCRIPTION
 **   The function get_fs_type() gets the name of file system type
 **   and if it is local or remote.
 **
 ** RETURN VALUES
 **   Function get_fs_type() will return INCORRECT on error otherwise
 **   SUCCESS and in fs_type_name a string with the filesystem type name.
 **   If filesystem type is not known 'unknown' will be returned.
 **   is_remote can have the following values:
 **          0 - local filesystem
 **          1 - remote filesystem
 **          2 - network filesystem
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.08.2026 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>    /* strerror()                                     */
#ifdef LINUX
# include <sys/vfs.h>  /* struct statfs, statfs()                        */
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__APPLE__)
/* Provides struct statfs, statfs(), and the MNT_LOCAL flag */
# include <sys/param.h> 
# include <sys/mount.h>
#elif defined(__sun) || defined(__svr4__)
# include <sys/statvfs.h> /* struct statvfs, statvfs() */
# include <sys/stat.h>
#else /* Fallback to the POSIX standard compliance header */
# include <sys/statvfs.h>
#endif
#include <errno.h>

/* Local function prototypes. */
static char *fstype(struct statfs *, int *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$ get_fs_type() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
get_fs_type(char *path, int *is_remote, char *fs_type_name)
{
#ifdef HAVE_STATFS
   struct statfs buf;

   if (statfs(path, &buf) == -1)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to statfs() `%s' : %s", path, strerror(errno));
      return(INCORRECT);
   }
   if (fs_type_name == NULL)
   {
      (void)fstype(&buf, is_remote);
   }
   else
   {
      (void)strcpy(fs_type_name, fstype(&buf, is_remote));
   }
#else
   if (fs_type_name != NULL)
   {
      (void)strcpy(fs_type_name, "unknown")
      is_remote = -1;
   }
#endif

   return(SUCCESS);

}


/*++++++++++++++++++++++++++++++ fstype() ++++++++++++++++++++++++++++++*/
static char *
fstype(struct statfs *buf, int *is_remote)
{
#if defined (HAVE_STRUCT_STATFS_F_FSTYPENAME) /* macOS, FreeBSD, NetBSD */
   *is_remote = !(buf.f_flags & MNT_LOCAL);
   return(buf->f_fstypename);
#elif defined (HAVE_STRUCT_STATVFS_F_BASETYPE) /* Solaris */
   *is_remote = -1; /* Not implemented, so unknown. */
   return(buf->f_basetype);
#else /* Linux */
/*
 * Note this code was taken from GNU coreutils:
 *    https://www.gnu.org/software/coreutils
 * Specifically taken from coreutils-9.11.tar.xz.
 * The Magic numbers are taken from src/fs.h and the names
 * from src/stat.c human_fstype().
 */
# define S_MAGIC_AAFS 0x5A3C69F0
# define S_MAGIC_ACFS 0x61636673
# define S_MAGIC_ADFS 0xADF5
# define S_MAGIC_AFFS 0xADFF
# define S_MAGIC_AFS 0x5346414F
# define S_MAGIC_ANON_INODE_FS 0x09041934
# define S_MAGIC_AUFS 0x61756673
# define S_MAGIC_AUTOFS 0x0187
# define S_MAGIC_BALLOON_KVM 0x13661366
# define S_MAGIC_BCACHEFS 0xCA451A4E
# define S_MAGIC_BEFS 0x42465331
# define S_MAGIC_BDEVFS 0x62646576
# define S_MAGIC_BFS 0x1BADFACE
# define S_MAGIC_BINDERFS 0x6C6F6F70
# define S_MAGIC_BPF_FS 0xCAFE4A11
# define S_MAGIC_BINFMTFS 0x42494E4D
# define S_MAGIC_BTRFS 0x9123683E
# define S_MAGIC_BTRFS_TEST 0x73727279
# define S_MAGIC_CEPH 0x00C36400
# define S_MAGIC_CGROUP 0x0027E0EB
# define S_MAGIC_CGROUP2 0x63677270
# define S_MAGIC_CIFS 0xFF534D42
# define S_MAGIC_CODA 0x73757245
# define S_MAGIC_COH 0x012FF7B7
# define S_MAGIC_CONFIGFS 0x62656570
# define S_MAGIC_CRAMFS 0x28CD3D45
# define S_MAGIC_CRAMFS_WEND 0x453DCD28
# define S_MAGIC_DAXFS 0x64646178
# define S_MAGIC_DEBUGFS 0x64626720
# define S_MAGIC_DEVFS 0x1373
# define S_MAGIC_DEVMEM 0x454D444D
# define S_MAGIC_DEVPTS 0x1CD1
# define S_MAGIC_DMA_BUF 0x444D4142
# define S_MAGIC_ECRYPTFS 0xF15F
# define S_MAGIC_EFIVARFS 0xDE5E81E4
# define S_MAGIC_EFS 0x00414A53
# define S_MAGIC_EROFS_V1 0xE0F5E1E2
# define S_MAGIC_EXFAT 0x2011BAB0
# define S_MAGIC_EXFS 0x45584653
# define S_MAGIC_EXOFS 0x5DF5
# define S_MAGIC_EXT 0x137D
# define S_MAGIC_EXT2 0xEF53
# define S_MAGIC_EXT2_OLD 0xEF51
# define S_MAGIC_F2FS 0xF2F52010
# define S_MAGIC_FAT 0x4006
# define S_MAGIC_FHGFS 0x19830326
# define S_MAGIC_FUSE 0x65735546
# define S_MAGIC_FUSECTL 0x65735543
# define S_MAGIC_FUTEXFS 0x0BAD1DEA
# define S_MAGIC_GFS 0x01161970
# define S_MAGIC_GPFS 0x47504653
# define S_MAGIC_GUEST_MEMFD 0x474D454D
# define S_MAGIC_HFS 0x4244
# define S_MAGIC_HFS_PLUS 0x482B
# define S_MAGIC_HFS_X 0x4858
# define S_MAGIC_HOSTFS 0x00C0FFEE
# define S_MAGIC_HPFS 0xF995E849
# define S_MAGIC_HUGETLBFS 0x958458F6
# define S_MAGIC_MTD_INODE_FS 0x11307854
# define S_MAGIC_IBRIX 0x013111A8
# define S_MAGIC_INOTIFYFS 0x2BAD1DEA
# define S_MAGIC_ISOFS 0x9660
# define S_MAGIC_ISOFS_R_WIN 0x4004
# define S_MAGIC_ISOFS_WIN 0x4000
# define S_MAGIC_JFFS 0x07C0
# define S_MAGIC_JFFS2 0x72B6
# define S_MAGIC_JFS 0x3153464A
# define S_MAGIC_KAFS 0x6B414653
# define S_MAGIC_LOGFS 0xC97E8168
# define S_MAGIC_LUSTRE 0x0BD00BD0
# define S_MAGIC_M1FS 0x5346314D
# define S_MAGIC_MINIX 0x137F
# define S_MAGIC_MINIX_30 0x138F
# define S_MAGIC_MINIX_V2 0x2468
# define S_MAGIC_MINIX_V2_30 0x2478
# define S_MAGIC_MINIX_V3 0x4D5A
# define S_MAGIC_MQUEUE 0x19800202
# define S_MAGIC_MSDOS 0x4D44
# define S_MAGIC_NCP 0x564C
# define S_MAGIC_NFS 0x6969
# define S_MAGIC_NFSD 0x6E667364
# define S_MAGIC_NILFS 0x3434
# define S_MAGIC_NSFS 0x6E736673
# define S_MAGIC_NTFS 0x5346544E
# define S_MAGIC_OPENPROM 0x9FA1
# define S_MAGIC_OCFS2 0x7461636F
# define S_MAGIC_OVERLAYFS 0x794C7630
# define S_MAGIC_PANFS 0xAAD7AAEA
# define S_MAGIC_PID_FS 0x50494446
# define S_MAGIC_PIPEFS 0x50495045
# define S_MAGIC_PPC_CMM 0xC7571590
# define S_MAGIC_PRL_FS 0x7C7C6673
# define S_MAGIC_PROC 0x9FA0
# define S_MAGIC_PSTOREFS 0x6165676C
# define S_MAGIC_QNX4 0x002F
# define S_MAGIC_QNX6 0x68191122
# define S_MAGIC_RAMFS 0x858458F6
# define S_MAGIC_RDTGROUP 0x07655821
# define S_MAGIC_REISERFS 0x52654973
# define S_MAGIC_ROMFS 0x7275
# define S_MAGIC_RPC_PIPEFS 0x67596969
# define S_MAGIC_SDCARDFS 0x5DCA2DF5
# define S_MAGIC_SECRETMEM 0x5345434D
# define S_MAGIC_SECURITYFS 0x73636673
# define S_MAGIC_SELINUX 0xF97CFF8C
# define S_MAGIC_SMACK 0x43415D53
# define S_MAGIC_SMB 0x517B
# define S_MAGIC_SMB2 0xFE534D42
# define S_MAGIC_SNFS 0xBEEFDEAD
# define S_MAGIC_SOCKFS 0x534F434B
# define S_MAGIC_SQUASHFS 0x73717368
# define S_MAGIC_SYSFS 0x62656572
# define S_MAGIC_SYSV2 0x012FF7B6
# define S_MAGIC_SYSV4 0x012FF7B5
# define S_MAGIC_TMPFS 0x01021994
# define S_MAGIC_TRACEFS 0x74726163
# define S_MAGIC_UBIFS 0x24051905
# define S_MAGIC_UDF 0x15013346
# define S_MAGIC_UFS 0x00011954
# define S_MAGIC_UFS_BYTESWAPPED 0x54190100
# define S_MAGIC_USBDEVFS 0x9FA2
# define S_MAGIC_V9FS 0x01021997
# define S_MAGIC_VBOXSF 0x786F4256
# define S_MAGIC_VMHGFS 0xBACBACBC
# define S_MAGIC_VXFS 0xA501FCF5
# define S_MAGIC_VZFS 0x565A4653
# define S_MAGIC_WSLFS 0x53464846
# define S_MAGIC_XENFS 0xABBA1974
# define S_MAGIC_XENIX 0x012FF7B4
# define S_MAGIC_XFS 0x58465342
# define S_MAGIC_XIAFS 0x012FD16D
# define S_MAGIC_Z3FOLD 0x0033
# define S_MAGIC_ZFS 0x2FC12FC1
# define S_MAGIC_ZONEFS 0x5A4F4653
# define S_MAGIC_ZSMALLOC 0x58295829

   switch(buf->f_type)
   {
      case S_MAGIC_AAFS: /* 0x5A3C69F0 local */
         *is_remote = 0;
         return("aafs");
      case S_MAGIC_ACFS: /* 0x61636673 remote */
         *is_remote = 1;
         return("acfs");
      case S_MAGIC_ADFS: /* 0xADF5 local */
         *is_remote = 0;
         return("adfs");
      case S_MAGIC_AFFS: /* 0xADFF local */
         *is_remote = 0;
         return("affs");
      case S_MAGIC_AFS: /* 0x5346414F remote */
         *is_remote = 1;
         return("afs");
      case S_MAGIC_ANON_INODE_FS: /* 0x09041934 local */
         *is_remote = 0;
         return("anon-inode FS");
      case S_MAGIC_AUFS: /* 0x61756673 remote */
         *is_remote = 0;
         /* FIXME: change syntax or add an optional attribute like "inotify:no".
            The above is labeled as "remote" so that tail always uses polling,
            but this isn't really a remote file system type.  */
         return("aufs");
      case S_MAGIC_AUTOFS: /* 0x0187 local */
         *is_remote = 0;
         return("autofs");
      case S_MAGIC_BALLOON_KVM: /* 0x13661366 local */
         *is_remote = 0;
         return("balloon-kvm-fs");
      case S_MAGIC_BCACHEFS: /* 0xCA451A4E local */
         *is_remote = 0;
         return("bcachefs");
      case S_MAGIC_BEFS: /* 0x42465331 local */
         *is_remote = 0;
         return("befs");
      case S_MAGIC_BDEVFS: /* 0x62646576 local */
         *is_remote = 0;
         return("bdevfs");
      case S_MAGIC_BFS: /* 0x1BADFACE local */
         *is_remote = 0;
         return("bfs");
      case S_MAGIC_BINDERFS: /* 0x6C6F6F70 local */
         *is_remote = 0;
         return("binderfs");
      case S_MAGIC_BPF_FS: /* 0xCAFE4A11 local */
         *is_remote = 0;
         return("bpf_fs");
      case S_MAGIC_BINFMTFS: /* 0x42494E4D local */
         *is_remote = 0;
         return("binfmt_misc");
      case S_MAGIC_BTRFS: /* 0x9123683E local */
         *is_remote = 0;
         return("btrfs");
      case S_MAGIC_BTRFS_TEST: /* 0x73727279 local */
         *is_remote = 0;
         return("btrfs_test");
      case S_MAGIC_CEPH: /* 0x00C36400 remote */
         *is_remote = 2;
         return("ceph");
      case S_MAGIC_CGROUP: /* 0x0027E0EB local */
         *is_remote = 0;
         return("cgroupfs");
      case S_MAGIC_CGROUP2: /* 0x63677270 local */
         *is_remote = 0;
         return("cgroup2fs");
      case S_MAGIC_CIFS: /* 0xFF534D42 remote */
         *is_remote = 2;
         return("cifs");
      case S_MAGIC_CODA: /* 0x73757245 remote */
         *is_remote = 2;
         return("coda");
      case S_MAGIC_COH: /* 0x012FF7B7 local */
         *is_remote = 0;
         return("coh");
      case S_MAGIC_CONFIGFS: /* 0x62656570 local */
         *is_remote = 0;
         return("configfs");
      case S_MAGIC_CRAMFS: /* 0x28CD3D45 local */
         *is_remote = 0;
         return("cramfs");
      case S_MAGIC_CRAMFS_WEND: /* 0x453DCD28 local */
         *is_remote = 0;
         return("cramfs-wend");
      case S_MAGIC_DAXFS: /* 0x64646178 local */
         *is_remote = 0;
         return("daxfs");
      case S_MAGIC_DEBUGFS: /* 0x64626720 local */
         *is_remote = 0;
         return("debugfs");
      case S_MAGIC_DEVFS: /* 0x1373 local */
         *is_remote = 0;
         return("devfs");
      case S_MAGIC_DEVMEM: /* 0x454D444D local */
         *is_remote = 0;
         return("devmem");
      case S_MAGIC_DEVPTS: /* 0x1CD1 local */
         *is_remote = 0;
         return("devpts");
      case S_MAGIC_DMA_BUF: /* 0x444D4142 local */
         *is_remote = 0;
         return("dma-buf-fs");
      case S_MAGIC_ECRYPTFS: /* 0xF15F local */
         *is_remote = 0;
         return("ecryptfs");
      case S_MAGIC_EFIVARFS: /* 0xDE5E81E4 local */
         *is_remote = 0;
         return("efivarfs");
      case S_MAGIC_EFS: /* 0x00414A53 local */
         *is_remote = 0;
         return("efs");
      case S_MAGIC_EROFS_V1: /* 0xE0F5E1E2 local */
         *is_remote = 0;
         return("erofs");
      case S_MAGIC_EXFAT: /* 0x2011BAB0 local */
         *is_remote = 0;
         return("exfat");
      case S_MAGIC_EXFS: /* 0x45584653 local */
         *is_remote = 0;
         return("exfs");
      case S_MAGIC_EXOFS: /* 0x5DF5 local */
         *is_remote = 0;
         return("exofs");
      case S_MAGIC_EXT: /* 0x137D local */
         *is_remote = 0;
         return("ext");
      case S_MAGIC_EXT2: /* 0xEF53 local */
         *is_remote = 0;
         return("ext2/3/4");
      case S_MAGIC_EXT2_OLD: /* 0xEF51 local */
         *is_remote = 0;
         return("ext2");
      case S_MAGIC_F2FS: /* 0xF2F52010 local */
         *is_remote = 0;
         return("f2fs");
      case S_MAGIC_FAT: /* 0x4006 local */
         *is_remote = 0;
         return("fat");
      case S_MAGIC_FHGFS: /* 0x19830326 remote */
         *is_remote = 2;
         return("fhgfs");
      case S_MAGIC_FUSE: /* 0x65735546 remote */
         *is_remote = 1;
         return("fuse");
      case S_MAGIC_FUSECTL: /* 0x65735543 remote */
         *is_remote = 1;
         return("fusectl");
      case S_MAGIC_FUTEXFS: /* 0x0BAD1DEA local */
         *is_remote = 0;
         return("futexfs");
      case S_MAGIC_GFS: /* 0x01161970 remote */
         *is_remote = 2;
         return("gfs/gfs2");
      case S_MAGIC_GPFS: /* 0x47504653 remote */
         *is_remote = 2;
         return("gpfs");
      case S_MAGIC_GUEST_MEMFD: /* 0x474D454D remote */
         *is_remote = 1;
         return("guest-memfd");
      case S_MAGIC_HFS: /* 0x4244 local */
         *is_remote = 0;
         return("hfs");
      case S_MAGIC_HFS_PLUS: /* 0x482B local */
         *is_remote = 0;
         return("hfs+");
      case S_MAGIC_HFS_X: /* 0x4858 local */
         *is_remote = 0;
         return("hfsx");
      case S_MAGIC_HOSTFS: /* 0x00C0FFEE local */
         *is_remote = 0;
         return("hostfs");
      case S_MAGIC_HPFS: /* 0xF995E849 local */
         *is_remote = 0;
         return("hpfs");
      case S_MAGIC_HUGETLBFS: /* 0x958458F6 local */
         *is_remote = 0;
         return("hugetlbfs");
      case S_MAGIC_MTD_INODE_FS: /* 0x11307854 local */
         *is_remote = 0;
         return("inodefs");
      case S_MAGIC_IBRIX: /* 0x013111A8 remote */
         *is_remote = 2;
         return("ibrix");
      case S_MAGIC_INOTIFYFS: /* 0x2BAD1DEA local */
         *is_remote = 0;
         return("inotifyfs");
      case S_MAGIC_ISOFS: /* 0x9660 local */
         *is_remote = 0;
         return("isofs");
      case S_MAGIC_ISOFS_R_WIN: /* 0x4004 local */
         *is_remote = 0;
         return("isofs");
      case S_MAGIC_ISOFS_WIN: /* 0x4000 local */
         *is_remote = 0;
         return("isofs");
      case S_MAGIC_JFFS: /* 0x07C0 local */
         *is_remote = 0;
         return("jffs");
      case S_MAGIC_JFFS2: /* 0x72B6 local */
         *is_remote = 0;
         return("jffs2");
      case S_MAGIC_JFS: /* 0x3153464A local */
         *is_remote = 0;
         return("jfs");
      case S_MAGIC_KAFS: /* 0x6B414653 remote */
         *is_remote = 2;
         return("k-afs");
      case S_MAGIC_LOGFS: /* 0xC97E8168 local */
         *is_remote = 0;
         return("logfs");
      case S_MAGIC_LUSTRE: /* 0x0BD00BD0 remote */
         *is_remote = 2;
         return("lustre");
      case S_MAGIC_M1FS: /* 0x5346314D local */
         *is_remote = 0;
         return("m1fs");
      case S_MAGIC_MINIX: /* 0x137F local */
         *is_remote = 0;
         return("minix");
      case S_MAGIC_MINIX_30: /* 0x138F local */
         *is_remote = 0;
         return("minix (30 char.)");
      case S_MAGIC_MINIX_V2: /* 0x2468 local */
         *is_remote = 0;
         return("minix v2");
      case S_MAGIC_MINIX_V2_30: /* 0x2478 local */
         *is_remote = 0;
         return("minix v2 (30 char.)");
      case S_MAGIC_MINIX_V3: /* 0x4D5A local */
         *is_remote = 0;
         return("minix3");
      case S_MAGIC_MQUEUE: /* 0x19800202 local */
         *is_remote = 0;
         return("mqueue");
      case S_MAGIC_MSDOS: /* 0x4D44 local */
         *is_remote = 0;
         return("msdos");
      case S_MAGIC_NCP: /* 0x564C remote */
         *is_remote = 2;
         return("novell");
      case S_MAGIC_NFS: /* 0x6969 remote */
         *is_remote = 2;
         return("nfs");
      case S_MAGIC_NFSD: /* 0x6E667364 remote */
         *is_remote = 2;
         return("nfsd");
      case S_MAGIC_NILFS: /* 0x3434 local */
         *is_remote = 0;
         return("nilfs");
      case S_MAGIC_NSFS: /* 0x6E736673 local */
         *is_remote = 0;
         return("nsfs");
      case S_MAGIC_NTFS: /* 0x5346544E local */
         *is_remote = 0;
         return("ntfs");
      case S_MAGIC_OPENPROM: /* 0x9FA1 local */
         *is_remote = 0;
         return("openprom");
      case S_MAGIC_OCFS2: /* 0x7461636F remote */
         *is_remote = 1;
         return("ocfs2");
      case S_MAGIC_OVERLAYFS: /* 0x794C7630 remote */
         *is_remote = 1;
         /* This may overlay remote file systems.
            Also there have been issues reported with inotify and overlayfs,
            so mark as "remote" so that polling is used.  */
         return("overlayfs");
      case S_MAGIC_PANFS: /* 0xAAD7AAEA remote */
         *is_remote = 2;
         return("panfs");
      case S_MAGIC_PID_FS: /* 0x50494446 local */
         *is_remote = 0;
         return("pidfs");
      case S_MAGIC_PIPEFS: /* 0x50495045 remote */
         *is_remote = 1;
         /* FIXME: change syntax or add an optional attribute like "inotify:no".
            pipefs is labeled as "remote" so that tail always polls,
            but it isn't really remote file system type.  */
         return("pipefs");
      case S_MAGIC_PPC_CMM: /* 0xC7571590 local */
         *is_remote = 0;
         return("ppc-cmm-fs");
      case S_MAGIC_PRL_FS: /* 0x7C7C6673 remote */
         *is_remote = 1;
         return("prl_fs");
      case S_MAGIC_PROC: /* 0x9FA0 local */
         *is_remote = 0;
         return("proc");
      case S_MAGIC_PSTOREFS: /* 0x6165676C local */
         *is_remote = 0;
         return("pstorefs");
      case S_MAGIC_QNX4: /* 0x002F local */
         *is_remote = 0;
         return("qnx4");
      case S_MAGIC_QNX6: /* 0x68191122 local */
         *is_remote = 0;
         return("qnx6");
      case S_MAGIC_RAMFS: /* 0x858458F6 local */
         *is_remote = 0;
         return("ramfs");
      case S_MAGIC_RDTGROUP: /* 0x07655821 local */
         *is_remote = 0;
         return("rdt");
      case S_MAGIC_REISERFS: /* 0x52654973 local */
         *is_remote = 0;
         return("reiserfs");
      case S_MAGIC_ROMFS: /* 0x7275 local */
         *is_remote = 0;
         return("romfs");
      case S_MAGIC_RPC_PIPEFS: /* 0x67596969 local */
         *is_remote = 0;
         return("rpc_pipefs");
      case S_MAGIC_SDCARDFS: /* 0x5DCA2DF5 local */
         *is_remote = 0;
         return("sdcardfs");
      case S_MAGIC_SECRETMEM: /* 0x5345434D local */
         *is_remote = 0;
         return("secretmem");
      case S_MAGIC_SECURITYFS: /* 0x73636673 local */
         *is_remote = 0;
         return("securityfs");
      case S_MAGIC_SELINUX: /* 0xF97CFF8C local */
         *is_remote = 0;
         return("selinux");
      case S_MAGIC_SMACK: /* 0x43415D53 local */
         *is_remote = 0;
         return("smackfs");
      case S_MAGIC_SMB: /* 0x517B remote */
         *is_remote = 2;
         return("smb");
      case S_MAGIC_SMB2: /* 0xFE534D42 remote */
         *is_remote = 2;
         return("smb2");
      case S_MAGIC_SNFS: /* 0xBEEFDEAD remote */
         *is_remote = 2;
         return("snfs");
      case S_MAGIC_SOCKFS: /* 0x534F434B local */
         *is_remote = 0;
         return("sockfs");
      case S_MAGIC_SQUASHFS: /* 0x73717368 local */
         *is_remote = 0;
         return("squashfs");
      case S_MAGIC_SYSFS: /* 0x62656572 local */
         *is_remote = 0;
         return("sysfs");
      case S_MAGIC_SYSV2: /* 0x012FF7B6 local */
         *is_remote = 0;
         return("sysv2");
      case S_MAGIC_SYSV4: /* 0x012FF7B5 local */
         *is_remote = 0;
         return("sysv4");
      case S_MAGIC_TMPFS: /* 0x01021994 local */
         *is_remote = 0;
         return("tmpfs");
      case S_MAGIC_TRACEFS: /* 0x74726163 local */
         *is_remote = 0;
         return("tracefs");
      case S_MAGIC_UBIFS: /* 0x24051905 local */
         *is_remote = 0;
         return("ubifs");
      case S_MAGIC_UDF: /* 0x15013346 local */
         *is_remote = 0;
         return("udf");
      case S_MAGIC_UFS: /* 0x00011954 local */
         *is_remote = 0;
         return("ufs");
      case S_MAGIC_UFS_BYTESWAPPED: /* 0x54190100 local */
         *is_remote = 0;
         return("ufs");
      case S_MAGIC_USBDEVFS: /* 0x9FA2 local */
         *is_remote = 0;
         return("usbdevfs");
      case S_MAGIC_V9FS: /* 0x01021997 local */
         *is_remote = 0;
         return("v9fs");
      case S_MAGIC_VBOXSF: /* 0x786F4256 remote */
         *is_remote = 1;
         return("vboxsf");
      case S_MAGIC_VMHGFS: /* 0xBACBACBC remote */
         *is_remote = 1;
         return("vmhgfs");
      case S_MAGIC_VXFS: /* 0xA501FCF5 remote */
         *is_remote = 1;
         /* Veritas File System can run in single instance or clustered mode,
            so mark as remote to cater for the latter case.  */
         return("vxfs");
      case S_MAGIC_VZFS: /* 0x565A4653 local */
         *is_remote = 0;
         return("vzfs");
      case S_MAGIC_WSLFS: /* 0x53464846 local */
         *is_remote = 0;
         return("wslfs");
      case S_MAGIC_XENFS: /* 0xABBA1974 local */
         *is_remote = 0;
         return("xenfs");
      case S_MAGIC_XENIX: /* 0x012FF7B4 local */
         *is_remote = 0;
         return("xenix");
      case S_MAGIC_XFS: /* 0x58465342 local */
         *is_remote = 0;
         return("xfs");
      case S_MAGIC_XIAFS: /* 0x012FD16D local */
         *is_remote = 0;
         return("xia");
      case S_MAGIC_Z3FOLD: /* 0x0033 local */
         *is_remote = 0;
         return("z3fold");
      case S_MAGIC_ZFS: /* 0x2FC12FC1 local */
         *is_remote = 0;
         return("zfs");
      case S_MAGIC_ZONEFS: /* 0x5A4F4653 local */
         *is_remote = 0;
         return("zonefs");
      case S_MAGIC_ZSMALLOC: /* 0x58295829 local */
         *is_remote = 0;
         return("zsmallocfs");
      default:
         *is_remote = -1;
         return("unknown");
   }
#endif
}
