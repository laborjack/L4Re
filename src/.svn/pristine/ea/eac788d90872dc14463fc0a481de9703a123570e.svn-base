IMPLEMENTATION:

#include "jdb_module.h"
#include "jdb_kobject.h"
#include "static_init.h"
#include "irq_controller.h"

class Jdb_icu : public Jdb_kobject_handler
{};

PUBLIC FIASCO_INIT
Jdb_icu::Jdb_icu()
: Jdb_kobject_handler((Icu *)0)
{ Jdb_kobject::module()->register_handler(this); }

PUBLIC
bool
Jdb_icu::show_kobject(Kobject_common *, int)
{ return true; }

PUBLIC
char const *
Jdb_icu::kobject_type(Kobject_common *) const
{ return "Icu/Pfc"; }

static Jdb_icu jdb_icu INIT_PRIORITY(JDB_MODULE_INIT_PRIO);
