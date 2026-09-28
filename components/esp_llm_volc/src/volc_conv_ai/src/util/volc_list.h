// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#ifndef __CONV_AI_SRC_UTIL_VOLC_LIST_H__
#define __CONV_AI_SRC_UTIL_VOLC_LIST_H__

#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct volc_list_head_t
  {
    struct volc_list_head_t *next, *prev;
  } volc_list_head_t;

#define __LIST_ADD(entry, before, after)                                 \
  {                                                                      \
    volc_list_head_t *new_ = (entry), *prev = (before), *next = (after); \
    (next)->prev = (new_);                                               \
    (new_)->next = (next);                                               \
    (new_)->prev = (prev);                                               \
    (prev)->next = (new_);                                               \
  }
#define volc_list_init(entry) \
  do                          \
  {                           \
    (entry)->next = (entry);  \
    (entry)->prev = (entry);  \
  } while (0)
#define volc_list_add(entry, base)             \
  do                                           \
  {                                            \
    __LIST_ADD((entry), (base), (base)->next); \
  } while (0)
#define volc_list_add_after(entry, base)       \
  do                                           \
  {                                            \
    __LIST_ADD((entry), (base), (base)->next); \
  } while (0)
#define volc_list_add_before(entry, base)      \
  do                                           \
  {                                            \
    __LIST_ADD((entry), (base)->prev, (base)); \
  } while (0)
#define volc_list_add_head(entry, head) volc_list_add_after(entry, head)
#define volc_list_add_tail(entry, head) volc_list_add_before(entry, head)
#define volc_list_del(entry)                 \
  do                                         \
  {                                          \
    (entry)->prev->next = (entry)->next;     \
    (entry)->next->prev = (entry)->prev;     \
    (entry)->next = (entry)->prev = (entry); \
  } while (0)
#define volc_list_empty(head) ((head)->next == (head))
#define volc_list_get_head(head) (volc_list_empty(head) ? (volc_list_head_t *)NULL : (head)->next)
#define volc_list_get_tail(head) (volc_list_empty(head) ? (volc_list_head_t *)NULL : (head)->prev)
#define volc_list_is_head(entry, head) ((entry)->prev == head)
#define volc_list_is_tail(entry, head) ((entry)->next == head)
#define volc_list_entry(ptr, type, member) ((type *)((char *)(ptr) - (unsigned long)(&((type *)0)->member)))
#define volc_list_for_each(h, head) for (h = (head)->next; h != (head); h = h->next)
#define volc_list_for_each_safe(h, n, head) for (h = (head)->next, n = h->next; h != (head); h = n, n = h->next)
#define volc_list_for_each_entry(p, head, type, member)                       \
  for (p = volc_list_entry((head)->next, type, member); &p->member != (head); \
       p = volc_list_entry(p->member.next, type, member))
#define volc_list_for_each_entry_safe(p, t, head, type, member)                                            \
  for (p = volc_list_entry((head)->next, type, member), t = volc_list_entry(p->member.next, type, member); \
       &p->member != (head); p = t, t = volc_list_entry(t->member.next, type, member))
#define volc_list_get_head_entry(head, type, member) \
  (volc_list_empty(head) ? (type *)NULL : volc_list_entry(((head)->next), type, member))
#define volc_list_get_tail_entry(head, type, member) \
  (volc_list_empty(head) ? (type *)NULL : volc_list_entry(((head)->prev), type, member))

#ifdef __cplusplus
}
#endif
#endif /* #define __CONV_AI_SRC_UTIL_VOLC_LIST_H__ */
