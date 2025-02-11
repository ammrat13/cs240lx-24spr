## Lab: using single stepping to check interleaving.

Update May 28 tues:
 - add trylock (part 2)
 - add a yield system call (part 3) and rewrite some tests so they
   have B yield rather than fail.
 - add a couple non-ridiculous ai-generated tests and find bugs (or not
   if it gets it right!)
 - ideally as an extension you can do at least two context switches
   and have some tests to show that this works (e.g., write some 
   assembly so you know the exact number of instructions).
 - checked in: a new header (`code/is-mem.h`) for determining if a 
   instruction is a load or a store.
 - checked in: tests rewritten to use yield (`tests-2.0`).
   
We'll use single-stepping to write a concurrency checker that is tiny
but mighty.


In many tools, we'd want to run a single instruction and then get control
back for a tool.

For this lab you'll have to read the comments. Sorry :( 
  - check-interleave.h: has the the interface.
  - check-interleave.c: has a working version.
  - tests/ has tests.  As usual, test 0 is easier than test 1 etc.


-----------------------------------------------------------------------
### Background


#### Interface

The client interface is `checker_t:checker-interlace.h`.  The client gives us
a `checker_t` structure with four routines:
  1. `c->A()`, `c->B()`: the two routines to check.
  2. `c->init()`: initialize the state used by `A()` `B()`.   Calling this
      after running A() and B() should always set their data to the same
      initial values so that they are deterministic.

  3. `c->check()`: called after running `A(); B()`: returns 1
      if state was correct, 0 otherwise.  This routine uses code-specific
      knowlege to decide if the final state is correct.

A sequential execution would look like:

```
        // 1.  initialize the state.
        c->init(c);
        c->A(c);
        if(!c->B(c))
            panic("B should not fail\n");
        // 4. check that the state passes.
        if(!c->check(c))
            panic("check failed sequentially: code is broken\n");
```

If this does not pass the code is very broken and we complain.

Assuming a single run passes, we can can put this block of code in a
loop  (as our example code in `check:check-interleave.c` does)
: this is the first thing we 

and multiple times and should pass each time.  If it does one of
the routines might be non-deterministic, or `init()` might not reset
the state correctly.


If you look at `check:check-interleave.c`: this is the first thing we 
do in the code.

#### Single stepping on the armv6

Single-stepping on the ARMv6 cpu we use (arm1176) has two weird features:
  1. You won't get single-step faults unless running at user mode.
     Thus, we'll have to run A() at user level (the initial mode is
     SUPER).

  2. ARMv6 implements single-stepping by using a "mismatch fault" where
     you give a code address `addr` and the CPU will throw a mismatch
     fault when the pc is equal to an address that is not equal to `addr`.
     To single step A():

        - set the initial mismatch address to the first instruction of A.
        - start running at user level.
        - you'll get a fault: in the fault handler, get the address
          of the fault (the value of the pc register), set a mismatch on
          `pc`, and then jump back.
        - this will cause you to single-step through all the code.
    
     Note: the checked in code does this; you just need to modify   
     the base code.

#### Single stepping A()

The code in `check:check-interleave.c` gives an example of how
to run `A()` in single step mode (but without switching).  
This has several pieces:
  1. We need to install exception handlers:  the calls to `full_except_*`
     at the start of `check` do this.  This uses the code from 140e:
     you should be able to drop in your versions.
  2. We need to run A at user level.  The call to `run_A_at_userlevel`
     does this.  

  3. We need to set faults and handle them: `single_step_handler_full`
     does this.

What `run_A_at_userlevel` does:
  1. It creates a full register set for A that can be switched into
     using `switchto_cswitch`.
  
  2. The arm has 16 general purpose registers and the process status
     word.  We use the CPSR of the current execution and just swap
     the mode.  We set PC (r15) to the address of A().  We set the
     stack pointer register sp to the end of a big array (stack
     grows down).   

     One tricky bit is that we set the return address register (lr)
     to the address of `A_terminated`: we do this so when A is done
     and returns it jumps to this routine and we can switch back into
     the checker.

     A second tricky bit is that `A_terminated` cannot just switch
     back to the original code b/c it is running at USER level so lacks
     permissions to switch back to `SUPER` (our original privileged mode).
     Thus it invokes a system call `sys_switchto(&start_regs)` which will
     cause a system call exception, (running in privileged mode) that will
     evantually calling the handler we registered `syscall_handler_full`.

  3. It turns on mismatching (single stepping).
  4. It then context switches from the current code to
     A() using `switchto_cswitch` (this is given; you built it in 140E).

  5. When A() is done and calls `A_terminated()` it will switch back
     using `start_regs`, which will turn off mismatch and return.

What `single_step_handler_full` does:
  1. It is passed in the full register set live when the code was
     interrupted.
  2. It verifies this was a single step fault.
  3. It then gets the pc, prints out a simple message.
  4. Resets the mismatch for the pc (so just that address will run)
  5. does a `switchto` to jump back to the exception code.


-----------------------------------------------------------------------
### Part 0: turn off single-step when `A()` calls `A_terminated()`

