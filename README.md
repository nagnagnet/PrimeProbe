## Prime+Probe is a last-level cache side-channel attack.

main.c is an attack code and key.c is a victim code.
The attacker knows the victim code and a platform information. (e.g. LLC structure)

note: Run main.c and key.c on different cores. (xx and yy are different cores)

    $ taskset -c xx ./key.o

    $ taskset -c yy ./main.o

To check the result, 

    $ python draw.py