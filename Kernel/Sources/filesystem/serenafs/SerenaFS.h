//
//  SerenaFS.h
//  kernel
//
//  Created by Dietmar Planitzer on 11/11/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef SerenaFS_h
#define SerenaFS_h

#include <filesystem/Filesystem.h>

OPAQUE_CLASS(SerenaFS, Filesystem);
typedef struct _SerenaFSMethodTable {
    FilesystemMethodTable   super;
} SerenaFSMethodTable;


// Formats the given disk drive and installs a SerenaFS with an empty root
// directory on it. 'user' and 'permissions' are the user and permissions that
// should be assigned to the root directory.
extern errno_t SerenaFS_FormatDrive(DiskDriverRef _Nonnull pDriver, User user, FilePermissions permissions);


// Creates an instance of SerenaFS. SerenaFS is a volatile file system that does not
// survive system restarts. The 'rootDirUser' parameter specifies the user and
// group ID of the root directory.
errno_t SerenaFS_Create(User rootDirUser, SerenaFSRef _Nullable * _Nonnull pOutFileSys);

#endif /* SerenaFS_h */
