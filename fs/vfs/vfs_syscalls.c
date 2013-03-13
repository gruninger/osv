/*
 * Copyright (c) 2005-2007, Kohsuke Ohtani
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * vfs_syscalls.c - everything in this file is a routine implementing
 *                  a VFS system call.
 */

#include <sys/stat.h>
#include <dirent.h>

#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

#include <osv/prex.h>
#include <osv/vnode.h>
#include "vfs.h"

int
sys_open(char *path, int flags, mode_t mode, int *out_fd)
{
	vnode_t vp, dvp;
	file_t fp;
	int fd;
	char *filename;
	int error;

	DPRINTF(VFSDB_SYSCALL, ("sys_open: path=%s flags=%x mode=%x\n",
				path, flags, mode));

	flags = FFLAGS(flags);
	if  ((flags & (FREAD | FWRITE)) == 0)
		return EINVAL;
	if (flags & O_CREAT) {
		error = namei(path, &vp);
		if (error == ENOENT) {
			/* Create new file. */
			if ((error = lookup(path, &dvp, &filename)) != 0)
				return error;
			if ((error = vn_access(dvp, VWRITE)) != 0) {
				vput(dvp);
				return error;
			}
			mode &= ~S_IFMT;
			mode |= S_IFREG;
			error = VOP_CREATE(dvp, filename, mode);
			vput(dvp);
			if (error)
				return error;
			if ((error = namei(path, &vp)) != 0)
				return error;
			flags &= ~O_TRUNC;
		} else if (error) {
			return error;
		} else {
			/* File already exits */
			if (flags & O_EXCL) {
				vput(vp);
				return EEXIST;
			}
			flags &= ~O_CREAT;
		}
	} else {
		/* Open */
		if ((error = namei(path, &vp)) != 0)
			return error;
	}
	if ((flags & O_CREAT) == 0) {
		if (flags & FWRITE || flags & O_TRUNC) {
			if ((error = vn_access(vp, VWRITE)) != 0) {
				vput(vp);
				return error;
			}
			if (vp->v_type == VDIR) {
				/* Openning directory with writable. */
				vput(vp);
				return EISDIR;
			}
		}
	}
	/* Process truncate request */
	if (flags & O_TRUNC) {
		if (!(flags & FWRITE) || (vp->v_type == VDIR)) {
			vput(vp);
			return EINVAL;
		}
		if ((error = VOP_TRUNCATE(vp, 0)) != 0) {
			vput(vp);
			return error;
		}
	}

	/* Request to file system */
	if ((error = VOP_OPEN(vp, flags)) != 0) {
		vput(vp);
		return error;
	}

	falloc(&fp, &fd);
	finit(fp, flags, DTYPE_VNODE, NULL, &vfs_ops);
	fp->f_vnode = vp;

	*out_fd = fd;
	fdrop(fp);

	vn_unlock(vp);
	return 0;
}

int
sys_close(int fd)
{
    file_t fp;
    int error;

	DPRINTF(VFSDB_SYSCALL, ("sys_close: fp=%x count=%d\n",
				(u_int)fp, fp->f_count));

	error = fget(fd, &fp);
	if (error)
	    return error;

	if (fp->f_count <= 0)
		sys_panic("sys_close");

	/* Release one ref we got by calling fget(); */
	fdrop(fp);

	/* Release one ref count, invokes fo_close() in case there are no holders */
	fdrop(fp);

	return 0;
}

int
sys_read(int fd, struct iovec *iov, size_t niov,
		off_t offset, size_t *count)
{

	struct uio *uio = NULL;
	file_t fp;
	int error;

	DPRINTF(VFSDB_SYSCALL, ("sys_write: fp=%x buf=%x size=%d\n",
				(u_int)fp, (u_int)buf, size));

	error = fget(fd, &fp);
	if (error)
	    return error;

	if ((fp->f_flags & FREAD) == 0) {
	    fdrop(fp);
		return EBADF;
	}

	error = copyinuio(iov, niov, &uio);
	if (error) {
	    fdrop(fp);
		return error;
	}

	if (uio->uio_resid == 0) {
		*count = 0;
		fdrop(fp);
		return 0;
	}

	uio->uio_rw = UIO_READ;
	fo_read(fp, uio, (offset == -1) ? 0 : FOF_OFFSET);
    fdrop(fp);
	*count = uio->uio_resid;

	return error;
}

