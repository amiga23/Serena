//
//  _aligndef.h
//  Apollo
//
//  Created by Dietmar Planitzer on 9/6/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef __ALIGNDEF_H
#define __ALIGNDEF_H 1

#define _Alignof(type) __alignof(type)
#define alignof(type) __alignof(type)

#define __Ceil_PowerOf2(x, mask)   (((x) + (mask)) & ~(mask))
#define __Floor_PowerOf2(x, mask) ((x) & ~(mask))

#endif /* __ALIGNDEF_H */
