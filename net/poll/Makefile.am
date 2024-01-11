SUBDIRS = .  tests

ACLOCAL_AMFLAGS = -I m4
AM_MAKEFLAGS = -s

lib_LTLIBRARIES = libpoll.la

# current:revision:age, do all of the following for each release:
#  source changes: revision++
#  interface added, removed, or changed: current++, revision = 0
#  interfaces added: age++
#  interfaces removed: age = 0
libpoll_la_LDFLAGS = -version-info 1:0:1

libpoll_la_SOURCES = \
	tester.c

nobase_include_HEADERS = \
	tester.h

AM_CFLAGS = -Wall
