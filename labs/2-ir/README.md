### Overview: reverse engineering a infrared remote control.

<p align="center">
  <img src="images/ir.png" width="350" />
</p>


Today is a fun reverse engineering lab.   
  1. Using an IR sensor --- [the Vishay TSOP4838](https://www.newark.com/webapp/wcs/stores/servlet/ProductDisplay?catalogId=15003&productSeoURL=vishay&partNumber=60K6999) --- you'll reverse engineer the key presses from a remote.  It's a fun puzzle that doesn't need much infrastructure.  Many projects you do get better if you can add remote control, so it's also useful.

  2. You'll then use an LED to transmit values the IR receiver can read.
     You'll use this to make a simple network that implements the UART
     protocol to send bytes.  Open ended, illuminating puzzles: (1)
     how fast can you send UART characters, (2) make a network bootloader.
    
     Loop-back is an interesting example: it's extremely sensitive to mistakes,
     which keeps you honest, and can be used as a simple "hello world" to 
     show the tradeoffs of the different ways to structure systems: interrupt
     based, threads, events.  We will likely do this on thursday.

As with the last lab, it should be easy to ignore all of our code if
you want, and just do everything from scratch ("Daniel mode").


Where stuff is:

  - The IR receiver datasheet is in `docs/tsop4838.pdf`.
    You should read this to make sure (1) you know which pins to connect
    to and (2) what min and max power they expect.

  - The IR led datasheet (for part 2) is `docs/tsal4400.pdf`.  The main
    rule: we do not use a resistor and the LED is tiny, so never leave
    it on for long, and never ever plug directly into 3v.  (Definitely
    not 5v.)

  - The code is in `code/ir-tsop.c`.

  - We have remotes.  However, if
    you have your phone, you should be able to download a "universal
    remote" app and use that.  A cool side-effect is having the
    infrastructure to control your pi from your phone.

    Note: many (most?) remote apps suck, so it makes sense to poke around
    on reddit or stackoverflow for a simple one.



*The single most common major mistake*:

  - Do not print values *while* recording times, since doing so will
    mess up your timings.  Instead when the IR has been low for "a long
    time", print all the timings you've seen.

First steps:
  1. Write the code to read from the input pin.  

  2. Remotes communicate by sending values for a speicific amount of
     time.  So to start off record the value sent and the time it was sent.

  3. A key press transition is typically signaled with a "very long"
     on-value.	Then a set of timed off/on pulses.    You should be
     able to see this pattern in the values you print and it should be
     "somewhat" stable.


Background: If you haven't had 140e: you should go through the labs:
  - [GPIO](https://github.com/dddrrreee/cs140e-24win/tree/main/labs/3-gpio)
  - [Interrupts](https://github.com/dddrrreee/cs140e-24win/tree/main/labs/5-interrupts)
  - [Threads](https://github.com/dddrrreee/cs140e-24win/tree/main/labs/6-threads)
     

### Checkoff

Checkoff:
  1. You should pass the few tests.
  2. You should show that pressing four keys on your remote causes your
     pi to print the key value.

  3. It's not a heavyweight lab: if you finish, do an extension or 
     start on lab 3!

--------------------------------------------------------------------
### Background: how an IR remote sends values

Optional background reading:
 - [A nice clear writeup of IR protocol](http://irq5.io/2012/07/27/infrared-remote-control-protocols-part-1/)

For the remotes we use they send a 0 or 1 bit by sending 1 for differing
amounts of time.  They seperate the bits by sending 0 for a fixed amount
of time.     A specific remote button will have a unique, consistent
integer formed by contatentating these bits (as with the uart protocol).
The bits for a given command are prefixed by a header that lasts much
longer a single bit.  It ends by going off for a long amount of time.

The timings for the specific remote I used:

        Signal      off (usec)    on (usec)
         HEADER      9000          4500
         0 bit        600           600
         1 bit        600           1600
         STOP         600           --

        default signal: 1.

        It also looks like they indicate the start of a transmission by
        sending 1 for about 40,000 usec and then a header.

So to send 01 they would:
    0. Send a header: 0 for about 9000 usec, then 1 for about 4500 usec.

    1. Send a skip (0 for 600 usec).
    2. Send a 0 (1 for 600 usec);
    3. Send a skip (0 for 600 usec);
    4. Send a 1 (1 for 1600usec).
    5. (maybe) send a skip (0 for 600usec).
    6. send nothing for a "long" time.


So given this information you should be able to reverse engineer the
value for each key.
   1. pick up the header.
   2. read the values.
   3. convert the values to 0 and 1, shifting them into an unsigned


A simple-minded algorithm:
  1. spin waiting until you read 0 from the IR.
  2. if we get a header (IR=0 for 9000, IR=1 for 4500)
     - then read bits until we timeout (set this longer than 
       any legal value -- I used 20000).
  3. reconstruct the signal by converting IR readings to a 1 or 0
     as follows:
    - 0 = IR=0 for 600us, IR=1 for 600usec.
    - 1 = IR=0 for 600us, IR=1 for 1600usec.

    For each bit: shift it into an unsigned (similar to UART!) and return
    when done.  The order doesn't matter since it's just an internal
    integer.

As with UART, there can be error so your code has to handle it.

  1. I accepted readings as a valid header if they
     were within 10% of the expected value.  So: a first reading IR=0
     for 9000 +/- 900usec, followed by IR=1 for 4500 +/- 450usec.

  2. When deciding if a value is a skip, return true if it's within
     a given number of usec (e.g., if its between 400 to 800usec).


  3. When deciding if transmission was a 1 or a 0, pick the value by
     deciding if its above or below the halfway point between the two
     values (e.g., if the timing is above (1600+600)/2 then its a 1 and
     is a 0 otherwise).  This was fairly robust and though sleazy (since
     it won't reject anything) is close to what alot of hardware does
     (according to Mark).

   4. If you are more clever than this somewhat brain-dead approach,
      be more clever!


Notes:
  - Again, don't print while reading or you'll get garbage timings.
  - Some remotes send different values for "long" or "short" keypresses.
    You'll have to experiment.

--------------------------------------------------------------------
### What to do:

Go through `tsop322.c` and start implementing the code.  It's all in one file
to make it easier to see what is going on.
  1. If you scan through for `unimplemented` you will see the parts to implement.
  2. Start with `get_readings` which will need a modified version of your timeout
     from last time.
  3. Then implement `convert` to produce a 32-bit value from a set of readings.
  4. Then figure out what value each key on your remote gives and implement
     `key_to_str`.
  5. There are some helper functions that hopefully are self explanatory.

----------------------------------------------------------------
#  Part 1: reverse engineering key presses.

Given the protocol described above, you should be able to reverse engineer the
value `v` for each key.

   0. Set at 32-bit unsigned variable `v` to 0.
   1. Wait until you see a start bit (sort of similar to the UART protocol you built).
   2. For each on pulse, record its bit position, and convert it to a 0 or 1
      (depending on its length, as above).
   3. For any 1 bit in (2) set the bit at that position in `v`.
   4. When your code gets a timeout, print `v` at the end.
   5. If you hit the same remote key over and over, you should get the same value.

The main issue is handling error.  The easiest thing is to decide based
on whether its above or below the midpoint value between the two.

NOTES:
   1. Use a `gpio_pullup` to pull the input pin high when nothing is happening.
   2. As we found in the early timing labs, if you print something
      while you are waiting for a time-based signal, the print will
      likely take long enough that you miss it or corrupt the timing.

   3. As mentioned before, since the timer on the pi is a different
      device than GPIO, you'd have to use a memory barrier to synchronize
      them.  To avoid this, you might want to use the pi's cycle counter
      (which doesn't need a memory barrier).  Of course, this can overflow
      pretty quickly, so make sure your code does not get confused.

----------------------------------------------------------------
###  Extension: smoothly control a light.

If you have a remote control, you'd probably want it to control something.
So for this extension, do so.

The trick here is that if you look at the TSOP code it has a bad tendency
to block everything else while it spins in a loop waiting for input.
This didn't matter for our keypress printing since printing wasn't
timing sensitive.  However, even something as simple as PWM-ing an LED
can go awry without care:

  1. You can't sit in a loop waiting for the complete IR signal b/c     
     you won't be turning the LED on-off.
  2. You can't blindly sit turning an LED on-off b/c then you may well
     be ignore IR input.

There's a variety of ways to solve this.  Most people spastically grab
one (e.g., interrupts) not thinking about how it exponentially explodes 
the number of paths in code, essentially making it untestable.

A few different options:

  1. Have the IR code run in a loop, and the PWM code run in a timer
     interrupt handler.  Easy to describe.   Not that hard to setup.
     Downside: can't test the code thoroughly, you do need the interrupt
     to run fast enough or you can miss IR input.

  2. Use hardware to PWM --- you can view this as having a second thread
     that happens to run on a hardware device.  The pi has a PWM 
     device that allows this.  Downside: datasheet is not great.

  3. Use cooperative threads: Each time the IR or the LED does a delay,
     yield to the other thread.  Advantages: less paths than the interrupt
     code.  The thread context switching cost (and time between yields)
     is a hard limit on how fine your accuracy can be.

  4. Use a run-to-completion thread (a function call with state) for
     the PWM code.  This routine is called whenever the IR code does
     a delay (i.e., in each place it would call yield for threads).
     The IR code just spins in a loop.  Downside: not very flexible.
     However, it uses the least amount of machinery (no assembly, no
     interrupts, no datasheets other than for IR).  I have a soft
     spot for doing this kind of thing since it's basically one
     big sequential program.

No matter what you do, you should record the time between when your
code runs and flag if it ever exceeeds some threadshold.

----------------------------------------------------------------
# Extension: IR interrupts.

In general, if we use remotes its to very-occasionally control a device
that is pretty busy doing something else.  In this case, it's natural
to use interrupts (note: this is hard if we care about very fine timing;
there are other approaches).

If you need a refresher:
  - lab 8 in cs140e goes over GPIO interrupts.
  - I checked in a staff version `staff-objs/gpio-int.o` that you can link in
    (by modifying `put-your-src-here.mk`) and use.
  - If you have it, you can also use your own code.

The relevant functions:

    // include/gpio.h

    // p97 set to detect rising edge (0->1) on <pin>.
    // as the broadcom doc states, it  detects by sampling based on the clock.
    // it looks for "011" (low, hi, hi) to suppress noise.  i.e., its triggered only
    // *after* a 1 reading has been sampled twice, so there will be delay.
    // if you want lower latency, you should us async rising edge (p99)
    void gpio_int_rising_edge(unsigned pin);

    // p98: detect falling edge (1->0).  sampled using the system clock.
    // similarly to rising edge detection, it suppresses noise by looking for
    // "100" --- i.e., is triggered after two readings of "0" and so the
    // interrupt is delayed two clock cycles.   if you want  lower latency,
    // you should use async falling edge. (p99)
    void gpio_int_falling_edge(unsigned pin);

    // p96: a 1<<pin is set in EVENT_DETECT if <pin> triggered an interrupt.
    // if you configure multiple events to lead to interrupts, you will have to
    // read the pin to determine which caused it.
    int gpio_event_detected(unsigned pin);

    // p96: have to write a 1 to the pin to clear the event.
    void gpio_event_clear(unsigned pin);

Simple strategy:
   1. enable rising edge (from 0 to 1).
   2. In the interrupt handler, just process the entire remote transmission.
   3. This locks up the pi during this, but in the case that you are going to
      do a major phase shift, this is ok.

A possibly better strategy:
   1. Detect each edge and put the result in a queue.
   2. When you detect a stop, convert the edges to a value.
   3. This is more complicated, but lets you spend not-much time in the handler.

