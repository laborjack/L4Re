/*****************************************************************************/
/**
 * \file
 * \brief   L4 kernel event tracing
 * \ingroup api_calls
 */
/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Björn Döbel <doebel@os.inf.tu-dresden.de>
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

L4_INLINE l4_tracebuffer_status_t *
fiasco_tbuf_get_status(void)
{
  l4_tracebuffer_status_t *tbuf;
  asm("int $3; cmpb $29, %%al" : "=a" (tbuf) : "0" (0));
  return tbuf;
}

L4_INLINE l4_addr_t
fiasco_tbuf_get_status_phys(void)
{
  l4_addr_t tbuf_phys;
  asm("int $3; cmpb $29, %%al" : "=a" (tbuf_phys) : "0" (5));
  return tbuf_phys;
}

L4_INLINE l4_umword_t
fiasco_tbuf_log(const char *text)
{
  l4_umword_t offset;
  asm volatile("int $3; cmpb $29, %%al"
	      : "=a" (offset)
	      : "a" (1), "d" (text));
  return offset;
}

L4_INLINE l4_umword_t
fiasco_tbuf_log_3val(const char *text, l4_umword_t v1, l4_umword_t v2, l4_umword_t v3)
{
  l4_umword_t offset;
  asm volatile("int $3; cmpb $29, %%al"
	      : "=a" (offset)
	      : "a" (4), "d" (text), "c" (v1), "S" (v2), "D" (v3));
  return offset;
}

L4_INLINE void
fiasco_tbuf_clear(void)
{
  asm volatile("int $3; cmpb $29, %%al" : : "a" (2));
}

L4_INLINE void
fiasco_tbuf_dump(void)
{
  asm volatile("int $3; cmpb $29, %%al" : : "a" (3));
}

L4_INLINE void
fiasco_timer_disable(void)
{
  asm volatile("int $3; cmpb $29, %%al" : : "a" (6));
}

L4_INLINE void
fiasco_timer_enable(void)
{
  asm volatile("int $3; cmpb $29, %%al" : : "a" (7));
}

L4_INLINE l4_umword_t
fiasco_tbuf_log_binary(const unsigned char *data)
{
  l4_umword_t offset;
  asm volatile("int $3; cmpb $29, %%al"
               : "=a" (offset)
               : "a" (8), "d" (data));
  return offset;
}
