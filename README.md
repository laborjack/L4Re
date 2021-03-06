Welcome to Fiasco.OC!
----------------------

Fiasco is a microkernel and an be used to construct flexible systems. We at
TU Dresden use it as a base for our TUD:OS system which supports running
real-time and time-sharing applications as well as virtualization
concurrently on one system. However, Fiasco is not only suitable for big and
complex systems, but also for small, embedded applications.

This distribution contains the Fiasco.OC microkernel and tools to build
it. User level applications are not included in this package.

Licensing
---------

Fiasco is freely redistributable under the GPL (see l4/kernel/fiasco/COPYING).
For different licensing schemes please contact us at:
  fiasco-core@os.inf.tu-dresden.de
This is a closed list, proper mails will be let through by the moderator.

For more details, see our webpage at: http://os.inf.tu-dresden.de/fiasco/

Supported architectures and ABIs
--------------------------------

  * IA32  (32-bit Intel, AMD and compatible CPUs, i486 and above)
  * AMD64 (64-bit Intel, AMD and compatible CPUs)
  * ARM   (various ARM CPUs and platforms)
  * UX    (Linux usermode emulation of Fiasco/L4)

A list of changes since the last release can be found in the CHANGES file.

Building the Fiasco kernel
--------------------------

To compile Fiasco you need a recent version of gcc and a recent version of
GNU binutils, GNU make and Perl version 5.6 or later.

Change to the directory fiasco/l4/kernel/fiasco/ and
create a build directory with "make BUILDDIR=build" and change to the
'build' directory. Optionally modify the default configuration using
"make menuconfig". Then compile the kernel by typing "make".

The kernel image can then be found in fiasco/l4/kernel/fiasco/build/ as
'fiasco'.

For further information refer to: http://os.inf.tu-dresden.de/fiasco/build.html

Mailing List
------------

For problem reports and suggestions regarding the Fiasco microkernel,
please subscribe and send mail to the l4-hackers mailing list:
   http://os.inf.tu-dresden.de/mailman/listinfo/l4-hackers/

Remote SVN
----------

The latest development versions are available from our SVN server.
The truly bleeding edge will want to check out daily snapshots from there.
However, be aware that these may not always run correctly.
You can also get user land programs for L4 from this SVN.

For information how to use SVN see:
http://os.inf.tu-dresden.de/L4Re/download.html


Enjoy!

The Fiasco Team
