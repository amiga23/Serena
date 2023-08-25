//
//  assert.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _ASSERT_H
#define _ASSERT_H 1

#include <stdnoreturn.h>

extern _Noreturn _Abort(const char* pFilename, int lineNum, const char* pFuncName);

#if NDEBUG
#define assert(cond)    ((void)0)
#else
#define assert(cond)   if ((cond) == 0) { _Abort(__FILE__, __LINE__, __func__); }
#endif

#endif /* _ASSERT_H */
