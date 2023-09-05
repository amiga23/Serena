//
//  stdbool.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _STDBOOL_H
#define _STDBOOL_H 1

#include <_cmndef.h>

__CPP_BEGIN

#ifndef __bool_true_false_are_defined

//typedef unsigned char _Bool;
#define bool unsigned char //_Bool
#define true    1
#define false   0
#define __bool_true_false_are_defined 1
#endif

__CPP_END

#endif /* _STDBOOL_H */
