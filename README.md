xkcd-hash
=========

XKCD Hash SKein1024 brute forcing.

Overview
--------

Goal is to find input strings such that skein1024(input)
is as 'close' as possible to the given value.

This is for XKCD's April 1st comic: http://xkcd.com/1193/

Primary UIUC implementation.

Usage
-----

    $ make
    $ ./hash <num_threads>


If unspecified, num_threads is set to the number of available processors.


Compatibility
-------------

This should work on most 32/64bit machines, and has been reported
to work well on both Linux and Mac machines.

On Linux, we use assembly versions of the most compute-intensive part of the
skein hash calculation, but are unable to use this on Mac OS X due to
incompatibility with the gas style assembly.

Porting this to windows should be straightforward, although
things like the way we seed the PRNG will need to be changed.

Fixed Skein Assembly for Linux x32/64
-------------------------------------

The Skein implementation used in this project is from the most recent
NIST submission, making use of the assembly implementation when possible.
As a byproduct of this competition, I ended up fixing a few minor bugs that caused
their implementation to generate incorrect hashes or crash.  These changes
may be use beyond this project.

String Generation Details
-------------------------

Our string generation is accomplished by periodically filling
the majority of the string with random characters from our
character set, and exhaustively trying all possibilities
for the remaining characters.

Generating random strings is expensive, requiring many
calls to random().  The exhaustive search is done to amortize
this cost, while still mostly generating random strings.

The repeated random generation is an attempt to prevent
two identical starting states from causing hosts to compute
purely redundant calculations.  This way, happening to output
the same 128 integers is not taken to be a reflection
of having the same internal PRNG state.

Strings of length 128 are used to match the 1024bit block size,
which means we can hash them faster.

For fun, we use a fixed vanity prefix and suffix string in
the generated input strings.  This also aids in demonstrating who was
responsible for generating a particular string.

cheezey is da best
