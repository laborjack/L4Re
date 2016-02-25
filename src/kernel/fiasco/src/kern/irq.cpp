INTERFACE:

#include "ipc_sender.h"
#include "irq_chip.h"
#include "kobject_helper.h"
#include "member_offs.h"
#include "sender.h"
#include "context.h"

class Ram_quota;
class Thread;


/** Hardware interrupts.  This class encapsulates handware IRQs.  Also,
    it provides a registry that ensures that only one receiver can sign up
    to receive interrupt IPC messages.
 */
class Irq : public Irq_base, public cxx::Dyn_castable<Irq, Kobject>
{
  MEMBER_OFFSET();
  typedef Slab_cache Allocator;

public:
  enum Op
  {
    Op_eoi_1      = 0, // Irq_sender + Irq_semaphore
    Op_compat_attach     = 1,
    Op_trigger    = 2, // Irq_sender + Irq_mux + Irq_semaphore
    Op_compat_chain      = 3,
    Op_eoi_2      = 4, // Icu + Irq_sender + Irq_semaphore
    Op_compat_detach     = 5,
  };

protected:
  Ram_quota *_q;
  Context::Drq _drq;
};


/**
 * IRQ Kobject to send IPC messages to a receiving thread.
 */
class Irq_sender
: public Kobject_h<Irq_sender, Irq>,
  public Ipc_sender<Irq_sender>
{
public:
  enum Op {
    Op_attach = 0,
    Op_detach = 1
  };

protected:
  Smword _queued;
  Thread *_irq_thread;

private:
  Mword _irq_id;
};


/**
 * IRQ Kobject to broadcast IRQs to multiple other IRQ objects.
 *
 * This is useful for PCI shared IRQs.
 */
class Irq_muxer : public Kobject_h<Irq_muxer, Irq>, private Irq_chip
{
public:
  enum Ops {
    Op_chain = 0
  };

  int set_mode(Mword, Irq_chip::Mode) { return 0; }
  bool is_edge_triggered(Mword) const { return false; }
  void switch_mode(bool)
  {
    // the irq object is assumed to be always handled as
    // level triggered
  }

  void set_cpu(Mword, Cpu_number)
  {
    // don't know what to do here, may be multiple targets on different
    // CPUs!
  }

  void ack(Mword) {}

  char const *chip_type() const { return "Bcast"; }

private:
  Smword _mask_cnt;
  Spin_lock<> _mux_lock;
};

//-----------------------------------------------------------------------------
IMPLEMENTATION:

#include "assert_opt.h"
#include "atomic.h"
#include "config.h"
#include "cpu_lock.h"
#include "entry_frame.h"
#include "globals.h"
#include "ipc_sender.h"
#include "kmem_slab.h"
#include "lock_guard.h"
#include "minmax.h"
#include "std_macros.h"
#include "thread_object.h"
#include "thread_state.h"
#include "l4_buf_iter.h"
#include "vkey.h"

namespace {
static Irq_base *irq_base_dcast(Kobject_iface *o)
{ return cxx::dyn_cast<Irq*>(o); }

struct Irq_base_cast
{
  Irq_base_cast()
  { Irq_base::dcast = &irq_base_dcast; }
};

static Irq_base_cast register_irq_base_cast;
}

PROTECTED inline
L4_msg_tag
Irq::dispatch_irq_proto(Unsigned16 op, bool may_unmask)
{
  switch (op)
    {
    case Op_eoi_1:
    case Op_eoi_2:
      if (may_unmask)
        unmask();
      return L4_msg_tag(L4_msg_tag::Schedule); // no reply

    case Op_trigger:
      log();
      hit(0);
      return L4_msg_tag(L4_msg_tag::Schedule); // no reply

    default:
      return commit_result(-L4_err::ENosys);
    }
}

PUBLIC
void
Irq_muxer::unmask(Mword)
{
  Smword old;
  do
    old = _mask_cnt;
  while (!mp_cas(&_mask_cnt, old, old - 1));

  if (old == 1)
    Irq_base::unmask();
}


