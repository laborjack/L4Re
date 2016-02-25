/**
 * \file
 * \brief   L4 kernel event tracing
 * \ingroup api_calls
 */
/*
 * (c) 2008-2015 Adam Lackorzynski <adam@l4re.org>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 *
 * As a special exception, you may use this file as part of a free software
 * library without restriction.  Specifically, if other files instantiate
 * templates or use macros or inline functions from this file, or you compile
 * this file and link it with other files to produce an executable, this
 * file does not by itself cause the resulting executable to be covered by
 * the GNU General Public License.  This exception does not however
 * invalidate any other reasons why the executable file might be covered by
 * the GNU General Public License.
 */
/*****************************************************************************/
#pragma once

#ifndef __L4_KTRACE_H__
#error "Do not include this file directly."
#endif

#include <l4/sys/kdebug.h>

L4_INLINE l4_tracebuffer_status_t *
fiasco_tbuf_get_status(void)
{
  /* Not implemented */
  return (l4_tracebuffer_status_t *)__KDEBUG_ARM_PARAM_1(29, 0);
}

L4_INLINE l4_addr_t
fiasco_tbuf_get_status_phys(void)
{
  enter_kdebug("fiasco_tbuf_get_status_phys not there");
  return ~0UL;
}

L4_INLINE l4_umword_t
fiasco_tbuf_log(const char *text)
{
  return __KDEBUG_ARM_PARAM_2(29, 1, text);
}

L4_INLINE l4_umword_t
fiasco_tbuf_log_3val(const char *text, l4_umword_t v1, l4_umword_t v2, l4_umword_t v3)
{
  return __KDEBUG_ARM_PARAM_5(29, 4, text, v1, v2, v3);
}

L4_INLINE void
fiasco_tbuf_clear(void)
{
  __KDEBUG_ARM_PARAM_1(29, 2);
}

L4_INLINE void
fiasco_tbuf_dump(void)
{
  __KDEBUG_ARM_PARAM_1(29, 3);
}

L4_INLINE void
fiasco_timer_disable(void)
{
  __KDEBUG_ARM_PARAM_1(29, 6);
}

L4_INLINE void
fiasco_timer_enable(void)
{
  __KDEBUG_ARM_PARAM_1(29, 6);
}

L4_INLINE l4_umword_t
fiasco_tbuf_log_binary(const unsigned char *data)
{
  return __KDEBUG_ARM_PARAM_2(29, 8, data);
}