int
sys_write(int fd, struct iovec *iov, size_t niov,
		off_t offset, size_t *count)
{

	struct uio *uio = NULL;
	file_t fp;
	int ioflags = 0;
	int error;

	DPRINTF(VFSDB_SYSCALL, ("sys_write: fp=%x uio=%x niv=%zu\n",
				(u_long)fp, (u_long)uio, niv));

    error = fget(fd, &fp);
    if (error)
        return error;

	if ((fp->f_flags & FWRITE) == 0) {
	    fdrop(fp);
		return EBADF;
	}
	if (fp->f_flags & O_APPEND)
		ioflags |= IO_APPEND;

	error = copyinuio(iov, niov, &uio);
	if (error) {
	    fdrop(fp);
		return error;
	}

	if (uio->uio_resid == 0) {
		*count = 0;
		fdrop(fp);
		return 0;
	}

	uio->uio_rw = UIO_WRITE;
	fo_write(fp, uio, ioflags);
	fdrop(fp);
	*count = uio->uio_resid;

	return error;
}

int
sys_lseek(int fd, off_t off, int type, off_t *origin)
{
	vnode_t vp;
	file_t fp;
	int error;

	DPRINTF(VFSDB_SYSCALL, ("sys_seek: fp=%x off=%d type=%d\n",
				(u_int)fp, (u_int)off, type));

    error = fget(fd, &fp);
    if (error)
        return error;

	vp = fp->f_vnode;
	vn_lock(vp);
	switch (type) {
	case SEEK_SET:
		if (off < 0)
			off = 0;
		if (off > (off_t)vp->v_size)
			off = vp->v_size;
		break;
	case SEEK_CUR:
		if (fp->f_offset + off > (off_t)vp->v_size)
			off = vp->v_size;
		else if (fp->f_offset + off < 0)
			off = 0;
		else
			off = fp->f_offset + off;
		break;
	case SEEK_END:
		if (off > 0)
			off = vp->v_size;
		else if ((int)vp->v_size + off < 0)
			off = 0;
		else
			off = vp->v_size + off;
		break;
	default:
		vn_unlock(vp);
		fdrop(fp);
		return EINVAL;
	}
	/* Request to check the file offset */
	if (VOP_SEEK(vp, fp, fp->f_offset, off) != 0) {
		vn_unlock(vp);
		fdrop(fp);
		return EINVAL;
	}
	*origin = off;
	fp->f_offset = off;
	vn_unlock(vp);
	fdrop(fp);

	return 0;
}

int
sys_ioctl(int fd, u_long request, void *buf)
{
	file_t fp;
    int error;

	DPRINTF(VFSDB_SYSCALL, ("sys_ioctl: fp=%x request=%x\n", fp, request));

    error = fget(fd, &fp);
    if (error)
        return error;

	if ((fp->f_flags & (FREAD | FWRITE)) == 0) {
	    fdrop(fp);
		return EBADF;
	}

	error = fo_ioctl(fp, request, buf);
	fdrop(fp);

	DPRINTF(VFSDB_SYSCALL, ("sys_ioctl: comp error=%d\n", error));
	return error;
}

int
sys_fsync(int fd)
{
	vnode_t vp;
	file_t fp;
	int error;

	DPRINTF(VFSDB_SYSCALL, ("sys_fsync: fp=%x\n", fp));

    error = fget(fd, &fp);
    if (error)
        return error;

	if ((fp->f_flags & FWRITE) == 0) {
	    fdrop(fp);
		return EBADF;
	}

	vp = fp->f_vnode;
	vn_lock(vp);
	error = VOP_FSYNC(vp, fp);
	vn_unlock(vp);

	fdrop(fp);
	return error;
}

int
sys_fstat(int fd, struct stat *st)
{
	int error;
    file_t fp;

    DPRINTF(VFSDB_SYSCALL, ("sys_fstat: fd=%x\n", fd));

	error = fget(fd, &fp);
    if (error)
        return error;

	error = fo_stat(fp, st);

	fdrop(fp);

	return error;
}

/*
 * Return 0 if directory is empty
 */
static int
check_dir_empty(char *path)
{
	int error;
	int fd;
	struct dirent dir;

	DPRINTF(VFSDB_SYSCALL, ("check_dir_empty\n"));

	if ((error = sys_opendir(path, &fd)) != 0)
		return error;
	do {
		error = sys_readdir(fd, &dir);
		if (error != 0 && error != EACCES)
			break;
	} while (!strcmp(dir.d_name, ".") || !strcmp(dir.d_name, ".."));

	sys_closedir(fd);

	if (error == ENOENT)
		return 0;
	else if (error == 0)
		return EEXIST;
	return error;
}