PUBLIC
void
Irq_muxer::mask(Mword)
{
  Smword old;
  do
    old = _mask_cnt;
  while (!mp_cas(&_mask_cnt, old, old + 1));

  if (old == 0)
    Irq_base::mask();
}


PUBLIC
void
Irq_muxer::unbind(Irq_base *irq)
{
  Irq_base *n;
    {
      auto g = lock_guard(_mux_lock);
      for (n = this; n->_next && n->_next != irq; n = n->_next)
        ;

      if (n->_next != irq)
        return; // someone else was faster

      // dequeue
      n->_next = n->_next->_next;
    }

  if (irq->masked())
    static_cast<Irq_chip&>(*this).unmask(0);

  Irq_chip::unbind(irq);
}


PUBLIC
void
Irq_muxer::mask_and_ack(Mword)
{}

PUBLIC inline
void
Irq_muxer::handle(Upstream_irq const *ui)
{
  assert (cpu_lock.test());
  Irq_base::mask_and_ack();
  ui->ack();

  if (EXPECT_FALSE (!Irq_base::_next))
    return;

  int irqs = 0;
  for (Irq_base *n = Irq_base::_next; n;)
    {
      ++irqs;
      n->__mask();
      n = n->Irq_base::_next;
    }

    {
      Smword old;
      do
	old = _mask_cnt;
      while (!mp_cas(&_mask_cnt, old, old + irqs));
    }

  for (Irq_base *n = Irq_base::_next; n;)
    {
      Irq *i = nonull_static_cast<Irq*>(n);
      i->hit(0);
      n = i->Irq_base::_next;
    }
}

PUBLIC explicit
Irq_muxer::Irq_muxer(Ram_quota *q = 0)
: Kobject_h<Irq_muxer, Irq>(q), _mask_cnt(0),
  _mux_lock(Spin_lock<>::Unlocked)
{
  hit_func = &handler_wrapper<Irq_muxer>;
}

PUBLIC
void
Irq_muxer::destroy(Kobject ***rl)
{
  while (Irq_base *n = Irq_base::_next)
    {
      auto g  = lock_guard(n->irq_lock());
      if (n->chip() == this)
        unbind(n);
    }

  Irq::destroy(rl);
}

PRIVATE
L4_msg_tag
Irq_muxer::sys_attach(L4_msg_tag tag, Utcb const *utcb, Syscall_frame *)
{
  Ko::Rights rights;
  Irq *irq = Ko::deref<Irq>(&tag, utcb, &rights);
  if (!irq)
    return tag;

  auto g = lock_guard(irq->irq_lock());
  irq->unbind();

  if (!irq->masked())
    {
      Smword old;
      do
        old = _mask_cnt;
      while (!mp_cas(&_mask_cnt, old, old + 1));
    }

  bind(irq, 0);

  auto mg = lock_guard(_mux_lock);
  irq->Irq_base::_next = Irq_base::_next;
  Irq_base::_next = irq;

  return commit_result(0);
}

PUBLIC
L4_msg_tag
Irq_muxer::kinvoke(L4_obj_ref, L4_fpage::Rights /*rights*/, Syscall_frame *f,
                   Utcb const *utcb, Utcb *)
{
  L4_msg_tag tag = f->tag();

  if (EXPECT_FALSE(tag.words() < 1))
    return commit_result(-L4_err::EInval);

  Unsigned16 op = access_once(utcb->values + 0) & 0xffff;

  switch (tag.proto())
    {
    case L4_msg_tag::Label_irq:
      // start BACKWARD COMPAT
      switch (op)
        {
        case Op_compat_chain:
          printf("KERNEL: backward compat IRQ-MUX chain, recompile your user code");
          return sys_attach(tag, utcb, f);
        default:
          break;
        }
      // end BACKWARD COMPAT
      return dispatch_irq_proto(op, false);

    case L4_msg_tag::Label_irq_mux:
      switch (op)
        {
        case Op_chain:
          return sys_attach(tag, utcb, f);

        default:
          return commit_result(-L4_err::ENosys);
        }

    default:
      return commit_result(-L4_err::EBadproto);
    }
}

