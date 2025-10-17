#ifndef VFS_H
#define VFS_H

#include <stddef.h>
#include <stdint.h>

// --- Forward declarations ---
typedef struct file file_t;
typedef struct vnode vnode_t;
typedef struct vfs_ops vfs_ops_t;
typedef struct mount mount_t;

// --- File structure ---
struct file {
    vnode_t *vnode;
    void *fs_data;   // FS-specific data
    size_t pos;      // Current read/write position
};

// --- Vnode structure ---
struct vnode {
    char *name;
    vfs_ops_t *ops;
    void *fs_node;          // FS-specific data
    vnode_t **children;     // Child nodes for directories
    size_t child_count;
};

// --- File operations (vtable) ---
struct vfs_ops {
    int (*read)(file_t *file, void *buf, size_t len);
    int (*write)(file_t *file, const void *buf, size_t len);
    int (*open)(file_t *file);
    int (*close)(file_t *file);
    vnode_t* (*lookup)(vnode_t *dir, const char *name);
};

// --- Mount point ---
struct mount {
    vnode_t *root;
    void *fs_info;  // FS-specific info (like superblock)
};

// --- VFS API ---
void vfs_init(void);
void vfs_mount(mount_t *mnt);
file_t* vfs_open(const char *path);
int vfs_read(file_t *file, void *buf, size_t len);
int vfs_write(file_t *file, const void *buf, size_t len);
int vfs_close(file_t *file);

#endif
