A toy multi-threading program, involving a match between stealers,
implemented using posix threads.

USAGE:
    ./staler [--stealer=n] [--moneyguy=n] [--punish-time=n] [--save-time=n]
             [--deposit-time=n]

    sane default are provided. --xxxx-time is measured in microsecond, so some number
    of order 10000 is proper.
