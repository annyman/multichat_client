#ifndef MULTICHAT_CURSES_COMPAT_H
#define MULTICHAT_CURSES_COMPAT_H

/*
 * ncurses wide-character headers live in different places depending on the
 * distribution (e.g. Debian ships <ncursesw/ncurses.h>, Fedora ships
 * <ncurses.h>). Prefer the wide header when it is available.
 */
#if defined(__has_include)
#  if __has_include(<ncursesw/ncurses.h>)
#    include <ncursesw/ncurses.h>
#  else
#    include <ncurses.h>
#  endif
#else
#  include <ncurses.h>
#endif

#endif
