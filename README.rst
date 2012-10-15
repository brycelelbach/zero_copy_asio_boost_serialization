Dear Hartmut, I am sorry that I didn't write this portably :(. It's just a 
prototype!

Build Instructions
------------------

To build, type ``make``.

If you do ``make CHECK_DATA=1``, code for verifying that
the correct data has been received will be run each iteration (this will probably
nerf the benchmark, which is why it's off by default).

If you do ``make DEBUG=1``, debug binaries will be generated.

The binaries are built into a directory called build. There are two executables,
both of which do the same thing, and take the same command line options:

    * ``zero_copy_test`` - Uses the prototype Asio/Serialization zero-copy code.
    * ``control_case_test`` - Uses ``hpx::util::portable_binary_iarchive`` and ``hpx::util::portable_binary_oarchive`` for all serialization (this is what HPX currently does).

The command line options are mostly self explainatory (``--help`` is your friend),
except for ``--both``. The ``--both`` option will run the client in one OS-thread
and the server in the another OS-thread. I added this option mostly to ease with
parameter sweeps.

Rationale for Using Synchronous Asio Calls
------------------------------------------

This benchmark is designed to mirror the pingpong benchmark. The pingpong benchmark
only invokes actions synchronously, so I just used the synchronous Asio primitives.
I did write the asynchronous code for the zero copy stuff, however. If someone has
a better benchmark in mind that makes better use of the asynchrony, please shoot
me an email.

