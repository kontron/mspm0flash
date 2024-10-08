// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2024 Kontron Europe GmbH
 *
 * Author: Heiko Thiery <heiko.thiery@kontron.com>
 * Created: May 18, 2024
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#define DEBUG(lvl, fmt, ...)                                    \
do {                                                            \
    if (verbosity > lvl) {                                      \
        fprintf(stderr, "%s() " fmt, __func__, ## __VA_ARGS__); \
    }                                                           \
} while (0)

#endif /* #ifndef __COMMON_H__ */
