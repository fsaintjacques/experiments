append-atomicity
================

Multiple internet sources diverges on `O_APPEND` writes to file being atomic. The POSIX standard states
that writes with size smaller or equal to `PIPE_BUF` (which is generaly 4k on Linux) are required to be
atomic.

According to `write(2)` on Linux:

> For a seekable file (i.e., one to which lseek(2) may be applied, for example, a regular file) writing
> takes place at the current file offset, and the file offset is incremented by the number of bytes actu‐
> ally written. If the file was open(2)ed with O_APPEND, the file offset is first set to the end of the
> file before writing. The adjustment of the file offset and the write operation are performed as an
> atomic step.
>
> POSIX requires that a read(2) which can be proved to occur after a write() has returned returns the new
> data. Note that not all filesystems are POSIX conforming.
>
> ...
>
> According to POSIX.1-2008/SUSv4 Section XSI 2.9.7 ("Thread Interactions with Regular File Operations"):
>
>     All of the following functions shall be atomic with respect to each other in the effects specified in
>     POSIX.1-2008 when they operate on regular files or symbolic links: ...
>
> Among the APIs subsequently listed are write() and writev(2). And among the effects that should be
> atomic across threads (and processes) are updates of the file offset. However, on Linux before version
> 3.14, this was not the case: if two processes that share an open file description (see open(2)) perform a
> write() (or writev(2)) at the same time, then the I/O operations were not atomic with respect updating
> the file offset, with the result that the blocks of data output by the two processes might (incorrectly)
> overlap. This problem was fixed in Linux 3.14.

From my understanding:

1. 2 processes/threads writting to the same file in append mode should not
   overlap.
2. While the previous statement is usefull, it does not guarantee that the
   number of written bytes is equal to the requested number of bytes to write.
3. It depends on the filesystem used.

experiment
==========

This experiment is divided in 2 files:

- `will-it-blend <n_threads> <n_runs> <n_bytes> <test_file>`: is a process
  that executes `<n_threads>` concurrent threads that will try to write
  `n_runs` blocks of `<n_bytes>` bytes to the requested file `<test_file>`.

- `validate` will run concurrent `will-it-blend` processes that appends to the
  same file an verify that the generated file is consistent (no overwrites).

While this expirement is subject to errors, it can give you an idea on how your
Linux & the file system behave.
