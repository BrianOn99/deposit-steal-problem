A toy multi-threading program, involving a match between stealers,
implemented using posix threads.

USAGE:
======
    ./staler [--stealer=n] [--moneyguy=n] [--punish-time=n] [--save-time=n]
             [--deposit-time=n]

    sane default are provided. --xxxx-time is measured in microsecond, so some number
    of order 10000 is proper.

EXPLAINATION:
=============
There are some ATM cracked by aome stealer. some guy are crazily depositing
money into the account.
With some reason, the ATM cannot tell how much $ left. The stealer each own 1
ATM, and the have to decide a fixed amount to retrieve from the machine for a
maximum number of frial. If the money left is not enough, he canot get a
mosquito.
The crazy money giver goto sleep when the ATM is full of money. The stealer
wake them up after stealing.

It seems that a higher amount to steal may not be good if the guys have to
spend a lot od\f time to deposit money (the account is always empty)

You should get something like this in the end:

stealer 1 stolen 97
stealer 2 stolen 188
stealer 3 stolen 267
stealer 4 stolen 336
stealer 5 stolen 410

TODO:
=====
allow different number of stealer and moneyguy


