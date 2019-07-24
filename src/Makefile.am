
noinst_HEADERS = defs.h cmpvers.h dlist.h jsmin.h make-pkglist.h msglog.h pkglist.h system.h dialog-ui.h

sbin_PROGRAMS  = chrefs pkginfo pkglog make-package make-pkglist check-db-integrity check-package check-requires \
                 install-package remove-package update-package install-pkglist


chrefs_SOURCES             = chrefs.c system.c msglog.c
pkginfo_SOURCES            = pkginfo.c system.c msglog.c
pkglog_SOURCES             = pkglog.c system.c msglog.c

check_db_integrity_SOURCES = check-db-integrity.c system.c msglog.c cmpvers.c dlist.c jsmin.c pkglist.c
check_db_integrity_LDADD   = -lm

check_requires_SOURCES     = check-requires.c system.c msglog.c cmpvers.c dlist.c jsmin.c pkglist.c
check_requires_LDADD       = -lm

check_package_SOURCES      = check-package.c system.c msglog.c cmpvers.c

make_pkglist_SOURCES       = make-pkglist.c system.c msglog.c cmpvers.c dlist.c jsmin.c pkglist.c
make_pkglist_LDADD         = -lm

make_package_SOURCES       = make-package.c system.c msglog.c dlist.c
make_package_LDADD         = -lm

install_package_SOURCES    = install-package.c system.c msglog.c cmpvers.c dlist.c
install_package_LDADD      = -lm
if USE_DIALOG
  install_package_SOURCES += dialog-ui.c
  install_package_CFLAFS   = $(DIALOG_CFLAGS)
  install_package_LDFLAGS  = $(DIALOG_LDFLAGS)
  install_package_LDADD   += $(DIALOG_LIBS)
endif

remove_package_SOURCES     = remove-package.c system.c msglog.c cmpvers.c dlist.c
remove_package_LDADD       = -lm
if USE_DIALOG
  remove_package_SOURCES  += dialog-ui.c
  remove_package_CFLAFS    = $(DIALOG_CFLAGS)
  remove_package_LDFLAGS   = $(DIALOG_LDFLAGS)
  remove_package_LDADD    += $(DIALOG_LIBS)
endif

update_package_SOURCES     = update-package.c system.c msglog.c cmpvers.c dlist.c
update_package_LDADD       = -lm
if USE_DIALOG
  update_package_SOURCES  += dialog-ui.c
  update_package_CFLAFS    = $(DIALOG_CFLAGS)
  update_package_LDFLAGS   = $(DIALOG_LDFLAGS)
  update_package_LDADD    += $(DIALOG_LIBS)
endif

install_pkglist_SOURCES    = install-pkglist.c system.c msglog.c cmpvers.c dlist.c
install_pkglist_LDADD      = -lm -lpthread
if USE_DIALOG
  install_pkglist_SOURCES += dialog-ui.c
  install_pkglist_CFLAFS   = $(DIALOG_CFLAGS)
  install_pkglist_LDFLAGS  = $(DIALOG_LDFLAGS)
  install_pkglist_LDADD   += $(DIALOG_LIBS)
endif


pkgdata_DATA = .dialogrc