int
sys_opendir(char *path, int *pfd)
{
	vnode_t dvp;
	file_t fp;
	int fd;
	int error;

	DPRINTF(VFSDB_SYSCALL, ("sys_opendir: path=%s\n", path));

	if ((error = sys_open(path, O_RDONLY, 0, &fd)) != 0)
		return error;

	error = fget(fd, &fp);
	if (error) {
	    sys_close(fd);
	    return error;
	}

	dvp = fp->f_vnode;
	vn_lock(dvp);
	if (dvp->v_type != VDIR) {
		vn_unlock(dvp);
		sys_close(fd);
		fdrop(fp);
		return ENOTDIR;
	}
	vn_unlock(dvp);
	fdrop(fp);

	*pfd = fd;
	return 0;
}

int
sys_closedir(int fd)
{
	vnode_t dvp;
	file_t fp;
	int error;

	DPRINTF(VFSDB_SYSCALL, ("sys_closedir: fd=%d\n", fd));

	error = fget(fd, &fp);
	if (error)
	    return error;

	dvp = fp->f_vnode;
	vn_lock(dvp);
	if (dvp->v_type != VDIR) {
	    vn_unlock(dvp);
	    fdrop(fp);
		return EBADF;
	}
	vn_unlock(dvp);
	fdrop(fp);
	error = sys_close(fd);
	return error;
}

int
sys_readdir(int fd, struct dirent *dir)
{
	vnode_t dvp;
	file_t fp;
	int error;

	DPRINTF(VFSDB_SYSCALL, ("sys_readdir: fd=%d\n", fd));

    error = fget(fd, &fp);
    if (error)
        return error;

	dvp = fp->f_vnode;
	vn_lock(dvp);
	if (dvp->v_type != VDIR) {
		vn_unlock(dvp);
		fdrop(fp);
		return EBADF;
	}
	error = VOP_READDIR(dvp, fp, dir);
	DPRINTF(VFSDB_SYSCALL, ("sys_readdir: error=%d path=%s\n",
				error, dir->d_name));
	vn_unlock(dvp);
	fdrop(fp);
	return error;
}

int
sys_rewinddir(int fd)
{
    vnode_t dvp;
    file_t fp;
    int error;

    DPRINTF(VFSDB_SYSCALL, ("sys_rewinddir: fd=%d\n", fd));

    error = fget(fd, &fp);
    if (error)
        return error;

	dvp = fp->f_vnode;
	vn_lock(dvp);
	if (dvp->v_type != VDIR) {
		vn_unlock(dvp);
		fdrop(fp);
		return EBADF;
	}
	fp->f_offset = 0;
	vn_unlock(dvp);
	fdrop(fp);
	return 0;
}

int
sys_seekdir(int fd, long loc)
{
    vnode_t dvp;
    file_t fp;
    int error;

    DPRINTF(VFSDB_SYSCALL, ("sys_seekdir: fd=%d\n", fd));

    error = fget(fd, &fp);
    if (error)
        return error;

	dvp = fp->f_vnode;
	vn_lock(dvp);
	if (dvp->v_type != VDIR) {
		vn_unlock(dvp);
		fdrop(fp);
		return EBADF;
	}
	fp->f_offset = (off_t)loc;
	vn_unlock(dvp);
	fdrop(fp);
	return 0;
}

int
sys_telldir(int fd, long *loc)
{
    vnode_t dvp;
    file_t fp;
    int error;

    DPRINTF(VFSDB_SYSCALL, ("sys_seekdir: fd=%d\n", fd));

    error = fget(fd, &fp);
    if (error)
        return error;

	dvp = fp->f_vnode;
	vn_lock(dvp);
	if (dvp->v_type != VDIR) {
		vn_unlock(dvp);
		fdrop(fp);
		return EBADF;
	}
	*loc = (long)fp->f_offset;
	vn_unlock(dvp);
	fdrop(fp);
	return 0;
}

