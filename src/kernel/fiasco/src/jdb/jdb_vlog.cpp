IMPLEMENTATION:

#include "jdb_module.h"
#include "jdb_kobject.h"
#include "static_init.h"
#include "vlog.h"

class Jdb_vlog : public Jdb_kobject_handler
{};

PUBLIC FIASCO_INIT
Jdb_vlog::Jdb_vlog()
: Jdb_kobject_handler((Vlog *)0)
{ Jdb_kobject::module()->register_handler(this); }

PUBLIC
bool
Jdb_vlog::show_kobject(Kobject_common *, int)
{ return true; }

PUBLIC
char const *
Jdb_vlog::kobject_type(Kobject_common *) const
{ return "Vlog"; }

static Jdb_vlog jdb_vlog INIT_PRIORITY(JDB_MODULE_INIT_PRIO);
