//
//  Filesystem.c
//  Apollo
//
//  Created by Dietmar Planitzer on 11/07/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Filesystem.h"


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Path Component
////////////////////////////////////////////////////////////////////////////////

// Initializes a path component from a NUL-terminated string
PathComponent PathComponent_MakeFromCString(const Character* _Nonnull pCString)
{
    PathComponent pc;

    pc.name = pCString;
    pc.count = String_Length(pCString);
    return pc;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: File
////////////////////////////////////////////////////////////////////////////////

// Creates a file object.
ErrorCode File_Create(FilesystemRef _Nonnull pFilesystem, UInt mode, InodeRef _Nonnull pNode, FileRef _Nullable * _Nonnull pOutFile)
{
    decl_try_err();
    FileRef pFile;

    try(IOChannel_AbstractCreate(&kFileClass, (IOResourceRef)pFilesystem, mode, (IOChannelRef*)&pFile));
    pFile->inode = Inode_ReacquireUnlocked(pNode);
    pFile->offset = 0ll;

catch:
    *pOutFile = pFile;
    return err;
}

// Creates a copy of the given file.
ErrorCode File_CreateCopy(FileRef _Nonnull pInFile, FileRef _Nullable * _Nonnull pOutFile)
{
    decl_try_err();
    FileRef pNewFile;

    try(IOChannel_AbstractCreateCopy((IOChannelRef)pInFile, (IOChannelRef*)&pNewFile));
    pNewFile->inode = Inode_ReacquireUnlocked(pInFile->inode);
    pNewFile->offset = pInFile->offset;

catch:
    *pOutFile = pNewFile;
    return err;
}

void File_deinit(FileRef _Nonnull self)
{
    if (self->inode) {
        Inode_Relinquish(self->inode);
        self->inode = NULL;
    }
}

ErrorCode File_seek(FileRef _Nonnull self, FileOffset offset, FileOffset* _Nullable pOutOldPosition, Int whence)
{
    if(pOutOldPosition) {
        *pOutOldPosition = self->offset;
    }

    FileOffset newOffset;

    switch (whence) {
        case SEEK_SET:
            newOffset = offset;
            break;

        case SEEK_CUR:
            newOffset = self->offset + offset;
            break;

        case SEEK_END:
            newOffset = Inode_GetFileSize(self->inode) + offset;
            break;

        default:
            return EINVAL;
    }

    if (newOffset < 0) {
        return EINVAL;
    }
    // XXX do overflow check

    self->offset = newOffset;
    return EOK;
}

CLASS_METHODS(File, IOChannel,
OVERRIDE_METHOD_IMPL(deinit, File, Object)
OVERRIDE_METHOD_IMPL(seek, File, IOChannel)
);


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Directory
////////////////////////////////////////////////////////////////////////////////

// Creates a directory object.
ErrorCode Directory_Create(FilesystemRef _Nonnull pFilesystem, InodeRef _Nonnull pNode, DirectoryRef _Nullable * _Nonnull pOutDir)
{
    decl_try_err();
    DirectoryRef pDir;

    try(IOChannel_AbstractCreate(&kDirectoryClass, (IOResourceRef)pFilesystem, FREAD, (IOChannelRef*)&pDir));
    pDir->inode = Inode_ReacquireUnlocked(pNode);
    pDir->offset = 0ll;

catch:
    *pOutDir = pDir;
    return err;
}

// Creates a copy of the given directory descriptor.
ErrorCode Directory_CreateCopy(DirectoryRef _Nonnull pInDir, DirectoryRef _Nullable * _Nonnull pOutDir)
{
    decl_try_err();
    DirectoryRef pNewDir;

    try(IOChannel_AbstractCreateCopy((IOChannelRef)pInDir, (IOChannelRef*)&pNewDir));
    pNewDir->inode = Inode_ReacquireUnlocked(pInDir->inode);
    pNewDir->offset = pInDir->offset;

catch:
    *pOutDir = pNewDir;
    return err;
}

void Directory_deinit(DirectoryRef _Nonnull self)
{
    if (self->inode) {
        Inode_Relinquish(self->inode);
        self->inode = NULL;
    }
}

ByteCount Directory_dup(DirectoryRef _Nonnull self, DirectoryRef _Nullable * _Nonnull pOutDir)
{
    return EBADF;
}

ByteCount Directory_read(DirectoryRef _Nonnull self, Byte* _Nonnull pBuffer, ByteCount nBytesToRead)
{
    return Filesystem_ReadDirectory(IOChannel_GetResource(self), self, pBuffer, nBytesToRead);
}

ByteCount Directory_write(DirectoryRef _Nonnull self, const Byte* _Nonnull pBuffer, ByteCount nBytesToWrite)
{
    return EBADF;
}

ErrorCode Directory_seek(DirectoryRef _Nonnull self, FileOffset offset, FileOffset* _Nullable pOutOldPosition, Int whence)
{
    
    if(pOutOldPosition) {
        *pOutOldPosition = self->offset;
    }
    if (whence != SEEK_SET || offset < 0) {
        return EINVAL;
    }
    if (offset > (FileOffset)INT_MAX) {
        return EOVERFLOW;
    }

    self->offset = offset;
    return EOK;
}

ErrorCode Directory_close(DirectoryRef _Nonnull self)
{
    return Filesystem_CloseDirectory(IOChannel_GetResource(self), self);
}

CLASS_METHODS(Directory, IOChannel,
OVERRIDE_METHOD_IMPL(deinit, Directory, Object)
OVERRIDE_METHOD_IMPL(dup, Directory, IOChannel)
OVERRIDE_METHOD_IMPL(read, Directory, IOChannel)
OVERRIDE_METHOD_IMPL(write, Directory, IOChannel)
OVERRIDE_METHOD_IMPL(seek, Directory, IOChannel)
OVERRIDE_METHOD_IMPL(close, Directory, IOChannel)
);


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Filesystem
////////////////////////////////////////////////////////////////////////////////

// Returns the next available FSID.
static FilesystemId Filesystem_GetNextAvailableId(void)
{
    // XXX want to:
    // XXX handle overflow (wrap around)
    // XXX make sure the generated id isn't actually in use by someone else
    static volatile AtomicInt gNextAvailableId = 0;
    return (FilesystemId) AtomicInt_Increment(&gNextAvailableId);
}

// Creates an instance of a filesystem subclass. Users of a concrete filesystem
// should not use this function to allocate an instance of the concrete filesystem.
// This function is for use by Filesystem subclassers to define the filesystem
// specific instance allocation function.
ErrorCode Filesystem_Create(ClassRef pClass, FilesystemRef _Nullable * _Nonnull pOutFileSys)
{
    decl_try_err();
    FilesystemRef self;

    try(_Object_Create(pClass, 0, (ObjectRef*)&self));
    self->fsid = Filesystem_GetNextAvailableId();
    Lock_Init(&self->inodeManagementLock);
    PointerArray_Init(&self->inodesInUse, 16);

    *pOutFileSys = self;
    return EOK;

catch:
    *pOutFileSys = NULL;
    return err;
}

void Filesystem_deinit(FilesystemRef _Nonnull self)
{
    PointerArray_Deinit(&self->inodesInUse);
    Lock_Deinit(&self->inodeManagementLock);
}

// Acquires the inode with the ID 'id'. The node is returned in a locked state.
// This methods guarantees that there will always only be at most one inode instance
// in memory at any given time and that only one VP can access/modify the inode.
// Once you're done with the inode, you should relinquish it back to the filesystem.
// This method should be used by subclassers to acquire inodes in order to return
// them to a filesystem user.
// This method calls the filesystem method onReadNodeFromDisk() to read the
// requested inode off the disk if there is no inode instance in memory at the
// time this method is called.
ErrorCode Filesystem_AcquireNodeWithId(FilesystemRef _Nonnull self, InodeId id, void* _Nullable pContext, InodeRef _Nullable _Locked * _Nonnull pOutNode)
{
    decl_try_err();
    InodeRef pNode = NULL;

    Lock_Lock(&self->inodeManagementLock);

    for (Int i = 0; i < PointerArray_GetCount(&self->inodesInUse); i++) {
        InodeRef pCurNode = (InodeRef)PointerArray_GetAt(&self->inodesInUse, i);

        if (Inode_GetId(pCurNode) == id) {
            pNode = pCurNode;
            break;
        }
    }

    if (pNode == NULL) {
        try(Filesystem_OnReadNodeFromDisk(self, id, pContext, &pNode));
        try(PointerArray_Add(&self->inodesInUse, pNode));
    }

    pNode->useCount++;
    //XXX Inode_Lock(pNode);
    Lock_Unlock(&self->inodeManagementLock);
    *pOutNode = pNode;

    return EOK;

catch:
    Lock_Unlock(&self->inodeManagementLock);
    *pOutNode = NULL;
    return err;
}

// Acquires a new reference to the given node. The returned node is locked.
InodeRef _Nonnull _Locked Filesystem_ReacquireNode(FilesystemRef _Nonnull self, InodeRef _Nonnull pNode)
{
    InodeRef pOutNode;

    try_bang(Filesystem_AcquireNodeWithId(self, Inode_GetId(pNode), NULL, &pOutNode));
    return pOutNode;
}

// Acquires a new reference to the given node. The returned node is NOT locked.
InodeRef _Nonnull Filesystem_ReacquireUnlockedNode(FilesystemRef _Nonnull self, InodeRef _Nonnull pNode)
{
    InodeRef pOutNode;

    try_bang(Filesystem_AcquireNodeWithId(self, Inode_GetId(pNode), NULL, &pOutNode));
    //XXX Inode_Unlock(pOutNode);
    return pOutNode;
}

// Relinquishes the given node back to the filesystem. This method will invoke
// the filesystem onRemoveNodeFromDisk() if no directory is referencing the inode
// anymore. This will remove the inode from disk.
void Filesystem_RelinquishNode(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pNode)
{
    Lock_Lock(&self->inodeManagementLock);
    assert(pNode->useCount > 0);
    pNode->useCount--;

    assert(pNode->linkCount >= 0);
    if (pNode->linkCount == 0) {
        Filesystem_OnRemoveNodeFromDisk(self, Inode_GetId(pNode));
    }
    //XXX Inode_Unlock(pNode);

    if (pNode->useCount == 0) {
        PointerArray_Remove(&self->inodesInUse, pNode);
        Inode_Destroy(pNode);
    }

    Lock_Unlock(&self->inodeManagementLock);
}

// Returns true if the filesystem can be safely unmounted which means that no
// inodes owned by the filesystem is currently in memory.
Bool Filesystem_CanSafelyUnmount(FilesystemRef _Nonnull self)
{
    Lock_Lock(&self->inodeManagementLock);
    const Bool ok = PointerArray_IsEmpty(&self->inodesInUse);
    Lock_Unlock(&self->inodeManagementLock);
    return ok;
}

// Invoked when Filesystem_AcquireNodeWithId() needs to read the requested inode
// off the disk. The override should read the inode data from the disk,
// create and inode instance and fill it in with the data from the disk and
// then return it. It should return a suitable error and NULL if the inode
// data can not be read off the disk.
ErrorCode Filesystem_onReadNodeFromDisk(FilesystemRef _Nonnull self, InodeId id, void* _Nullable pContext, InodeRef _Nullable * _Nonnull pOutNode)
{
    return EIO;
}

// Invoked when Filesystem_RelinquishNode() has determined that the inode is
// no longer being referenced by any directory and that the on-disk
// representation should be deleted from the disk and deallocated. This
// operation is assumed to never fail.
void Filesystem_onRemoveNodeFromDisk(FilesystemRef _Nonnull self, InodeId id)
{
}


// Invoked when an instance of this file system is mounted. Note that the
// kernel guarantees that no operations will be issued to the filesystem
// before onMount() has returned with EOK.
ErrorCode Filesystem_onMount(FilesystemRef _Nonnull self, const Byte* _Nonnull pParams, ByteCount paramsSize)
{
    return EIO;
}

// Invoked when a mounted instance of this file system is unmounted. A file
// system may return an error. Note however that this error is purely advisory
// and the file system implementation is required to do everything it can to
// successfully unmount. Unmount errors are ignored and the file system manager
// will complete the unmount in any case.
ErrorCode Filesystem_onUnmount(FilesystemRef _Nonnull self)
{
    return EOK;
}


// Returns the root node of the filesystem if the filesystem is currently in
// mounted state. Returns ENOENT and NULL if the filesystem is not mounted.
ErrorCode Filesystem_acquireRootNode(FilesystemRef _Nonnull self, InodeRef _Nullable _Locked * _Nonnull pOutNode)
{
    *pOutNode = NULL;
    return ENOENT;
}

// Returns EOK and the node that corresponds to the tuple (parent-node, name),
// if that node exists. Otherwise returns ENOENT and NULL.  Note that this
// function has to support the special names "." (node itself) and ".."
// (parent of node) in addition to "regular" filenames. If 'pParentNode' is
// the root node of the filesystem and 'pComponent' is ".." then 'pParentNode'
// should be returned. If the path component name is longer than what is
// supported by the file system, ENAMETOOLONG should be returned.
ErrorCode Filesystem_acquireNodeForName(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pParentNode, const PathComponent* _Nonnull pComponent, User user, InodeRef _Nullable _Locked * _Nonnull pOutNode)
{
    *pOutNode = NULL;
    return ENOENT;
}

// Returns the name of the node with the id 'id' which a child of the
// directory node 'pParentNode'. 'id' may be of any type. The name is
// returned in the mutable path component 'pComponent'. 'count' in path
// component is 0 on entry and should be set to the actual length of the
// name on exit. The function is expected to return EOK if the parent node
// contains 'id' and ENOENT otherwise. If the name of 'id' as stored in the
// file system is > the capacity of the path component, then ERANGE should
// be returned.
ErrorCode Filesystem_getNameOfNode(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pParentNode, InodeId id, User user, MutablePathComponent* _Nonnull pComponent)
{
    pComponent->count = 0;
    return ENOENT;
}

// Returns a file info record for the given Inode. The Inode may be of any
// file type.
ErrorCode Filesystem_getFileInfo(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pNode, FileInfo* _Nonnull pOutInfo)
{
    return EIO;
}

// Modifies one or more attributes stored in the file info record of the given
// Inode. The Inode may be of any type.
ErrorCode Filesystem_setFileInfo(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pNode, User user, MutableFileInfo* _Nonnull pInfo)
{
    return EIO;
}

// Creates an empty directory as a child of the given directory node and with
// the given name, user and file permissions. Returns EEXIST if a node with
// the given name already exists.
ErrorCode Filesystem_createDirectory(FilesystemRef _Nonnull self, const PathComponent* _Nonnull pName, InodeRef _Nonnull _Locked pParentNode, User user, FilePermissions permissions)
{
    return EACCESS;
}

// Opens the directory represented by the given node. Returns a directory
// descriptor object which is teh I/O channel that allows you to read the
// directory content.
ErrorCode Filesystem_openDirectory(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pDirNode, User user, DirectoryRef _Nullable * _Nonnull pOutDir)
{
    return EACCESS;
}

// Reads the next set of directory entries. The first entry read is the one
// at the current directory index stored in 'pDir'. This function guarantees
// that it will only ever return complete directories entries. It will never
// return a partial entry. Consequently the provided buffer must be big enough
// to hold at least one directory entry. Note that this function is expected
// to return "." for the entry at index #0 and ".." for the entry at index #1.
ByteCount Filesystem_readDirectory(FilesystemRef _Nonnull self, DirectoryRef _Nonnull pDir, Byte* _Nonnull pBuffer, ByteCount nBytesToRead)
{
    return -EIO;
}

// Closes the given directory I/O channel.
ErrorCode Filesystem_closeDirectory(FilesystemRef _Nonnull self, DirectoryRef _Nonnull pDir)
{
    Object_Release(pDir);
    return EOK;
}

// Verifies that the given node is accessible assuming the given access mode.
ErrorCode Filesystem_checkAccess(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pNode, User user, Int mode)
{
    return EACCESS;
}

// Unlink the node identified by the path component 'pName' and which is an
// immediate child of the (directory) node 'pParentNode'. The parent node is
// guaranteed to be a node owned by the filesystem.
// This function must validate that a directory entry with the given name
// actually exists, is a file or an empty directory before it attempts to
// remove the entry from the parent node.
ErrorCode Filesystem_unlink(FilesystemRef _Nonnull self, const PathComponent* _Nonnull pName, InodeRef _Nonnull _Locked pParentNode, User user)
{
    return EACCESS;
}

// Renames the node with name 'pName' and which is an immediate child of the
// node 'pParentNode' such that it becomes a child of 'pNewParentNode' with
// the name 'pNewName'. All nodes are guaranteed to be owned by the filesystem.
ErrorCode Filesystem_rename(FilesystemRef _Nonnull self, const PathComponent* _Nonnull pName, InodeRef _Nonnull _Locked pParentNode, const PathComponent* _Nonnull pNewName, InodeRef _Nonnull _Locked pNewParentNode, User user)
{
    return EACCESS;
}


CLASS_METHODS(Filesystem, IOResource,
OVERRIDE_METHOD_IMPL(deinit, Filesystem, Object)
METHOD_IMPL(onReadNodeFromDisk, Filesystem)
METHOD_IMPL(onRemoveNodeFromDisk, Filesystem)
METHOD_IMPL(onMount, Filesystem)
METHOD_IMPL(onUnmount, Filesystem)
METHOD_IMPL(acquireRootNode, Filesystem)
METHOD_IMPL(acquireNodeForName, Filesystem)
METHOD_IMPL(getNameOfNode, Filesystem)
METHOD_IMPL(getFileInfo, Filesystem)
METHOD_IMPL(setFileInfo, Filesystem)
METHOD_IMPL(createDirectory, Filesystem)
METHOD_IMPL(openDirectory, Filesystem)
METHOD_IMPL(readDirectory, Filesystem)
METHOD_IMPL(closeDirectory, Filesystem)
METHOD_IMPL(checkAccess, Filesystem)
METHOD_IMPL(unlink, Filesystem)
METHOD_IMPL(rename, Filesystem)
);