/** Bind a receiver to this device interrupt.
    @param t the receiver that wants to receive IPC messages for this IRQ
    @return true if the binding could be established
 */
PUBLIC inline NEEDS ["atomic.h", "cpu_lock.h", "lock_guard.h"]
bool
Irq_sender::alloc(Thread *t)
{
  bool ret = mp_cas(&_irq_thread, reinterpret_cast<Thread*>(0), t);

  if (ret)
    {
      if (EXPECT_TRUE(t != 0))
        {
          t->inc_ref();
          if (Cpu::online(t->home_cpu()))
            _chip->set_cpu(pin(), t->home_cpu());
        }

      _queued = 0;
    }

  return ret;
}

PUBLIC
Receiver *
Irq_sender::owner() const { return _irq_thread; }

/** Release an device interrupt.
    @param t the receiver that ownes the IRQ
    @return true if t really was the owner of the IRQ and operation was 
            successful
 */
PRIVATE
bool
Irq_sender::free(Thread *t, Kobject ***rl)
{
  bool ret = mp_cas(&_irq_thread, t, reinterpret_cast<Thread*>(0));

  if (ret)
    {
      auto guard = lock_guard(cpu_lock);
      mask();

      if (EXPECT_TRUE(t != 0))
	{
	  t->Receiver::abort_send(this);

	  // release cpu-lock early, actually before delete
	  guard.reset();

          t->put_n_reap(rl);
	}
    }

  return ret;
}

PUBLIC explicit
Irq_sender::Irq_sender(Ram_quota *q = 0)
: Kobject_h<Irq_sender, Irq>(q), _queued(0), _irq_thread(0), _irq_id(~0UL)
{
  hit_func = &hit_level_irq;
}

PUBLIC
void
Irq_sender::switch_mode(bool is_edge_triggered)
{
  hit_func = is_edge_triggered ? &hit_edge_irq : &hit_level_irq;
}

PUBLIC
void
Irq_sender::destroy(Kobject ***rl)
{
  auto g = lock_guard(cpu_lock);
  auto t = access_once(&_irq_thread);
  if (t)
    free(t, rl);

  Irq::destroy(rl);
}


/** Consume one interrupt.
    @return number of IRQs that are still pending.
 */
PRIVATE inline NEEDS ["atomic.h"]
Smword
Irq_sender::consume()
{
  Smword old;

  do
    {
      old = _queued;
    }
  while (!mp_cas (&_queued, old, old - 1));

  if (old == 2 && hit_func == &hit_edge_irq)
    unmask();

  return old - 1;
}

PUBLIC inline
int
Irq_sender::queued()
{
  return _queued;
}


/**
 * Predicate used to figure out if the sender shall be enqueued
 * for sending a second message after sending the first.
 */
PUBLIC inline NEEDS[Irq_sender::consume]
bool
Irq_sender::requeue_sender()
{ return consume() > 0; }

/**
 * Predicate used to figure out if the sender shall be deqeued after
 * sending the request.
 */
PUBLIC inline NEEDS[Irq_sender::consume]
bool
Irq_sender::dequeue_sender()
{ return consume() < 1; }

PUBLIC inline
Syscall_frame *
Irq_sender::transfer_msg(Receiver *recv)
{
  Syscall_frame* dst_regs = recv->rcv_regs();

  // set ipc return value: OK
  dst_regs->tag(L4_msg_tag(0));

  // set ipc source thread id
  dst_regs->from(_irq_id);

  return dst_regs;
}

