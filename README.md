# libx1000

This repository contains reworked **libx1000**, that fixes the LOCK prefix segfault on *Intel Quark X1000*.

This version of **[libx1000](https://github.com/assa77/libx1000)** was developed by *[Alexander M. Albertian](mailto:assa@4ip.ru)*.

Original **libx1000** was developed in 2015 by *[Ray Kinsella](mailto:ray.kinsella@intel.com)*.

You can see his blog for the [original description of libx1000](http://mdr78.github.io/x1000/2016/10/21/fixing-lock-prefix-on-x1000.html).

## Original description

After much fun installing *Debian 7* (*Wheezy*) on an *Intel Galileo Gen 2* I was greated by the following at the login prompt.
```
login: segfault at b7173107 ip b714f07b sp bf97ea94 error ffff0007 in libpthread-2.13.so[b714a000+15000]
```
The *X1000 SOC* used on the Intel Galileo’s suffers from a bug in it’s instruction set. It’s pretty well documented on *[Wikipedia](https://en.wikipedia.org/wiki/Intel_Quark#Segfault_bug")* and the *[Debian Bugzilla](https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=738575")*. This is a particularly nasty bug in the *X1000 ISA*, that causes the preciding instruction to be re-executed when a the *LOCK* instruction prefix coincides with a page fault.

The *LOCK* instruction itself is un-necessarily on the *X1000*, as it is a single core SOC and there are no other cores to signal with the *LOCK* prefix. The approach of distributions like Buildroot and Yocto is to strip the *LOCK* prefix during compile time with the *[-momit-lock-prefix](https://sourceware.org/ml/binutils/2014-08/msg00043.html)* argument to the assembler. This approach works fine for roll-your-own distributions like these, but doesn’t help much with trying to run *Debian* on the *Galileo*.

The approach adopted by *[UbiLinux](https://solutionsdirectory.intel.com/solutions-directory/ubilinux-debian-linux-intel%C2%AE-edison-and-intel%C2%AE-galileo-platforms)*, a *Debian Linux* derived distribution was to package their own glibc and pthread versions that where patched to remove the *LOCK* prefix. This model works fine also but I wanted to run *Debian* not a *Galileo* specific-derivative.

Similarly another possible approach is to create a new hardware capability (*hwcap*) to indicate the lock prefix issue to the runtime linker. *HWCaps* are created at runtime and are communicated to the runtime linker through [vd.so](http://man7.org/linux/man-pages/man7/vdso.7.html) - a library the linux kernel inserts at runtime into the address space of all applications. This is how the runtime linker '*knows*' to load cpu optimized versions of libraries. This is why the runtime linker might cause an application to load `/lib/i686/cmov/libc.so.6` instead of `/lib/libc.so.6` on an architecture that supports the *cmov* instruction. Similarly you might have `/lib/i586/nlp/libc.so.6` to indicate no-lock-prefix (*nlp*).

This got me thinking about where the *LOCK* prefix actually occurs in applications and more importantly where they might conincide with a page fault. Typically when we call the *LOCK* prefix with `spin_lock*()` and friends, the memory has already been faulted as we initialize the spinlock before use.
```C
int some_func(int some_arg){

spinlock_t x = SPIN_LOCK_UNLOCKED;

spin_lock(x);
...
```
This holds for most variables on the stack and the heap, that we explicitly initialize them and fault them into memory before use (i.e. before they are *LOCK*’ed). However for variables that we decare and initalize globally these are typically faulted into memory initalized.
```C
spinlock_t x = SPIN_LOCK_UNLOCKED;

int some_func(int some_arg){

spin_lock(x);
...
```
These variables that are declared and initalized globally are described in the `.bss` section.
```bash
[rkinsell@localhost _posts]$ readelf -S /bin/bash
There are 29 section headers, starting at offset 0x105438:

Section Headers:
[Nr] Name              Type             Address           Offset
  	Size              EntSize          Flags  Link  Info  Align
[ 0]                   NULL             0000000000000000  00000000
   		0000000000000000  0000000000000000           0     0     0
[ 1] .interp           PROGBITS         0000000000000238  00000238
   		000000000000001c  0000000000000000   A       0     0     1
...
[23] .got              PROGBITS         00000000002fa830  000fa830
   		00000000000007c0  0000000000000008  WA       0     0     8
[24] .data             PROGBITS         00000000002fb000  000fb000
   		0000000000008568  0000000000000000  WA       0     0     32
[25] .bss              NOBITS           0000000000303580  00103568
   		00000000000059d8  0000000000000000  WA       0     0     32
...
```
However as it happen this is not what is going in the segfault in `login/pthread` shown above. The other system behaviour that can cause a page fault to coincide as a *LOCK* is a `fork()`.
```C
spinlock_t x;

int some_func(int some_arg){

x = SPIN_LOCK_UNLOCKED;

if(fork() &gt; 0) {
	spin_lock(x);

    ...
```
What is happening in the above code sample is the parent process initializes the spinlock and forks, this causes the child process to reuse the pages of parent process however these pages in the child are marked '*copy-on-write*'. The pages only become writeable when the child process tries to write to these pages, a page fault is generated and the fault handler copies the page into a new page in the child process marked writeable. Clearly if write trigging the fault is a `spin_lock()`, we get a segfault and that is what is happening at login.

If we had a way to insert ourselves into address space of all applications like `vd.so` to prefault `.bss` etc, it would solve *98%* of the problems without necessitating creating a *X1000* optimized version of each package. Turns out the runtime linker provides a facility for this in *LD preload*. This facility can accessed either by indicating a library with the `LD_PRELOAD` environmental variable or adding the library to the configuration file `/etc/ld.so.preload`. I developed a library that does exactly this called **libx1000**. You can find original version of it on [github](https://github.com/mdr78/libx1000) with all the necessary Debian packaging to both install the library and setup `/etc/ld.so.preload`.

It hooks `main()`, `fork()` and `mmap()` to prefault memory using `mlock()`. It has a configuration options to allow the user to mlock just the data segments like `.bss`, and/or the stack and the heap, and/or mmap'ed memory - figuring out which is which using `proc/self/maps`.
```ini
#MEMLOCK indicates what to lock in memory
# DS=Data Segment
# HS=Heap &amp; Stack
# MM=All MMAPs
MEMLOCK=DS|HS|MM
#EXECHOOK indicates when to apply the mlock
# PI=PreInit
# FK=Fork
# MM=MemMap
EXECHOOK=PI|FK|MM
```

You make an application specific configuration e.g. `login.conf` or a global configuration `defaults.conf`, that can be placed in either the applications working directory or `/etc/libx1000.so.d/`.

Once the library was installed, *Debian Wheezy* worked purfectly on the *Galileo*.