int
sys_mkdir(char *path, mode_t mode)
{
	char *name;
	vnode_t vp, dvp;
	int error;

	DPRINTF(VFSDB_SYSCALL, ("sys_mkdir: path=%s mode=%d\n",	path, mode));

	if ((error = namei(path, &vp)) == 0) {
		/* File already exists */
		vput(vp);
		return EEXIST;
	}
	/* Notice: vp is invalid here! */

	if ((error = lookup(path, &dvp, &name)) != 0) {
		/* Directory already exists */
		return error;
	}
	if ((error = vn_access(dvp, VWRITE)) != 0)
		goto out;
	mode &= ~S_IFMT;
	mode |= S_IFDIR;

	error = VOP_MKDIR(dvp, name, mode);
 out:
	vput(dvp);
	return error;
}

int
sys_rmdir(char *path)
{
	vnode_t vp, dvp;
	int error;
	char *name;

	DPRINTF(VFSDB_SYSCALL, ("sys_rmdir: path=%s\n", path));

	if ((error = check_dir_empty(path)) != 0)
		return error;
	if ((error = namei(path, &vp)) != 0)
		return error;
	if ((error = vn_access(vp, VWRITE)) != 0)
		goto out;
	if (vp->v_type != VDIR) {
		error = ENOTDIR;
		goto out;
	}
	if (vp->v_flags & VROOT || vcount(vp) >= 2) {
		error = EBUSY;
		goto out;
	}
	if ((error = lookup(path, &dvp, &name)) != 0)
		goto out;

	error = VOP_RMDIR(dvp, vp, name);
	vn_unlock(vp);
	vgone(vp);
	vput(dvp);
	return error;

 out:
	vput(vp);
	return error;
}

int
sys_mknod(char *path, mode_t mode)
{
	char *name;
	vnode_t vp, dvp;
	int error;

	DPRINTF(VFSDB_SYSCALL, ("sys_mknod: path=%s mode=%d\n",	path, mode));

	switch (mode & S_IFMT) {
	case S_IFREG:
	case S_IFDIR:
	case S_IFIFO:
	case S_IFSOCK:
		/* OK */
		break;
	default:
		return EINVAL;
	}

	if ((error = namei(path, &vp)) == 0) {
		vput(vp);
		return EEXIST;
	}

	if ((error = lookup(path, &dvp, &name)) != 0)
		return error;
	if ((error = vn_access(dvp, VWRITE)) != 0)
		goto out;
	if (S_ISDIR(mode))
		error = VOP_MKDIR(dvp, name, mode);
	else
		error = VOP_CREATE(dvp, name, mode);
 out:
	vput(dvp);
	return error;
}

int
sys_rename(char *src, char *dest)
{
	vnode_t vp1, vp2 = 0, dvp1, dvp2;
	char *sname, *dname;
	int error;
	size_t len;
	char root[] = "/";

	DPRINTF(VFSDB_SYSCALL, ("sys_rename: src=%s dest=%s\n", src, dest));

	if ((error = namei(src, &vp1)) != 0)
		return error;
	if ((error = vn_access(vp1, VWRITE)) != 0)
		goto err1;

	/* If source and dest are the same, do nothing */
	if (!strncmp(src, dest, PATH_MAX))
		goto err1;

	/* Check if target is directory of source */
	len = strlen(dest);
	if (!strncmp(src, dest, len)) {
		error = EINVAL;
		goto err1;
	}
	/* Is the source busy ? */
	if (vcount(vp1) >= 2) {
		error = EBUSY;
		goto err1;
	}
	/* Check type of source & target */
	error = namei(dest, &vp2);
	if (error == 0) {
		/* target exists */
		if (vp1->v_type == VDIR && vp2->v_type != VDIR) {
			error = ENOTDIR;
			goto err2;
		} else if (vp1->v_type != VDIR && vp2->v_type == VDIR) {
			error = EISDIR;
			goto err2;
		}
		if (vp2->v_type == VDIR && check_dir_empty(dest)) {
			error = EEXIST;
			goto err2;
		}

		if (vcount(vp2) >= 2) {
			error = EBUSY;
			goto err2;
		}
	}

	dname = strrchr(dest, '/');
	if (dname == NULL) {
		error = ENOTDIR;
		goto err2;
	}
	if (dname == dest)
		dest = root;

	*dname = 0;
	dname++;

	if ((error = lookup(src, &dvp1, &sname)) != 0)
		goto err2;

	if ((error = namei(dest, &dvp2)) != 0)
		goto err3;

	/* The source and dest must be same file system */
	if (dvp1->v_mount != dvp2->v_mount) {
		error = EXDEV;
		goto err4;
	}
	error = VOP_RENAME(dvp1, vp1, sname, dvp2, vp2, dname);
 err4:
	vput(dvp2);
 err3:
	vput(dvp1);
 err2:
	if (vp2)
		vput(vp2);
 err1:
	vput(vp1);
	return error;
}

