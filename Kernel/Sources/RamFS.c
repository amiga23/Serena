//
//  RamFS.c
//  Apollo
//
//  Created by Dietmar Planitzer on 11/11/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "RamFSPriv.h"


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Inode extensions
////////////////////////////////////////////////////////////////////////////////

// Returns true if the given directory node is empty (contains just "." and "..").
static Bool DirectoryNode_IsEmpty(InodeRef _Nonnull _Locked self)
{
    return Inode_GetFileSize(self) <= sizeof(RamDirectoryEntry) * 2;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Filesystem
////////////////////////////////////////////////////////////////////////////////

// Creates an instance of RamFS. RamFS is a volatile file system that does not
// survive system restarts. The 'rootDirUser' parameter specifies the user and
// group ID of the root directory.
ErrorCode RamFS_Create(User rootDirUser, RamFSRef _Nullable * _Nonnull pOutFileSys)
{
    decl_try_err();
    RamFSRef self;

    assert(sizeof(RamDiskNode) <= kRamBlockSize);
    assert(sizeof(RamDirectoryEntry) * kRamDirectoryEntriesPerBlock == kRamBlockSize);
    
    try(Filesystem_Create(&kRamFSClass, (FilesystemRef*)&self));
    Lock_Init(&self->lock);
    ConditionVariable_Init(&self->notifier);
    PointerArray_Init(&self->dnodes, 16);
    self->rootDirUser = rootDirUser;
    self->nextAvailableInodeId = 1;
    self->isMounted = false;
    self->isReadOnly = false;

    try(RamFS_FormatWithEmptyFilesystem(self));

    *pOutFileSys = self;
    return EOK;

catch:
    *pOutFileSys = NULL;
    return err;
}

void RamFS_deinit(RamFSRef _Nonnull self)
{
    for(Int i = 0; i < PointerArray_GetCount(&self->dnodes); i++) {
        RamFS_DestroyDiskNode(self, PointerArray_GetAtAs(&self->dnodes, i, RamDiskNodeRef));
    }
    PointerArray_Deinit(&self->dnodes);
    ConditionVariable_Deinit(&self->notifier);
    Lock_Deinit(&self->lock);
}

static ErrorCode RamFS_FormatWithEmptyFilesystem(RamFSRef _Nonnull self)
{
    decl_try_err();
    const FilePermissions scopePerms = kFilePermission_Read | kFilePermission_Write | kFilePermission_Execute;
    const FilePermissions dirPerms = FilePermissions_Make(scopePerms, scopePerms, scopePerms);

    try(RamFS_CreateDirectoryDiskNode(self, 0, self->rootDirUser.uid, self->rootDirUser.gid, dirPerms, &self->rootDirId));
    return EOK;

catch:
    return err;
}

static Int RamFS_GetIndexOfDiskNodeForId(RamFSRef _Nonnull self, InodeId id)
{
    for (Int i = 0; i < PointerArray_GetCount(&self->dnodes); i++) {
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
ErrorCode RamFS_onAllocateNodeOnDisk(RamFSRef _Nonnull self, InodeType type, void* _Nullable pContext, InodeId* _Nonnull pOutId)
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
    RamFS_DestroyDiskNode(self, pDiskNode);
    *pOutId = 0;
    return err;
}

static void RamFS_DestroyDiskNode(RamFSRef _Nonnull self, RamDiskNodeRef _Nullable pDiskNode)
{
    if (pDiskNode) {
        for (Int i = 0; i < kMaxDirectDataBlockPointers; i++) {
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
ErrorCode RamFS_onReadNodeFromDisk(RamFSRef _Nonnull self, InodeId id, void* _Nullable pContext, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    const Int dIdx = RamFS_GetIndexOfDiskNodeForId(self, id);
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
        &pDiskNode->blockMap,
        pOutNode);

catch:
    return err;
}

// Invoked when the inode is relinquished and it is marked as modified. The
// filesystem override should write the inode meta-data back to the 
// corresponding disk node.
ErrorCode RamFS_onWriteNodeToDisk(RamFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode)
{
    decl_try_err();
    const Int dIdx = RamFS_GetIndexOfDiskNodeForId(self, Inode_GetId(pNode));
    if (dIdx < 0) throw(ENOENT);
    RamDiskNodeRef pDiskNode = PointerArray_GetAtAs(&self->dnodes, dIdx, RamDiskNodeRef);

    pDiskNode->linkCount = Inode_GetLinkCount(pNode);
    pDiskNode->uid = Inode_GetUserId(pNode);
    pDiskNode->gid = Inode_GetGroupId(pNode);
    pDiskNode->permissions = Inode_GetFilePermissions(pNode);
    pDiskNode->size = Inode_GetFileSize(pNode);
    return EOK;

catch:
    return err;
}

// Invoked when Filesystem_RelinquishNode() has determined that the inode is
// no longer being referenced by any directory and that the on-disk
// representation should be deleted from the disk and deallocated. This
// operation is assumed to never fail.
void RamFS_onRemoveNodeFromDisk(RamFSRef _Nonnull self, InodeId id)
{
    const Int dIdx = RamFS_GetIndexOfDiskNodeForId(self, id);

    if (dIdx >= 0) {
        RamDiskNodeRef pDiskNode = PointerArray_GetAtAs(&self->dnodes, dIdx, RamDiskNodeRef);

        RamFS_DestroyDiskNode(self, pDiskNode);
        PointerArray_RemoveAt(&self->dnodes, dIdx);
    }
}

// Checks whether the given user should be granted access to the given node based
// on the requested permission. Returns EOK if access should be granted and a suitable
// error code if it should be denied.
static ErrorCode RamFS_CheckAccess_Locked(RamFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, User user, FilePermissions permission)
{
    if (permission == kFilePermission_Write) {
        if (self->isReadOnly) {
            return EROFS;
        }

        // XXX once we support actual text mapping, we'll need to check whether the text file is in use
    }

    return Inode_CheckAccess(pNode, user, permission);
}

// Returns true if the array of directory entries starting at 'pEntry' and holding
// 'nEntries' entries contains a directory entry that matches 'pQuery'.
static Bool xHasMatchingDirectoryEntry(const RamDirectoryQuery* _Nonnull pQuery, const RamDirectoryEntry* _Nonnull pEntry, Int nEntries, RamDirectoryEntry* _Nullable * _Nullable pOutEmptyPtr, RamDirectoryEntry* _Nullable * _Nonnull pOutEntryPtr)
{
    while (nEntries-- > 0) {
        if (pEntry->id > 0) {
            switch (pQuery->kind) {
                case kDirectoryQuery_PathComponent:
                    if (String_EqualsUpTo(pEntry->filename, pQuery->u.pc->name, __min(pQuery->u.pc->count, kMaxFilenameLength))) {
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
static ErrorCode RamFS_GetDirectoryEntry(RamFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, const RamDirectoryQuery* _Nonnull pQuery, RamDirectoryEntry* _Nullable * _Nullable pOutEmptyPtr, RamDirectoryEntry* _Nullable * _Nonnull pOutEntryPtr)
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
        const Int blockIdx = offset >> (FileOffset)kRamBlockSizeShift;
        const ByteCount nBytesAvailable = (ByteCount)__min((FileOffset)kRamBlockSize, fileSize - offset);
        Byte* pDiskBlock;

        if (nBytesAvailable <= 0) {
            break;
        }

        try(RamFS_GetDiskBlockForBlockIndex(self, pNode, blockIdx, kBlock_Read, &pDiskBlock));
        const RamDirectoryEntry* pCurEntry = (const RamDirectoryEntry*)pDiskBlock;
        const Int nDirEntries = nBytesAvailable / sizeof(RamDirectoryEntry);
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
static inline ErrorCode RamFS_GetDirectoryEntryForName(RamFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, const PathComponent* _Nonnull pName, RamDirectoryEntry* _Nullable * _Nonnull pOutEntryPtr)
{
    RamDirectoryQuery q;

    q.kind = kDirectoryQuery_PathComponent;
    q.u.pc = pName;
    return RamFS_GetDirectoryEntry(self, pNode, &q, NULL, pOutEntryPtr);
}

// Returns a reference to the directory entry that holds 'id'. NULL and a
// suitable error is returned if no such entry exists.
static ErrorCode RamFS_GetDirectoryEntryForId(RamFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, InodeId id, RamDirectoryEntry* _Nullable * _Nonnull pOutEntryPtr)
{
    RamDirectoryQuery q;

    q.kind = kDirectoryQuery_InodeId;
    q.u.id = id;
    return RamFS_GetDirectoryEntry(self, pNode, &q, NULL, pOutEntryPtr);
}

// Looks up the disk block that corresponds to the logical block address 'blockIdx'.
// The first logical block is #0 at the very beginning of the file 'pNode'. Logical
// block addresses increment by one until the end of the file. Note that not every
// logical block address may be backed by an actual disk block. A missing disk block
// is substituted at read time by an empty block.
// NOTE: never marks the inode as modified. The caller has to take care of this.
static ErrorCode RamFS_GetDiskBlockForBlockIndex(RamFSRef _Nonnull self, InodeRef _Nonnull pNode, Int blockIdx, BlockAccessMode mode, Byte* _Nullable * _Nonnull pOutDiskBlock)
{
    decl_try_err();
    Byte* pDiskBlock = NULL;

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
static ErrorCode RamFS_xRead(RamFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, FileOffset offset, ByteCount nBytesToRead, RamReadCallback _Nonnull cb, void* _Nullable pContext, ByteCount* _Nonnull pOutBytesRead)
{
    decl_try_err();
    const FileOffset fileSize = Inode_GetFileSize(pNode);
    ByteCount nOriginalBytesToRead = nBytesToRead;

    if (offset < 0ll) {
        throw(EINVAL);
    }

    while (nBytesToRead > 0) {
        const Int blockIdx = offset >> (FileOffset)kRamBlockSizeShift;
        const ByteCount blockOffset = offset & (FileOffset)kRamBlockSizeMask;
        const ByteCount nBytesAvailable = (ByteCount)__min((FileOffset)(kRamBlockSize - blockOffset), __min(fileSize - offset, (FileOffset)nBytesToRead));
        Byte* pDiskBlock;

        if (nBytesAvailable <= 0) {
            break;
        }

        try(RamFS_GetDiskBlockForBlockIndex(self, pNode, blockIdx, kBlock_Read, &pDiskBlock));
        nBytesToRead -= cb(pContext, pDiskBlock + blockOffset, nBytesAvailable);
        offset += (FileOffset)nBytesAvailable;
    }

catch:
    *pOutBytesRead = nOriginalBytesToRead - nBytesToRead;
    return err;
}

// Writes 'nBytesToWrite' bytes to the file 'pNode' starting at offset 'offset'.
// 'cb' is used to copy the data from teh source to the disk block(s).
static ErrorCode RamFS_xWrite(RamFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, FileOffset offset, ByteCount nBytesToWrite, RamWriteCallback _Nonnull cb, void* _Nullable pContext, ByteCount* _Nonnull pOutBytesWritten)
{
    decl_try_err();
    ByteCount nBytesWritten = 0;

    if (offset < 0ll) {
        throw(EINVAL);
    }

    while (nBytesToWrite > 0) {
        const Int blockIdx = offset >> (FileOffset)kRamBlockSizeShift;
        const ByteCount blockOffset = offset & (FileOffset)kRamBlockSizeMask;
        const ByteCount nBytesAvailable = __min(kRamBlockSize - blockOffset, nBytesToWrite);
        Byte* pDiskBlock;

        try(RamFS_GetDiskBlockForBlockIndex(self, pNode, blockIdx, kBlock_Write, &pDiskBlock));
        cb(pDiskBlock + blockOffset, pContext, nBytesAvailable);
        nBytesWritten += nBytesAvailable;
        offset += (FileOffset)nBytesAvailable;
    }

catch:
    if (nBytesWritten > 0) {
        if (offset > Inode_GetFileSize(pNode)) {
            Inode_SetFileSize(pNode, offset);
        }
        Inode_SetModified(pNode);
    }
    *pOutBytesWritten = nBytesWritten;
    return err;
}


// Invoked when an instance of this file system is mounted. Note that the
// kernel guarantees that no operations will be issued to the filesystem
// before onMount() has returned with EOK.
ErrorCode RamFS_onMount(RamFSRef _Nonnull self, const Byte* _Nonnull pParams, ByteCount paramsSize)
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
ErrorCode RamFS_onUnmount(RamFSRef _Nonnull self)
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
ErrorCode RamFS_acquireRootNode(RamFSRef _Nonnull self, InodeRef _Nullable _Locked * _Nonnull pOutNode)
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
ErrorCode RamFS_acquireNodeForName(RamFSRef _Nonnull self, InodeRef _Nonnull _Locked pParentNode, const PathComponent* _Nonnull pName, User user, InodeRef _Nullable _Locked * _Nonnull pOutNode)
{
    decl_try_err();
    RamDirectoryEntry* pEntry;

    try(RamFS_CheckAccess_Locked(self, pParentNode, user, kFilePermission_Execute));
    try(RamFS_GetDirectoryEntryForName(self, pParentNode, pName, &pEntry));
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
ErrorCode RamFS_getNameOfNode(RamFSRef _Nonnull self, InodeRef _Nonnull _Locked pParentNode, InodeId id, User user, MutablePathComponent* _Nonnull pComponent)
{
    decl_try_err();
    RamDirectoryEntry* pEntry;

    try(RamFS_CheckAccess_Locked(self, pParentNode, user, kFilePermission_Read | kFilePermission_Execute));
    try(RamFS_GetDirectoryEntryForId(self, pParentNode, id, &pEntry));

    const ByteCount len = String_LengthUpTo(pEntry->filename, kMaxFilenameLength);
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
ErrorCode RamFS_getFileInfo(RamFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, FileInfo* _Nonnull pOutInfo)
{
    Inode_GetFileInfo(pNode, pOutInfo);
    return EOK;
}

// Modifies one or more attributes stored in the file info record of the given
// Inode. The Inode may be of any type.
ErrorCode RamFS_setFileInfo(RamFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, User user, MutableFileInfo* _Nonnull pInfo)
{
    decl_try_err();

    if (self->isReadOnly) {
        throw(EROFS);
    }
    try(Inode_SetFileInfo(pNode, user, pInfo));

catch:
    return err;
}

static ErrorCode RamFS_RemoveDirectoryEntry(RamFSRef _Nonnull self, InodeRef _Nonnull _Locked pDirNode, InodeId idToRemove)
{
    decl_try_err();
    RamDirectoryEntry* pEntry;

    try(RamFS_GetDirectoryEntryForId(self, pDirNode, idToRemove, &pEntry));
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
static ErrorCode RamFS_InsertDirectoryEntry(RamFSRef _Nonnull self, InodeRef _Nonnull _Locked pDirNode, const PathComponent* _Nonnull pName, InodeId id, RamDirectoryEntry* _Nullable pEmptyEntry)
{
    decl_try_err();

    if (pName->count > kMaxFilenameLength) {
        return ENAMETOOLONG;
    }

    if (pEmptyEntry == NULL) {
        // Append a new entry
        RamBlockMap* pContent = Inode_GetBlockMap(pDirNode);
        const FileOffset size = Inode_GetFileSize(pDirNode);
        const Int blockIdx = size / kRamBlockSize;
        const Int remainder = size & kRamBlockSizeMask;

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
    Character* p = String_CopyUpTo(pEmptyEntry->filename, pName->name, pName->count);
    while (p < &pEmptyEntry->filename[kMaxFilenameLength]) *p++ = '\0';
    pEmptyEntry->id = id;


    // Mark the directory as modified
    if (err == EOK) {
        Inode_SetModified(pDirNode);
    }

catch:
    return err;
}

static ErrorCode RamFS_CreateDirectoryDiskNode(RamFSRef _Nonnull self, InodeId parentId, UserId uid, GroupId gid, FilePermissions permissions, InodeId* _Nonnull pOutId)
{
    decl_try_err();
    InodeRef _Locked pDirNode = NULL;

    try(Filesystem_AllocateNode((FilesystemRef)self, kInode_Directory, uid, gid, permissions, NULL, &pDirNode));
    const InodeId id = Inode_GetId(pDirNode);

    try(RamFS_InsertDirectoryEntry(self, pDirNode, &kPathComponent_Self, id, NULL));
    try(RamFS_InsertDirectoryEntry(self, pDirNode, &kPathComponent_Parent, (parentId > 0) ? parentId : id, NULL));

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
ErrorCode RamFS_createDirectory(RamFSRef _Nonnull self, const PathComponent* _Nonnull pName, InodeRef _Nonnull _Locked pParentNode, User user, FilePermissions permissions)
{
    decl_try_err();

    // 'pParentNode' must be a directory
    if (!Inode_IsDirectory(pParentNode)) {
        throw(ENOTDIR);
    }


    // We must have write permissions for 'pParentNode'
    try(RamFS_CheckAccess_Locked(self, pParentNode, user, kFilePermission_Write));


    // Make sure that 'pParentNode' doesn't already have an entry with name 'pName'.
    // Also figure out whether there's an empty entry that we can reuse.
    RamDirectoryEntry* pEmptyEntry;
    RamDirectoryEntry* pExistingEntry;
    RamDirectoryQuery q;

    q.kind = kDirectoryQuery_PathComponent;
    q.u.pc = pName;
    err = RamFS_GetDirectoryEntry(self, pParentNode, &q, &pEmptyEntry, &pExistingEntry);
    if (err != ENOENT) {
        if (err == EOK) {
            throw(EEXIST);
        } else {
            throw(err);
        }
    }
    err = EOK;


    // Create the new directory and add it to its parent directory
    InodeId newDirId = 0;
    try(RamFS_CreateDirectoryDiskNode(self, Inode_GetId(pParentNode), user.uid, user.gid, permissions, &newDirId));
    try(RamFS_InsertDirectoryEntry(self, pParentNode, pName, newDirId, pEmptyEntry));

    return EOK;

catch:
    // XXX Unlink new dir disk node
    return err;
}

// Opens the directory represented by the given node. Returns a directory
// descriptor object which is the I/O channel that allows you to read the
// directory content.
ErrorCode RamFS_openDirectory(RamFSRef _Nonnull self, InodeRef _Nonnull _Locked pDirNode, User user, DirectoryRef _Nullable * _Nonnull pOutDir)
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
static ByteCount xCopyOutDirectoryEntries(DirectoryEntry* _Nonnull pOut, const RamDirectoryEntry* _Nonnull pIn, ByteCount nBytesToRead)
{
    ByteCount nBytesCopied = 0;

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

ByteCount RamFS_readDirectory(RamFSRef _Nonnull self, DirectoryRef _Nonnull pDir, Byte* _Nonnull pBuffer, ByteCount nBytesToRead)
{
    InodeRef _Locked pNode = Directory_GetInode(pDir);
    const ByteCount nBytesToReadFromDirectory = (nBytesToRead / sizeof(DirectoryEntry)) * sizeof(RamDirectoryEntry);
    ByteCount nBytesRead;

    const ErrorCode err = RamFS_xRead(self, 
        pNode, 
        Directory_GetOffset(pDir),
        nBytesToReadFromDirectory,
        (RamReadCallback)xCopyOutDirectoryEntries,
        pBuffer,
        &nBytesRead);
    Directory_IncrementOffset(pDir, nBytesRead);
    return (err == EOK) ? nBytesRead : -err;
}

static ByteCount xCopyOutFileContent(Byte* _Nonnull pOut, const Byte* _Nonnull pIn, ByteCount nBytesToRead)
{
    Bytes_CopyRange(pOut, pIn, nBytesToRead);
    return nBytesToRead;
}

ByteCount RamFS_read(RamFSRef _Nonnull self, FileRef _Nonnull pFile, Byte* _Nonnull pBuffer, ByteCount nBytesToRead)
{
    InodeRef _Locked pNode = File_GetInode(pFile);
    ByteCount nBytesRead;

    const ErrorCode err = RamFS_xRead(self, 
        pNode, 
        File_GetOffset(pFile),
        nBytesToRead,
        (RamReadCallback)xCopyOutFileContent,
        pBuffer,
        &nBytesRead);
    File_IncrementOffset(pFile, nBytesRead);
    return (err == EOK) ? nBytesRead : -err;
}

ByteCount RamFS_write(RamFSRef _Nonnull self, FileRef _Nonnull pFile, const Byte* _Nonnull pBuffer, ByteCount nBytesToWrite)
{
    InodeRef _Locked pNode = File_GetInode(pFile);
    ByteCount nBytesWritten;

    const ErrorCode err = RamFS_xWrite(self, 
        pNode, 
        File_GetOffset(pFile),
        nBytesToWrite,
        (RamWriteCallback)Bytes_CopyRange,
        pBuffer,
        &nBytesWritten);
    File_IncrementOffset(pFile, nBytesWritten);
    return (err == EOK) ? nBytesWritten : -err;
}

// Verifies that the given node is accessible assuming the given access mode.
ErrorCode RamFS_checkAccess(RamFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, User user, Int mode)
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
ErrorCode RamFS_unlink(RamFSRef _Nonnull self, InodeRef _Nonnull _Locked pNodeToUnlink, InodeRef _Nonnull _Locked pParentNode, User user)
{
    decl_try_err();

    // We must have write permissions for 'pParentNode'
    try(RamFS_CheckAccess_Locked(self, pParentNode, user, kFilePermission_Write));


    // A directory must be empty in order to be allowed to unlink it
    if (Inode_IsDirectory(pNodeToUnlink) && !DirectoryNode_IsEmpty(pNodeToUnlink)) {
        throw(EBUSY);
    }


    // Remove the directory entry in the parent directory
    try(RamFS_RemoveDirectoryEntry(self, pParentNode, Inode_GetId(pNodeToUnlink)));


    // Unlink the node itself
    Inode_Unlink(pNodeToUnlink);

catch:
    return err;
}

// Renames the node with name 'pName' and which is an immediate child of the
// node 'pParentNode' such that it becomes a child of 'pNewParentNode' with
// the name 'pNewName'. All nodes are guaranteed to be owned by the filesystem.
ErrorCode RamFS_rename(RamFSRef _Nonnull self, const PathComponent* _Nonnull pName, InodeRef _Nonnull _Locked pParentNode, const PathComponent* _Nonnull pNewName, InodeRef _Nonnull _Locked pNewParentNode, User user)
{
    // XXX implement me
    return EACCESS;
}


CLASS_METHODS(RamFS, Filesystem,
OVERRIDE_METHOD_IMPL(deinit, RamFS, Object)
OVERRIDE_METHOD_IMPL(onAllocateNodeOnDisk, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(onReadNodeFromDisk, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(onWriteNodeToDisk, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(onRemoveNodeFromDisk, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(onMount, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(onUnmount, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(acquireRootNode, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(acquireNodeForName, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(getNameOfNode, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(getFileInfo, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(setFileInfo, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(createDirectory, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(openDirectory, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(readDirectory, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(read, RamFS, IOResource)
OVERRIDE_METHOD_IMPL(write, RamFS, IOResource)
OVERRIDE_METHOD_IMPL(checkAccess, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(unlink, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(rename, RamFS, Filesystem)
);