If you compile and run the code you'll notice the single-step handler
prints a ton of instructions even though `A()` is tiny.  The reason for
this is that when A() exits, it jumps to `A_terminated()`.  A simple
hack here is to change the single step handler to compare the `pc`
value against `A_terminated()` and if equal, turn single stepping off.

The result of this is that the test output should do down dramatically.

Easiest approach: change the makefile to only run
`tests/0-epsilon-test.c`.

-----------------------------------------------------------------------
### Part 1: do a single switch from A() to B()

For this make sure your code handles all tests tests besides test 4.
Test 3 and 5 are reasonable; the others are trivial.  

-----------------------------------------------------------------------
### Part 2:  make a `sys_trylock()` for test 4.

For this you'll add a system call that implements the try-lock needed for
test `4-trylock-ok-test.c`.    This is an example of a common pattern
we will do in the checker: implement a concurrency primitive using a
system call so that we (1) don't interrupt it with single-stepping and
(2) have a complete context for the blocking thread so we can yield from
one to another if needed.

(Note, this isn't the only way we can acheive these two goals.)

The most general way to implement this is to 
run `B()` as a second thread.
Why:
  - Given how the exception trampoline code is written, we can't
    call system calls from privileged code.  (Note: you could rewrite
    your 140e `full-except.c` to do so if you want).

    Thus, if B() is allowed to call the trylock code (it is) we need to
    run B()'s code at user-level.  The most straightfoward way to do so
    is to run it as a second thread.  This will also let us implement a
    `yield` primitive (part 3, below) and switch multiple times.

  - Note: you could also achieve the same result by (1) by adding new
    system calls or (2) changing the code to check if its at user-level
    and, if not, call the function directly.  We don't do so b/c most
    of the extensions need two threads.

You can do so by:
 1. Adapt the the `run_A` code to create a second thread.  Note: b/c
    of armv6 restrictions make sure you are allocating the stack to be
    8-byte aligned.  You'll need to change the code so on exit it does
    the right thing for both the A and the B thread.

 2. Have a thread queue that you put the threads on and dequeue (as usual).
 3. Adapt your single step handler to use a `switchto` to the next thread
    rather than calling B directly.
 4. For some code you wo't be able to run the sequential check as-is, so 
    either disable it, or make sure your threads can handle "running 
    sequentially".

This test is pretty dumb, so you probably should write another one.

-----------------------------------------------------------------------
### Part 3:  make a `sys_yield()`.

For simplicity, our base checking system assumed it could call B() in
the exception handler and it would run to completion: i.e., it couldn't
yield back to `A()`  if the shared state was not ready (e.g., in the
case of a shared queue, if `B()` wanted to do a pop
and `A()` hadn't done a push yet).

Two lame consequences:
 1. Our base checker interface is a bit hacked in that it has
    B() return a failure if it can't make progress rather than yield
    and wait.
 2. As a result of (1) we check a dramatically smaller set of 
    interleavings than are possible --- e.g., we only run `B()`
    to completion when the shared state is completely setup.   This
    will cause us to miss errors.

So, fix this:
  1. Use the fact we have two threads (from part 2) to implement a new
     system call, `sys_yield()` so that B (and A) can yield to the
     other thread when it needs it additional work done before it can
     make progress.
  2. Write a test that shows your yield works as expected.
  3. Make copies and rewrite the tests so that they do yield rather 
     than have B() return an error.

-----------------------------------------------------------------------
### Part 4: add some interesting tests.

Easy mode: get some "lock-free" code from GPT or Claude and check it.
Should be able to find some bugs.

More interesting: check gcc's synchonization library code.  The cool thing
about single-step is that we work just as well on machine code. (Though
it's harder to debug!).

Other possible: 
 - Rewrite circular to use compiler memory barriers and look for bugs.
 - Extend the checker to look for bugs in interrupt handlers.

-----------------------------------------------------------------------
### Extensions

There's an enormous number of extensions:
  - Extend the code to do 2 switches, 3 switches, etc.

  - Make the error handling better.  E.g., on the unix side,
    use `addr2line` to convert the code addresses we ran on to line numbers.

  - Speed: to reduce the number of switches notice that A() can only
    effect B() if it does a store or synchronization instruction.  B()
    can only be effected it if does a load or synchonization instruction.
    If A() only writes to its copy of registers (e.g., an
    add, subtract, multiply) it can't affect B().  (And vice versa.)
    
    Thus we only need to switch on memory or synchornization operations.
    To do this you can decode the instruction that was interrupted and
    only switch if it was a memory or sync operation.  You already have
    some idea of how to decode instructions from the JIT labs: in our case
    we don't need to know the exact instruction, just that it does memory.

  - Speed: the number of paths grows exponentially with the number of
    switches.  The standard way to handle this is to hash the memory
    state and if you get to the same program point with the same state,
    prune execution.

    As a crude method you could side-step the need to know what memory
    locations are being read or written by hashing the register set for
    each thread and prune when the remaining context switches are  the
    same (or fewer) than a previous state.
