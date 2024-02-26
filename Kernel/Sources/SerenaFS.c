//
//  SerenaFS.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/11/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "SerenaFSPriv.h"
#include "MonotonicClock.h"


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Inode extensions
////////////////////////////////////////////////////////////////////////////////

// Returns true if the given directory node is empty (contains just "." and "..").
static bool DirectoryNode_IsEmpty(InodeRef _Nonnull _Locked self)
{
    return Inode_GetFileSize(self) <= sizeof(RamDirectoryEntry) * 2;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Filesystem
////////////////////////////////////////////////////////////////////////////////

// Creates an instance of SerenaFS. SerenaFS is a volatile file system that does not
// survive system restarts. The 'rootDirUser' parameter specifies the user and
// group ID of the root directory.
errno_t SerenaFS_Create(User rootDirUser, SerenaFSRef _Nullable * _Nonnull pOutFileSys)
{
    decl_try_err();
    SerenaFSRef self;

    assert(sizeof(RamDiskNode) <= kRamBlockSize);
    assert(sizeof(RamDirectoryEntry) * kRamDirectoryEntriesPerBlock == kRamBlockSize);
    
    try(Filesystem_Create(&kSerenaFSClass, (FilesystemRef*)&self));
    Lock_Init(&self->lock);
    ConditionVariable_Init(&self->notifier);
    PointerArray_Init(&self->dnodes, 16);
    self->rootDirUser = rootDirUser;
    self->nextAvailableInodeId = 1;
    self->isMounted = false;
    self->isReadOnly = false;

    try(SerenaFS_FormatWithEmptyFilesystem(self));

    *pOutFileSys = self;
    return EOK;

catch:
    *pOutFileSys = NULL;
    return err;
}

void SerenaFS_deinit(SerenaFSRef _Nonnull self)
{
    for(int i = 0; i < PointerArray_GetCount(&self->dnodes); i++) {
        SerenaFS_DestroyDiskNode(self, PointerArray_GetAtAs(&self->dnodes, i, RamDiskNodeRef));
    }
    PointerArray_Deinit(&self->dnodes);
    ConditionVariable_Deinit(&self->notifier);
    Lock_Deinit(&self->lock);
}

static errno_t SerenaFS_FormatWithEmptyFilesystem(SerenaFSRef _Nonnull self)
{
    decl_try_err();
    const FilePermissions ownerPerms = kFilePermission_Read | kFilePermission_Write | kFilePermission_Execute;
    const FilePermissions otherPerms = kFilePermission_Read | kFilePermission_Execute;
    const FilePermissions dirPerms = FilePermissions_Make(ownerPerms, otherPerms, otherPerms);

    try(SerenaFS_CreateDirectoryDiskNode(self, 0, self->rootDirUser.uid, self->rootDirUser.gid, dirPerms, &self->rootDirId));
    return EOK;

catch:
    return err;
}

static int SerenaFS_GetIndexOfDiskNodeForId(SerenaFSRef _Nonnull self, InodeId id)
{
    for (int i = 0; i < PointerArray_GetCount(&self->dnodes); i++) {
        RamDiskNodeRef pCurDiskNode = PointerArray_GetAtAs(&self->dnodes, i, RamDiskNodeRef);

        if (pCurDiskNode->id == id) {
            return i;
        }
    }
    return -1;
}

// Invoked when Filesystem_AllocateNode() is called. Subclassers should
// override this method to allocate the on-disk representation of an inode
// of the given type.
errno_t SerenaFS_onAllocateNodeOnDisk(SerenaFSRef _Nonnull self, FileType type, void* _Nullable pContext, InodeId* _Nonnull pOutId)
{
    decl_try_err();
    const InodeId id = (InodeId) self->nextAvailableInodeId++;
    RamDiskNodeRef pDiskNode = NULL;

    try(kalloc_cleared(sizeof(RamDiskNode), (void**)&pDiskNode));
    pDiskNode->id = id;
    pDiskNode->linkCount = 1;
    pDiskNode->type = type;

    try(PointerArray_Add(&self->dnodes, pDiskNode));
    *pOutId = id;

    return EOK;

catch:
    SerenaFS_DestroyDiskNode(self, pDiskNode);
    *pOutId = 0;
    return err;
}

static void SerenaFS_DestroyDiskNode(SerenaFSRef _Nonnull self, RamDiskNodeRef _Nullable pDiskNode)
{
    if (pDiskNode) {
        for (int i = 0; i < kMaxDirectDataBlockPointers; i++) {
            kfree(pDiskNode->blockMap.p[i]);
            pDiskNode->blockMap.p[i] = NULL;
        }
        kfree(pDiskNode);
    }
}

// Invoked when Filesystem_AcquireNodeWithId() needs to read the requested inode
// off the disk. The override should read the inode data from the disk,
// create and inode instance and fill it in with the data from the disk and
// then return it. It should return a suitable error and NULL if the inode
// data can not be read off the disk.
errno_t SerenaFS_onReadNodeFromDisk(SerenaFSRef _Nonnull self, InodeId id, void* _Nullable pContext, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    const int dIdx = SerenaFS_GetIndexOfDiskNodeForId(self, id);
    if (dIdx < 0) throw(ENOENT);
    const RamDiskNodeRef pDiskNode = PointerArray_GetAtAs(&self->dnodes, dIdx, RamDiskNodeRef);

    return Inode_Create(
        Filesystem_GetId(self),
        id,
        pDiskNode->type,
        pDiskNode->linkCount,
        pDiskNode->uid,
        pDiskNode->gid,
        pDiskNode->permissions,
        pDiskNode->size,
        pDiskNode->accessTime,
        pDiskNode->modificationTime,
        pDiskNode->statusChangeTime,
        &pDiskNode->blockMap,
        pOutNode);

catch:
    return err;
}

// Invoked when the inode is relinquished and it is marked as modified. The
// filesystem override should write the inode meta-data back to the 
// corresponding disk node.
errno_t SerenaFS_onWriteNodeToDisk(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode)
{
    decl_try_err();
    const int dIdx = SerenaFS_GetIndexOfDiskNodeForId(self, Inode_GetId(pNode));
    if (dIdx < 0) throw(ENOENT);
    RamDiskNodeRef pDiskNode = PointerArray_GetAtAs(&self->dnodes, dIdx, RamDiskNodeRef);
    const TimeInterval curTime = MonotonicClock_GetCurrentTime();

    if (Inode_IsAccessed(pNode)) {
        pDiskNode->accessTime = curTime;
    }
    if (Inode_IsUpdated(pNode)) {
        pDiskNode->accessTime = curTime;
    }
    if (Inode_IsStatusChanged(pNode)) {
        pDiskNode->statusChangeTime = curTime;
    }
    pDiskNode->size = Inode_GetFileSize(pNode);
    pDiskNode->linkCount = Inode_GetLinkCount(pNode);
    pDiskNode->uid = Inode_GetUserId(pNode);
    pDiskNode->gid = Inode_GetGroupId(pNode);
    pDiskNode->permissions = Inode_GetFilePermissions(pNode);
    return EOK;

catch:
    return err;
}

// Invoked when Filesystem_RelinquishNode() has determined that the inode is
// no longer being referenced by any directory and that the on-disk
// representation should be deleted from the disk and deallocated. This
// operation is assumed to never fail.
void SerenaFS_onRemoveNodeFromDisk(SerenaFSRef _Nonnull self, InodeId id)
{
    const int dIdx = SerenaFS_GetIndexOfDiskNodeForId(self, id);

    if (dIdx >= 0) {
        RamDiskNodeRef pDiskNode = PointerArray_GetAtAs(&self->dnodes, dIdx, RamDiskNodeRef);

        SerenaFS_DestroyDiskNode(self, pDiskNode);
        PointerArray_RemoveAt(&self->dnodes, dIdx);
    }
}

// Checks whether the given user should be granted access to the given node based
// on the requested permission. Returns EOK if access should be granted and a suitable
// error code if it should be denied.
static errno_t SerenaFS_CheckAccess_Locked(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, User user, AccessMode mode)
{
    if (mode == kFilePermission_Write) {
        if (self->isReadOnly) {
            return EROFS;
        }

        // XXX once we support actual text mapping, we'll need to check whether the text file is in use
    }

    FilePermissions permissions;
    switch (mode) {
        case kAccess_Readable:      permissions = FilePermissions_Make(kFilePermission_Read, 0, 0); break;
        case kAccess_Writable:      permissions = FilePermissions_Make(kFilePermission_Write, 0, 0); break;
        case kAccess_Executable:    permissions = FilePermissions_Make(kFilePermission_Execute, 0, 0); break;
        default:                    permissions = 0; break;
    }
    return Inode_CheckAccess(pNode, user, permissions);
}

// Returns true if the array of directory entries starting at 'pEntry' and holding
// 'nEntries' entries contains a directory entry that matches 'pQuery'.
static bool xHasMatchingDirectoryEntry(const RamDirectoryQuery* _Nonnull pQuery, const RamDirectoryEntry* _Nonnull pEntry, int nEntries, RamDirectoryEntry* _Nullable * _Nullable pOutEmptyPtr, RamDirectoryEntry* _Nullable * _Nonnull pOutEntryPtr)
{
    while (nEntries-- > 0) {
        if (pEntry->id > 0) {
            switch (pQuery->kind) {
                case kDirectoryQuery_PathComponent:
                    if (PathComponent_EqualsString(pQuery->u.pc, pEntry->filename)) {
                        *pOutEntryPtr = (RamDirectoryEntry*)pEntry;
                        return true;
                    }
                    break;

                case kDirectoryQuery_InodeId:
                    if (pEntry->id == pQuery->u.id) {
                       *pOutEntryPtr = (RamDirectoryEntry*)pEntry;
                        return true;
                    }
                    break;

                default:
                    abort();
            }
        }
        else if (pOutEmptyPtr) {
            *pOutEmptyPtr = (RamDirectoryEntry*)pEntry;
        }
        pEntry++;
    }

    return false;
}

// Returns a reference to the directory entry that holds 'pName'. NULL and a
// suitable error is returned if no such entry exists or 'pName' is empty or
// too long.
static errno_t SerenaFS_GetDirectoryEntry(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, const RamDirectoryQuery* _Nonnull pQuery, RamDirectoryEntry* _Nullable * _Nullable pOutEmptyPtr, RamDirectoryEntry* _Nullable * _Nonnull pOutEntryPtr)
{
    decl_try_err();
    const FileOffset fileSize = Inode_GetFileSize(pNode);
    FileOffset offset = 0ll;

    if (pOutEmptyPtr) {
        *pOutEmptyPtr = NULL;
    }
    *pOutEntryPtr = NULL;

    if (pQuery->kind == kDirectoryQuery_PathComponent) {
        if (pQuery->u.pc->count == 0) {
            return ENOENT;
        }
        if (pQuery->u.pc->count > kMaxFilenameLength) {
            return ENAMETOOLONG;
        }
    }

    while (true) {
        const int blockIdx = offset >> (FileOffset)kRamBlockSizeShift;
        const ssize_t nBytesAvailable = (ssize_t)__min((FileOffset)kRamBlockSize, fileSize - offset);
        char* pDiskBlock;

        if (nBytesAvailable <= 0) {
            break;
        }

        try(SerenaFS_GetDiskBlockForBlockIndex(self, pNode, blockIdx, kBlock_Read, &pDiskBlock));
        const RamDirectoryEntry* pCurEntry = (const RamDirectoryEntry*)pDiskBlock;
        const int nDirEntries = nBytesAvailable / sizeof(RamDirectoryEntry);
        if (xHasMatchingDirectoryEntry(pQuery, pCurEntry, nDirEntries, pOutEmptyPtr, pOutEntryPtr)) {
            return EOK;
        }
        offset += (FileOffset)nBytesAvailable;
    }
    return ENOENT;

catch:
    return err;
}

// Returns a reference to the directory entry that holds 'pName'. NULL and a
// suitable error is returned if no such entry exists or 'pName' is empty or
// too long.
static inline errno_t SerenaFS_GetDirectoryEntryForName(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, const PathComponent* _Nonnull pName, RamDirectoryEntry* _Nullable * _Nonnull pOutEntryPtr)
{
    RamDirectoryQuery q;

    q.kind = kDirectoryQuery_PathComponent;
    q.u.pc = pName;
    return SerenaFS_GetDirectoryEntry(self, pNode, &q, NULL, pOutEntryPtr);
}

// Returns a reference to the directory entry that holds 'id'. NULL and a
// suitable error is returned if no such entry exists.
static errno_t SerenaFS_GetDirectoryEntryForId(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, InodeId id, RamDirectoryEntry* _Nullable * _Nonnull pOutEntryPtr)
{
    RamDirectoryQuery q;

    q.kind = kDirectoryQuery_InodeId;
    q.u.id = id;
    return SerenaFS_GetDirectoryEntry(self, pNode, &q, NULL, pOutEntryPtr);
}

// Looks up the disk block that corresponds to the logical block address 'blockIdx'.
// The first logical block is #0 at the very beginning of the file 'pNode'. Logical
// block addresses increment by one until the end of the file. Note that not every
// logical block address may be backed by an actual disk block. A missing disk block
// is substituted at read time by an empty block.
// NOTE: never marks the inode as modified. The caller has to take care of this.
static errno_t SerenaFS_GetDiskBlockForBlockIndex(SerenaFSRef _Nonnull self, InodeRef _Nonnull pNode, int blockIdx, BlockAccessMode mode, char* _Nullable * _Nonnull pOutDiskBlock)
{
    decl_try_err();
    char* pDiskBlock = NULL;

    if (blockIdx < 0 || blockIdx >= kMaxDirectDataBlockPointers) {
        throw(EFBIG);
    }

    RamBlockMap* pBlockMap = Inode_GetBlockMap(pNode);
    pDiskBlock = pBlockMap->p[blockIdx];

    if (pDiskBlock) {
        *pOutDiskBlock = pDiskBlock;
    }
    else {
        if (mode == kBlock_Read) {
            *pOutDiskBlock = self->emptyBlock;
        }
        else {
            try(kalloc_cleared(kRamBlockSize, (void**)&pDiskBlock));
            pBlockMap->p[blockIdx] = pDiskBlock;
        }
    }

catch:
    *pOutDiskBlock = pDiskBlock;
    return err;
}

// Reads 'nBytesToRead' bytes from the file 'pNode' starting at offset 'offset'.
// This functions reads a block full of data from teh backing store and then
// invokes 'cb' with this block of data. 'cb' is expected to process the data.
// Note that 'cb' may process just a subset of the data and it returns how much
// of the data it has processed. This amount of bytes is then subtracted from
// 'nBytesToRead'. However the offset is always advanced by a full block size.
// This process continues until 'nBytesToRead' has decreased to 0, EOF or an
// error is encountered. Whatever comes first. 
static errno_t SerenaFS_xRead(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, FileOffset offset, ssize_t nBytesToRead, RamReadCallback _Nonnull cb, void* _Nullable pContext, ssize_t* _Nonnull pOutBytesRead)
{
    decl_try_err();
    const FileOffset fileSize = Inode_GetFileSize(pNode);
    ssize_t nOriginalBytesToRead = nBytesToRead;

    if (offset < 0ll) {
        throw(EINVAL);
    }

    while (nBytesToRead > 0) {
        const int blockIdx = (int)(offset >> (FileOffset)kRamBlockSizeShift);   //XXX blockIdx should be 64bit
        const ssize_t blockOffset = offset & (FileOffset)kRamBlockSizeMask;
        const ssize_t nBytesAvailable = (ssize_t)__min((FileOffset)(kRamBlockSize - blockOffset), __min(fileSize - offset, (FileOffset)nBytesToRead));
        char* pDiskBlock;

        if (nBytesAvailable <= 0) {
            break;
        }

        const errno_t e1 = SerenaFS_GetDiskBlockForBlockIndex(self, pNode, blockIdx, kBlock_Read, &pDiskBlock);
        if (e1 != EOK) {
            err = (nBytesToRead == nOriginalBytesToRead) ? e1 : EOK;
            break;
        }

        nBytesToRead -= cb(pContext, pDiskBlock + blockOffset, nBytesAvailable);
        offset += (FileOffset)nBytesAvailable;
    }

catch:
    *pOutBytesRead = nOriginalBytesToRead - nBytesToRead;
    if (*pOutBytesRead > 0) {
        Inode_SetModified(pNode, kInodeFlag_Accessed);
    }
    return err;
}

// Writes 'nBytesToWrite' bytes to the file 'pNode' starting at offset 'offset'.
// 'cb' is used to copy the data from teh source to the disk block(s).
static errno_t SerenaFS_xWrite(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, FileOffset offset, ssize_t nBytesToWrite, RamWriteCallback _Nonnull cb, void* _Nullable pContext, ssize_t* _Nonnull pOutBytesWritten)
{
    decl_try_err();
    ssize_t nBytesWritten = 0;

    if (offset < 0ll) {
        throw(EINVAL);
    }

    while (nBytesToWrite > 0) {
        const int blockIdx = (int)(offset >> (FileOffset)kRamBlockSizeShift);   //XXX blockIdx should be 64bit
        const ssize_t blockOffset = offset & (FileOffset)kRamBlockSizeMask;
        const ssize_t nBytesAvailable = __min(kRamBlockSize - blockOffset, nBytesToWrite);
        char* pDiskBlock;

        const errno_t e1 = SerenaFS_GetDiskBlockForBlockIndex(self, pNode, blockIdx, kBlock_Write, &pDiskBlock);
        if (e1 != EOK) {
            err = (nBytesWritten == 0) ? e1 : EOK;
            break;
        }
        
        cb(pDiskBlock + blockOffset, pContext, nBytesAvailable);
        nBytesWritten += nBytesAvailable;
        offset += (FileOffset)nBytesAvailable;
    }

catch:
    if (nBytesWritten > 0) {
        if (offset > Inode_GetFileSize(pNode)) {
            Inode_SetFileSize(pNode, offset);
        }
        Inode_SetModified(pNode, kInodeFlag_Updated | kInodeFlag_StatusChanged);
    }
    *pOutBytesWritten = nBytesWritten;
    return err;
}


// Invoked when an instance of this file system is mounted. Note that the
// kernel guarantees that no operations will be issued to the filesystem
// before onMount() has returned with EOK.
errno_t SerenaFS_onMount(SerenaFSRef _Nonnull self, const void* _Nonnull pParams, ssize_t paramsSize)
{
    decl_try_err();

    Lock_Lock(&self->lock);

    if (self->isMounted) {
        throw(EIO);
    }

catch:
    Lock_Unlock(&self->lock);
    return err;
}

// Invoked when a mounted instance of this file system is unmounted. A file
// system may return an error. Note however that this error is purely advisory
// and the file system implementation is required to do everything it can to
// successfully unmount. Unmount errors are ignored and the file system manager
// will complete the unmount in any case.
errno_t SerenaFS_onUnmount(SerenaFSRef _Nonnull self)
{
    decl_try_err();
/*
    Lock_Lock(&self->lock);
    if (!self->isMounted) {
        throw(EIO);
    }

    // There might be one or more operations currently ongoing. Wait until they
    // are done.
    while(self->busyCount > 0) {
        try(ConditionVariable_Wait(&self->notifier, &self->lock, kTimeInterval_Infinity));
    }


    // Make sure that there are no open files anywhere referencing us
    if (!FilesystemManager_CanSafelyUnmountFilesystem(gFilesystemManager, (FilesystemRef)self)) {
        throw(EBUSY);
    }

    // XXX Flush dirty buffers to disk

    Object_Release(self->root);
    self->root = NULL;

catch:
    Lock_Unlock(&self->lock);
    */
    return err;
}


// Returns the root node of the filesystem if the filesystem is currently in
// mounted state. Returns ENOENT and NULL if the filesystem is not mounted.
errno_t SerenaFS_acquireRootNode(SerenaFSRef _Nonnull self, InodeRef _Nullable _Locked * _Nonnull pOutNode)
{
    return Filesystem_AcquireNodeWithId((FilesystemRef)self, self->rootDirId, NULL, pOutNode);
}

// Returns EOK and the node that corresponds to the tuple (parent-node, name),
// if that node exists. Otherwise returns ENOENT and NULL.  Note that this
// function has to support the special names "." (node itself) and ".."
// (parent of node) in addition to "regular" filenames. If 'pParentNode' is
// the root node of the filesystem and 'pComponent' is ".." then 'pParentNode'
// should be returned. If the path component name is longer than what is
// supported by the file system, ENAMETOOLONG should be returned.
errno_t SerenaFS_acquireNodeForName(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pParentNode, const PathComponent* _Nonnull pName, User user, InodeRef _Nullable _Locked * _Nonnull pOutNode)
{
    decl_try_err();
    RamDirectoryEntry* pEntry;

    try(SerenaFS_CheckAccess_Locked(self, pParentNode, user, kFilePermission_Execute));
    try(SerenaFS_GetDirectoryEntryForName(self, pParentNode, pName, &pEntry));
    try(Filesystem_AcquireNodeWithId((FilesystemRef)self, pEntry->id, NULL, pOutNode));
    return EOK;

catch:
    *pOutNode = NULL;
    return err;
}

// Returns the name of the node with the id 'id' which a child of the
// directory node 'pParentNode'. 'id' may be of any type. The name is
// returned in the mutable path component 'pComponent'. 'count' in path
// component is 0 on entry and should be set to the actual length of the
// name on exit. The function is expected to return EOK if the parent node
// contains 'id' and ENOENT otherwise. If the name of 'id' as stored in the
// file system is > the capacity of the path component, then ERANGE should
// be returned.
errno_t SerenaFS_getNameOfNode(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pParentNode, InodeId id, User user, MutablePathComponent* _Nonnull pComponent)
{
    decl_try_err();
    RamDirectoryEntry* pEntry;

    try(SerenaFS_CheckAccess_Locked(self, pParentNode, user, kFilePermission_Read | kFilePermission_Execute));
    try(SerenaFS_GetDirectoryEntryForId(self, pParentNode, id, &pEntry));

    const ssize_t len = String_LengthUpTo(pEntry->filename, kMaxFilenameLength);
    if (len > pComponent->capacity) {
        throw(ERANGE);
    }

    String_CopyUpTo(pComponent->name, pEntry->filename, len);
    pComponent->count = len;
    return EOK;

catch:
    pComponent->count = 0;
    return err;
}

// Returns a file info record for the given Inode. The Inode may be of any
// file type.
errno_t SerenaFS_getFileInfo(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, FileInfo* _Nonnull pOutInfo)
{
    Inode_GetFileInfo(pNode, pOutInfo);
    return EOK;
}

// Modifies one or more attributes stored in the file info record of the given
// Inode. The Inode may be of any type.
errno_t SerenaFS_setFileInfo(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, User user, MutableFileInfo* _Nonnull pInfo)
{
    decl_try_err();

    if (self->isReadOnly) {
        throw(EROFS);
    }
    try(Inode_SetFileInfo(pNode, user, pInfo));

catch:
    return err;
}

static errno_t SerenaFS_RemoveDirectoryEntry(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDirNode, InodeId idToRemove)
{
    decl_try_err();
    RamDirectoryEntry* pEntry;

    try(SerenaFS_GetDirectoryEntryForId(self, pDirNode, idToRemove, &pEntry));
    pEntry->id = 0;
    pEntry->filename[0] = '\0';

    return EOK;

catch:
    return err;
}

// Inserts a new directory entry of the form (pName, id) into the directory node
// 'pDirNode'. 'pEmptyEntry' is an optional insertion hint. If this pointer exists
// then the directory entry that it points to will be reused for the new directory
// entry; otherwise a completely new entry will be added to the directory.
// NOTE: this function does not verify that the new entry is unique. The caller
// has to ensure that it doesn't try to add a duplicate entry to the directory.
static errno_t SerenaFS_InsertDirectoryEntry(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDirNode, const PathComponent* _Nonnull pName, InodeId id, RamDirectoryEntry* _Nullable pEmptyEntry)
{
    decl_try_err();

    if (pName->count > kMaxFilenameLength) {
        return ENAMETOOLONG;
    }

    if (pEmptyEntry == NULL) {
        // Append a new entry
        RamBlockMap* pContent = Inode_GetBlockMap(pDirNode);
        const FileOffset size = Inode_GetFileSize(pDirNode);
        const int blockIdx = size / kRamBlockSize;
        const int remainder = size & kRamBlockSizeMask;

        if (size == 0) {
            try(kalloc(kRamBlockSize, (void**)&pContent->p[0]));
            pEmptyEntry = (RamDirectoryEntry*)pContent->p[0];
        }
        else if (remainder < kRamBlockSize) {
            pEmptyEntry = (RamDirectoryEntry*)(pContent->p[blockIdx] + remainder);
        }
        else {
            try(kalloc(kRamBlockSize, (void**)&pContent->p[blockIdx + 1]));
            pEmptyEntry = (RamDirectoryEntry*)pContent->p[blockIdx + 1];
        }

        Inode_IncrementFileSize(pDirNode, sizeof(RamDirectoryEntry));
    }


    // Update the entry
    char* p = String_CopyUpTo(pEmptyEntry->filename, pName->name, pName->count);
    while (p < &pEmptyEntry->filename[kMaxFilenameLength]) *p++ = '\0';
    pEmptyEntry->id = id;


    // Mark the directory as modified
    if (err == EOK) {
        Inode_SetModified(pDirNode, kInodeFlag_Updated | kInodeFlag_StatusChanged);
    }

catch:
    return err;
}

static errno_t SerenaFS_CreateDirectoryDiskNode(SerenaFSRef _Nonnull self, InodeId parentId, UserId uid, GroupId gid, FilePermissions permissions, InodeId* _Nonnull pOutId)
{
    decl_try_err();
    InodeRef _Locked pDirNode = NULL;

    try(Filesystem_AllocateNode((FilesystemRef)self, kFileType_Directory, uid, gid, permissions, NULL, &pDirNode));
    const InodeId id = Inode_GetId(pDirNode);

    try(SerenaFS_InsertDirectoryEntry(self, pDirNode, &kPathComponent_Self, id, NULL));
    try(SerenaFS_InsertDirectoryEntry(self, pDirNode, &kPathComponent_Parent, (parentId > 0) ? parentId : id, NULL));

    Filesystem_RelinquishNode((FilesystemRef)self, pDirNode);
    *pOutId = id;
    return EOK;

catch:
    Filesystem_RelinquishNode((FilesystemRef)self, pDirNode);
    *pOutId = 0;
    return err;
}

// Creates an empty directory as a child of the given directory node and with
// the given name, user and file permissions. Returns EEXIST if a node with
// the given name already exists.
errno_t SerenaFS_createDirectory(SerenaFSRef _Nonnull self, const PathComponent* _Nonnull pName, InodeRef _Nonnull _Locked pParentNode, User user, FilePermissions permissions)
{
    decl_try_err();

    // 'pParentNode' must be a directory
    if (!Inode_IsDirectory(pParentNode)) {
        throw(ENOTDIR);
    }


    // We must have write permissions for 'pParentNode'
    try(SerenaFS_CheckAccess_Locked(self, pParentNode, user, kFilePermission_Write));


    // Make sure that 'pParentNode' doesn't already have an entry with name 'pName'.
    // Also figure out whether there's an empty entry that we can reuse.
    RamDirectoryEntry* pEmptyEntry;
    RamDirectoryEntry* pExistingEntry;
    RamDirectoryQuery q;

    q.kind = kDirectoryQuery_PathComponent;
    q.u.pc = pName;
    err = SerenaFS_GetDirectoryEntry(self, pParentNode, &q, &pEmptyEntry, &pExistingEntry);
    if (err == ENOENT) {
        err = EOK;
    } else if (err == EOK) {
        throw(EEXIST);
    } else {
        throw(err);
    }


    // Create the new directory and add it to its parent directory
    InodeId newDirId = 0;
    try(SerenaFS_CreateDirectoryDiskNode(self, Inode_GetId(pParentNode), user.uid, user.gid, permissions, &newDirId));
    try(SerenaFS_InsertDirectoryEntry(self, pParentNode, pName, newDirId, pEmptyEntry));

    return EOK;

catch:
    // XXX Unlink new dir disk node
    return err;
}

// Opens the directory represented by the given node. Returns a directory
// descriptor object which is the I/O channel that allows you to read the
// directory content.
errno_t SerenaFS_openDirectory(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDirNode, User user, DirectoryRef _Nullable * _Nonnull pOutDir)
{
    decl_try_err();

    try(Inode_CheckAccess(pDirNode, user, kFilePermission_Read));
    try(Directory_Create((FilesystemRef)self, pDirNode, pOutDir));

catch:
    return err;
}

// Reads the next set of directory entries. The first entry read is the one
// at the current directory index stored in 'pDir'. This function guarantees
// that it will only ever return complete directories entries. It will never
// return a partial entry. Consequently the provided buffer must be big enough
// to hold at least one directory entry. Note that this function is expected
// to return "." for the entry at index #0 and ".." for the entry at index #1.
static ssize_t xCopyOutDirectoryEntries(DirectoryEntry* _Nonnull pOut, const RamDirectoryEntry* _Nonnull pIn, ssize_t nBytesToRead)
{
    ssize_t nBytesCopied = 0;

    while (nBytesToRead > 0) {
        if (pIn->id > 0) {
            pOut->inodeId = pIn->id;
            String_CopyUpTo(pOut->name, pIn->filename, kMaxFilenameLength);
            nBytesCopied += sizeof(RamDirectoryEntry);
            pOut++;
        }
        pIn++;
        nBytesToRead -= sizeof(RamDirectoryEntry);
    }

    return nBytesCopied;
}

errno_t SerenaFS_readDirectory(SerenaFSRef _Nonnull self, DirectoryRef _Nonnull pDir, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    InodeRef _Locked pNode = Directory_GetInode(pDir);
    const ssize_t nBytesToReadFromDirectory = (nBytesToRead / sizeof(DirectoryEntry)) * sizeof(RamDirectoryEntry);
    ssize_t nBytesRead;

    // XXX reading multiple entries at once doesn't work right because xRead advances 'pBuffer' by sizeof(RamDirectoryEntry) rather
    // XXX than DirectoryEntry. Former is 32 bytes and later is 260 bytes.
    // XXX the Directory_GetOffset() should really return the numer of the entry rather than a byte offset
    const errno_t err = SerenaFS_xRead(self, 
        pNode, 
        Directory_GetOffset(pDir),
        nBytesToReadFromDirectory,
        (RamReadCallback)xCopyOutDirectoryEntries,
        pBuffer,
        &nBytesRead);
    Directory_IncrementOffset(pDir, nBytesRead);
    *nOutBytesRead = (nBytesRead / sizeof(RamDirectoryEntry)) * sizeof(DirectoryEntry);
    return err;
}

// Creates an empty file and returns the inode of that file. The behavior is
// non-exclusive by default. Meaning the file is created if it does not 
// exist and the file's inode is merrily acquired if it already exists. If
// the mode is exclusive then the file is created if it doesn't exist and
// an error is thrown if the file exists. Note that the file is not opened.
// This must be done by calling the open() method.
errno_t SerenaFS_createFile(SerenaFSRef _Nonnull self, const PathComponent* _Nonnull pName, InodeRef _Nonnull _Locked pParentNode, User user, unsigned int options, FilePermissions permissions, InodeRef _Nullable _Locked * _Nonnull pOutNode)
{
    decl_try_err();

    // 'pParentNode' must be a directory
    if (!Inode_IsDirectory(pParentNode)) {
        throw(ENOTDIR);
    }


    // We must have write permissions for 'pParentNode'
    try(SerenaFS_CheckAccess_Locked(self, pParentNode, user, kFilePermission_Write));


    // Make sure that 'pParentNode' doesn't already have an entry with name 'pName'.
    // Also figure out whether there's an empty entry that we can reuse.
    RamDirectoryEntry* pEmptyEntry;
    RamDirectoryEntry* pExistingEntry;
    RamDirectoryQuery q;

    q.kind = kDirectoryQuery_PathComponent;
    q.u.pc = pName;
    err = SerenaFS_GetDirectoryEntry(self, pParentNode, &q, &pEmptyEntry, &pExistingEntry);
    if (err == ENOENT) {
        err = EOK;
    } else if (err == EOK) {
        if ((options & kOpen_Exclusive) == kOpen_Exclusive) {
            // Exclusive mode: File already exists -> throw an error
            throw(EEXIST);
        }
        else {
            // Non-exclusive mode: File already exists -> acquire it and let the caller open it
            try(Filesystem_AcquireNodeWithId((FilesystemRef)self, pExistingEntry->id, NULL, pOutNode));

            // Truncate the file to length 0, if requested
            if ((options & kOpen_Truncate) == kOpen_Truncate) {
                SerenaFS_xTruncateFile(self, *pOutNode, 0);
            }

            return EOK;
        }
    } else {
        throw(err);
    }


    // Create the new file and add it to its parent directory
    try(Filesystem_AllocateNode((FilesystemRef)self, kFileType_RegularFile, user.uid, user.gid, permissions, NULL, pOutNode));
    try(SerenaFS_InsertDirectoryEntry(self, pParentNode, pName, Inode_GetId(*pOutNode), pEmptyEntry));

    return EOK;

catch:
    // XXX Unlink new file disk node if necessary
    return err;
}

// Opens a resource context/channel to the resource. This new resource context
// will be represented by a (file) descriptor in user space. The resource context
// maintains state that is specific to this connection. This state will be
// protected by the resource's internal locking mechanism. 'pNode' represents
// the named resource instance that should be represented by the I/O channel.
errno_t SerenaFS_open(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, unsigned int mode, User user, FileRef _Nullable * _Nonnull pOutFile)
{
    decl_try_err();
    FilePermissions permissions = 0;

    if (Inode_IsDirectory(pNode)) {
        throw(EISDIR);
    }

    if ((mode & kOpen_ReadWrite) == 0) {
        throw(EACCESS);
    }
    if ((mode & kOpen_Read) != 0) {
        permissions |= kFilePermission_Read;
    }
    if ((mode & kOpen_Write) != 0) {
        permissions |= kFilePermission_Write;
    }

    try(Inode_CheckAccess(pNode, user, permissions));
    try(File_Create((FilesystemRef)self, mode, pNode, pOutFile));

    if ((mode & kOpen_Truncate) != 0) {
        SerenaFS_xTruncateFile(self, pNode, 0);
    }
    
catch:
    return err;
}

// Close the resource. The purpose of the close operation is:
// - flush all data that was written and is still buffered/cached to the underlying device
// - if a write operation is ongoing at the time of the close then let this write operation finish and sync the underlying device
// - if a read operation is ongoing at the time of the close then interrupt the read with an EINTR error
// The resource should be internally marked as closed and all future read/write/etc operations on the resource should do nothing
// and instead return a suitable status. Eg a write should return EIO and a read should return EOF.
// It is permissible for a close operation to block the caller for some (reasonable) amount of time to complete the flush.
// The close operation may return an error. Returning an error will not stop the kernel from completing the close and eventually
// deallocating the resource. The error is passed on to the caller but is purely advisory in nature. The close operation is
// required to mark the resource as closed whether the close internally succeeded or failed. 
errno_t SerenaFS_close(SerenaFSRef _Nonnull self, FileRef _Nonnull pFile)
{
    // Nothing to do for now
    return EOK;
}

static ssize_t xCopyOutFileContent(void* _Nonnull pOut, const void* _Nonnull pIn, ssize_t nBytesToRead)
{
    Bytes_CopyRange(pOut, pIn, nBytesToRead);
    return nBytesToRead;
}

errno_t SerenaFS_read(SerenaFSRef _Nonnull self, FileRef _Nonnull pFile, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    InodeRef _Locked pNode = File_GetInode(pFile);

    const errno_t err = SerenaFS_xRead(self, 
        pNode, 
        File_GetOffset(pFile),
        nBytesToRead,
        (RamReadCallback)xCopyOutFileContent,
        pBuffer,
        nOutBytesRead);
    File_IncrementOffset(pFile, *nOutBytesRead);
    return err;
}

errno_t SerenaFS_write(SerenaFSRef _Nonnull self, FileRef _Nonnull pFile, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    InodeRef _Locked pNode = File_GetInode(pFile);
    FileOffset offset;

    if (File_IsAppendOnWrite(pFile)) {
        offset = Inode_GetFileSize(pNode);
    } else {
        offset = File_GetOffset(pFile);
    }

    const errno_t err = SerenaFS_xWrite(self, 
        pNode, 
        offset,
        nBytesToWrite,
        (RamWriteCallback)Bytes_CopyRange,
        pBuffer,
        nOutBytesWritten);
    File_IncrementOffset(pFile, *nOutBytesWritten);
    return err;
}

// Internal file truncation function. Shortens the file 'pNode' to the new and
// smaller size 'length'. Does not support increasing the size of a file. Expects
// that 'pNode' is a regular file.
static void SerenaFS_xTruncateFile(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, FileOffset length)
{
    const FileOffset oldLength = Inode_GetFileSize(pNode);
    const FileOffset oldLengthRoundedUpToBlockBoundary = __Ceil_PowerOf2(oldLength, kRamBlockSize);
    const int firstBlockIdx = (int)(oldLengthRoundedUpToBlockBoundary >> (FileOffset)kRamBlockSizeShift);    //XXX blockIdx should be 64bit
    RamBlockMap* pBlockMap = Inode_GetBlockMap(pNode);

    for (int i = firstBlockIdx; i < kMaxDirectDataBlockPointers; i++) {
        void* pBlockPtr = pBlockMap->p[i];

        if (pBlockPtr) {
            kfree(pBlockPtr);
            pBlockMap->p[i] = NULL;
        }
    }

    Inode_SetFileSize(pNode, length);
    Inode_SetModified(pNode, kInodeFlag_Updated | kInodeFlag_StatusChanged);
}

// Change the size of the file 'pNode' to 'length'. EINVAL is returned if
// the new length is negative. No longer needed blocks are deallocated if
// the new length is less than the old length and zero-fille blocks are
// allocated and assigned to the file if the new length is longer than the
// old length. Note that a filesystem implementation is free to defer the
// actual allocation of the new blocks until an attempt is made to read or
// write them.
errno_t SerenaFS_truncate(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, User user, FileOffset length)
{
    decl_try_err();

    if (Inode_IsDirectory(pNode)) {
        throw(EISDIR);
    }
    if (!Inode_IsRegularFile(pNode)) {
        throw(ENOTDIR);
    }
    if (length < 0) {
        throw(EINVAL);
    }
    try(Inode_CheckAccess(pNode, user, kFilePermission_Write));

    const FileOffset oldLength = Inode_GetFileSize(pNode);
    if (oldLength < length) {
        // Expansion in size
        // Just set the new file size. The needed blocks will be allocated on
        // demand when read/write is called to manipulate the new data range.
        Inode_SetFileSize(pNode, length);
        Inode_SetModified(pNode, kInodeFlag_Updated | kInodeFlag_StatusChanged); 
    }
    else if (oldLength > length) {
        // Reduction in size
        SerenaFS_xTruncateFile(self, pNode, length);
    }

catch:
    return err;
}

// Verifies that the given node is accessible assuming the given access mode.
errno_t SerenaFS_checkAccess(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, User user, int mode)
{
    decl_try_err();

    if ((mode & kAccess_Readable) == kAccess_Readable) {
        err = Inode_CheckAccess(pNode, user, kFilePermission_Read);
    }
    if (err == EOK && ((mode & kAccess_Writable) == kAccess_Writable)) {
        err = Inode_CheckAccess(pNode, user, kFilePermission_Write);
    }
    if (err == EOK && ((mode & kAccess_Executable) == kAccess_Executable)) {
        err = Inode_CheckAccess(pNode, user, kFilePermission_Execute);
    }

    return err;
}

// Unlink the node 'pNode' which is an immediate child of 'pParentNode'.
// Both nodes are guaranteed to be members of the same filesystem. 'pNode'
// is guaranteed to exist and that it isn't a mountpoint and not the root
// node of the filesystem.
// This function must validate that that if 'pNode' is a directory, that the
// directory is empty (contains nothing except "." and "..").
errno_t SerenaFS_unlink(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNodeToUnlink, InodeRef _Nonnull _Locked pParentNode, User user)
{
    decl_try_err();

    // We must have write permissions for 'pParentNode'
    try(SerenaFS_CheckAccess_Locked(self, pParentNode, user, kFilePermission_Write));


    // A directory must be empty in order to be allowed to unlink it
    if (Inode_IsDirectory(pNodeToUnlink) && !DirectoryNode_IsEmpty(pNodeToUnlink)) {
        throw(EBUSY);
    }


    // Remove the directory entry in the parent directory
    try(SerenaFS_RemoveDirectoryEntry(self, pParentNode, Inode_GetId(pNodeToUnlink)));


    // Unlink the node itself
    Inode_Unlink(pNodeToUnlink);
    Inode_SetModified(pNodeToUnlink, kInodeFlag_StatusChanged);

catch:
    return err;
}

// Renames the node with name 'pName' and which is an immediate child of the
// node 'pParentNode' such that it becomes a child of 'pNewParentNode' with
// the name 'pNewName'. All nodes are guaranteed to be owned by the filesystem.
errno_t SerenaFS_rename(SerenaFSRef _Nonnull self, const PathComponent* _Nonnull pName, InodeRef _Nonnull _Locked pParentNode, const PathComponent* _Nonnull pNewName, InodeRef _Nonnull _Locked pNewParentNode, User user)
{
    // XXX implement me
    return EACCESS;
}


CLASS_METHODS(SerenaFS, Filesystem,
OVERRIDE_METHOD_IMPL(deinit, SerenaFS, Object)
OVERRIDE_METHOD_IMPL(onAllocateNodeOnDisk, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(onReadNodeFromDisk, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(onWriteNodeToDisk, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(onRemoveNodeFromDisk, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(onMount, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(onUnmount, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(acquireRootNode, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(acquireNodeForName, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(getNameOfNode, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(getFileInfo, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(setFileInfo, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(createFile, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(createDirectory, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(openDirectory, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(readDirectory, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(open, SerenaFS, IOResource)
OVERRIDE_METHOD_IMPL(close, SerenaFS, IOResource)
OVERRIDE_METHOD_IMPL(read, SerenaFS, IOResource)
OVERRIDE_METHOD_IMPL(write, SerenaFS, IOResource)
OVERRIDE_METHOD_IMPL(truncate, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(checkAccess, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(unlink, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(rename, SerenaFS, Filesystem)
);