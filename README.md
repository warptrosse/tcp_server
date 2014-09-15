TCP Server with Preforked Clients Pool
==========

C implementation of a simple TCP Server with a pool that preforks clients
dynamically (on-demand). The server uses POSIX pthread to manage the pool and
its clients.