PUBLIC void
Irq_sender::modify_label(Mword const *todo, int cnt)
{
  for (int i = 0; i < cnt*4; i += 4)
    {
      Mword const test_mask = todo[i];
      Mword const test      = todo[i+1];
      if ((_irq_id & test_mask) == test)
	{
	  Mword const set_mask = todo[i+2];
	  Mword const set      = todo[i+3];

	  _irq_id = (_irq_id & ~set_mask) | set;
	  return;
	}
    }
}


PRIVATE static
Context::Drq::Result
Irq_sender::handle_remote_hit(Context::Drq *, Context *, void *arg)
{
  Irq_sender *irq = (Irq_sender*)arg;
  irq->set_cpu(current_cpu());
  if (EXPECT_TRUE(irq->send_msg(irq->_irq_thread, false)))
    return Context::Drq::no_answer_resched();
  return Context::Drq::no_answer();
}

PRIVATE inline
Smword
Irq_sender::queue()
{
  Smword old;
  do
    old = _queued;
  while (!mp_cas(&_queued, old, old + 1));
  return old;
}


PRIVATE inline
void
Irq_sender::count_and_send(Smword queued)
{
  if (EXPECT_TRUE (queued == 0) && EXPECT_TRUE(_irq_thread != 0))	// increase hit counter
    {
      if (EXPECT_FALSE(_irq_thread->home_cpu() != current_cpu()))
	_irq_thread->drq(&_drq, handle_remote_hit, this,
	                 Context::Drq::Target_ctxt, Context::Drq::No_wait);
      else
	send_msg(_irq_thread, true);
    }
}


PUBLIC inline NEEDS[Irq_sender::count_and_send, Irq_sender::queue]
void
Irq_sender::_hit_level_irq(Upstream_irq const *ui)
{
  // We're entered holding the kernel lock, which also means irqs are
  // disabled on this CPU (XXX always correct?).  We never enable irqs
  // in this stack frame (except maybe in a nonnested invocation of
  // switch_exec() -> switchin_context()) -- they will be re-enabled
  // once we return from it (iret in entry.S:all_irqs) or we switch to
  // a different thread.

  // LOG_MSG_3VAL(current(), "IRQ", dbg_id(), 0, _queued);

  assert (cpu_lock.test());
  mask_and_ack();
  ui->ack();
  count_and_send(queue());
}

PRIVATE static
void
Irq_sender::hit_level_irq(Irq_base *i, Upstream_irq const *ui)
{ nonull_static_cast<Irq_sender*>(i)->_hit_level_irq(ui); }

PUBLIC inline NEEDS[Irq_sender::count_and_send, Irq_sender::queue]
void
Irq_sender::_hit_edge_irq(Upstream_irq const *ui)
{
  // We're entered holding the kernel lock, which also means irqs are
  // disabled on this CPU (XXX always correct?).  We never enable irqs
  // in this stack frame (except maybe in a nonnested invocation of
  // switch_exec() -> switchin_context()) -- they will be re-enabled
  // once we return from it (iret in entry.S:all_irqs) or we switch to
  // a different thread.

  // LOG_MSG_3VAL(current(), "IRQ", dbg_id(), 0, _queued);

  assert (cpu_lock.test());
  Smword q = queue();

  // if we get a second edge triggered IRQ before the first is
  // handled we can mask the IRQ.  The consume function will
  // unmask the IRQ when the last IRQ is dequeued.
  if (!q)
    ack();
  else
    mask_and_ack();

  ui->ack();
  count_and_send(q);
}

PRIVATE static
void
Irq_sender::hit_edge_irq(Irq_base *i, Upstream_irq const *ui)
{ nonull_static_cast<Irq_sender*>(i)->_hit_edge_irq(ui); }


PRIVATE
L4_msg_tag
Irq_sender::sys_attach(L4_msg_tag tag, Utcb const *utcb,
                       Syscall_frame *)
{
  Thread *thread;

  if (tag.items())
    {
      Ko::Rights rights;
      thread = Ko::deref<Thread>(&tag, utcb, &rights);
      if (!thread)
        return tag;
    }
  else
    thread = current_thread();

  if (alloc(thread))
    {
      _irq_id = utcb->values[1];
      return commit_result(0);
    }

  return commit_result(-L4_err::EInval);
}