int
sys_unlink(char *path)
{
	char *name;
	vnode_t vp, dvp;
	int error;

	DPRINTF(VFSDB_SYSCALL, ("sys_unlink: path=%s\n", path));

	if ((error = namei(path, &vp)) != 0)
		return error;
	if ((error = vn_access(vp, VWRITE)) != 0)
		goto out;
	if (vp->v_type == VDIR) {
		error = EPERM;
		goto out;
	}
	/* XXX: Need to allow unlink for opened file. */
	if (vp->v_flags & VROOT || vcount(vp) >= 2) {
		error = EBUSY;
		goto out;
	}
	if ((error = lookup(path, &dvp, &name)) != 0)
		goto out;

	error = VOP_REMOVE(dvp, vp, name);

	vn_unlock(vp);
	vgone(vp);
	vput(dvp);
	return 0;
 out:
	vput(vp);
	return error;
}

int
sys_access(char *path, int mode)
{
	vnode_t vp;
	int error, flags;

	DPRINTF(VFSDB_SYSCALL, ("sys_access: path=%s mode=%x\n", path, mode));

	/* If F_OK is set, we return here if file is not found. */
	if ((error = namei(path, &vp)) != 0)
		return error;

	flags = 0;
	if (mode & R_OK)
		flags |= VREAD;
	if (mode & W_OK)
		flags |= VWRITE;
	if (mode & X_OK)
		flags |= VEXEC;

	error = vn_access(vp, flags);

	vput(vp);
	return error;
}

int
sys_stat(char *path, struct stat *st)
{
	vnode_t vp;
	int error;

	DPRINTF(VFSDB_SYSCALL, ("sys_stat: path=%s\n", path));

	if ((error = namei(path, &vp)) != 0)
		return error;
	error = vn_stat(vp, st);
	vput(vp);
	return error;
}

int
sys_statfs(char *path, struct statfs *buf)
{
	struct vnode *vp;
	int error;

	memset(buf, 0, sizeof(*buf));

	error = namei(path, &vp);
	if (error)
		return error;

	error = VFS_STATFS(vp->v_mount, buf);
	vput(vp);

	return error;
}

int
sys_fstatfs(int fd, struct statfs *buf)
{
    file_t fp;
	struct vnode *vp;
	int error;

	memset(buf, 0, sizeof(*buf));

	error = fget(fd, &fp);
	if (error)
	    return error;

	vp = fp->f_vnode;

	vn_lock(vp);
	error = VFS_STATFS(vp->v_mount, buf);
	vn_unlock(vp);

	return error;
}

int
sys_truncate(char *path, off_t length)
{
	return 0;
}

int
sys_ftruncate(int fd, off_t length)
{
	return 0;
}

int
sys_fchdir(file_t fp, char *cwd)
{
	vnode_t dvp;

	dvp = fp->f_vnode;
	vn_lock(dvp);
	if (dvp->v_type != VDIR) {
		vn_unlock(dvp);
		return EBADF;
	}
	strlcpy(cwd, dvp->v_path, PATH_MAX);
	vn_unlock(dvp);
	return 0;
}

ssize_t
sys_readlink(char *path, char *buf, size_t bufsize)
{
	int error;
	struct vnode *vp;

	error = namei(path, &vp);
	if (error)
		return error;

	/* no symlink support (yet) in OSv */
	vput(vp);
	return EINVAL;
}

int
sys_dup(int fd, int* out_fd)
{
    file_t fp;
    int new_fd;
    int error;

    error = fget(fd, &fp);
    if (error)
        return error;

    new_fd = fdalloc(fp);

    if (fp->f_type == DTYPE_VNODE) {
        vref(fp->f_vnode);
    }

    /* Don't call fdrop(), refcount is increased by fget() */

    *out_fd = new_fd;
    return 0;

}

int
sys_dup3(int fd, int new_fd)
{
    file_t fp;
    int error;

    error = fget(fd, &fp);
    if (error)
        return error;

    error = fdset(new_fd, fp);
    if (error)
        return error;

    if (fp->f_type == DTYPE_VNODE) {
        vref(fp->f_vnode);
    }

    /* Don't call fdrop(), refcount is increased by fget() */

    return 0;

}