PRIVATE
L4_msg_tag
Irq_sender::sys_detach()
{
  Reap_list rl;
  free(_irq_thread, rl.list());
  _irq_id = ~0UL;
  cpu_lock.clear();
  rl.del();
  cpu_lock.lock();
  return commit_result(0);
}


PUBLIC
L4_msg_tag
Irq_sender::kinvoke(L4_obj_ref, L4_fpage::Rights /*rights*/, Syscall_frame *f,
                    Utcb const *utcb, Utcb *)
{
  L4_msg_tag tag = f->tag();

  if (EXPECT_FALSE(tag.words() < 1))
    return commit_result(-L4_err::EInval);

  Unsigned16 op = access_once(utcb->values + 0);

  switch (tag.proto())
    {
    case L4_msg_tag::Label_irq:
      // start BACKWARD COMPAT
      switch (op)
        {
        case Op_compat_attach:
          printf("KERNEL: backward compat IRQ attach, recompile your user code\n");
          return sys_attach(tag, utcb, f);
        case Op_compat_detach:
          printf("KERNEL: backward compat IRQ detach, recompile your user code\n");
          return sys_detach();
        default:
          break;
        }
      // end BACKWARD COMPAT
      return dispatch_irq_proto(op, _queued < 1);

    case L4_msg_tag::Label_irq_sender:
      switch (op)
        {
        case Op_attach:
          return sys_attach(tag, utcb, f);

        case Op_detach:
          return sys_detach();

        default:
          return commit_result(-L4_err::ENosys);
        }
    default:
      return commit_result(-L4_err::EBadproto);
    }
}

PUBLIC
Mword
Irq_sender::obj_id() const
{ return _irq_id; }



 // Irq implementation

static Kmem_slab _irq_allocator(max(sizeof (Irq_sender), sizeof(Irq_muxer)),
                                __alignof__ (Irq), "Irq");

PRIVATE static
Irq::Allocator *
Irq::allocator()
{ return &_irq_allocator; }

PUBLIC inline
void *
Irq::operator new (size_t, void *p)
{ return p; }

PUBLIC
void
Irq::operator delete (void *_l)
{
  Irq *l = reinterpret_cast<Irq*>(_l);
  if (l->_q)
    l->_q->free(sizeof(Irq));

  allocator()->free(l);
}

PUBLIC template<typename T> inline NEEDS[Irq::allocator, Irq::operator new]
static
T*
Irq::allocate(Ram_quota *q)
{
  void *nq =allocator()->q_alloc(q);
  if (nq)
    return new (nq) T(q);

  return 0;
}


PUBLIC explicit inline
Irq::Irq(Ram_quota *q = 0) : _q(q) {}

PUBLIC
void
Irq::destroy(Kobject ***rl)
{
  Irq_base::destroy();
  Kobject::destroy(rl);
}

namespace {
static Kobject_iface * FIASCO_FLATTEN
irq_sender_factory(Ram_quota *q, Space *,
                   L4_msg_tag, Utcb const *,
                   int *err)
{
  *err = L4_err::ENomem;
  return Irq::allocate<Irq_sender>(q);
}

static Kobject_iface * FIASCO_FLATTEN
irq_mux_factory(Ram_quota *q, Space *,
                L4_msg_tag, Utcb const *,
                int *err)
{
  *err = L4_err::ENomem;
  return Irq::allocate<Irq_muxer>(q);
}

static inline void __attribute__((constructor)) FIASCO_INIT
register_factory()
{
  Kobject_iface::set_factory(L4_msg_tag::Label_irq_sender, irq_sender_factory);
  Kobject_iface::set_factory(L4_msg_tag::Label_irq_mux, irq_mux_factory);
}
}
